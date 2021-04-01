#include "pixelsim.hpp"
#include "geometry.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "pixelsim_impl.hpp"
#include "utils.hpp"
#include <QElapsedTimer>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <utility>
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
#include <tbb/global_control.h>
#include <tbb/task_scheduler_init.h>
#endif
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#include <omp.h>
#endif

namespace sme {

namespace simulate {

void PixelSim::calculateDcdt() {
  // calculate dcd/dt in all compartments
  for (auto &sim : simCompartments) {
    if (useTBB) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
      sim->evaluateReactions_tbb();
      sim->evaluateDiffusionOperator_tbb();
#endif
    } else {
      sim->evaluateReactions();
      sim->evaluateDiffusionOperator();
    }
  }
  // membrane contribution to dc/dt
  for (auto &sim : simMembranes) {
    sim->evaluateReactions();
  }
  for (auto &sim : simCompartments) {
    sim->spatiallyAverageDcdt();
  }
}

void PixelSim::doRK101(double dt) {
  // RK1(0)1: Forwards Euler, no error estimate
  calculateDcdt();
  for (auto &sim : simCompartments) {
    if (useTBB) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
      sim->doForwardsEulerTimestep_tbb(dt);
#endif
    } else {
      sim->doForwardsEulerTimestep(dt);
    }
  }
}

void PixelSim::doRK212(double dt) {
  // RK2(1)2: Heun / Modified Euler, with embedded forwards Euler error
  // estimate Shu-Osher form used here taken from eq(2.15) of
  // https://doi.org/10.1016/0021-9991(88)90177-5
  calculateDcdt();
  for (auto &sim : simCompartments) {
    if (useTBB) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
      sim->doRK212Substep1_tbb(dt);
#endif
    } else {
      sim->doRK212Substep1(dt);
    }
  }
  calculateDcdt();
  for (auto &sim : simCompartments) {
    if (useTBB) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
      sim->doRK212Substep2_tbb(dt);
#endif
    } else {
      sim->doRK212Substep2(dt);
    }
  }
}

void PixelSim::doRK323(double dt) {
  // RK3(2)3: Shu Osher method with embedded Heun error estimate
  // Taken from eq(2.18) of
  // https://doi.org/10.1016/0021-9991(88)90177-5
  constexpr std::array<double, 3> g1{1.0, 0.25, 0.666666666666666666666};
  constexpr std::array<double, 3> g2{0.0, 0.0, 0.0};
  constexpr std::array<double, 3> g3{0.0, 0.75, 0.333333333333333333333};
  constexpr std::array<double, 3> beta{1.0, 0.25, 0.6666666666666666666};
  constexpr std::array<double, 3> delta{0.0, 0.0, 1.0};
  for (auto &sim : simCompartments) {
    sim->doRKInit();
  }
  for (std::size_t i = 0; i < 3; ++i) {
    doRKSubstep(dt, g1[i], g2[i], g3[i], beta[i], delta[i]);
  }
  for (auto &sim : simCompartments) {
    sim->doRKFinalise(0.0, 2.0, -1.0);
  }
}

void PixelSim::doRK435(double dt) {
  // RK4(3)5: 3S* algorithm 6 (see also table 6)
  // https://doi.org/10.1016/j.jcp.2009.11.006
  // 5 stage RK4 with embedded RK3 error estimate
  constexpr std::array<double, 5> g1{0.0, -0.497531095840104, 1.010070514199942,
                                     -3.196559004608766, 1.717835630267259};
  constexpr std::array<double, 5> g2{1.0, 1.384996869124138, 3.878155713328178,
                                     -2.324512951813145, -0.514633322274467};
  constexpr std::array<double, 5> g3{0.0, 0.0, 0.0, 1.642598936063715,
                                     0.188295940828347};
  constexpr std::array<double, 5> beta{0.075152045700771, 0.211361016946069,
                                       1.100713347634329, 0.728537814675568,
                                       0.393172889823198};
  constexpr std::array<double, 7> delta{1.0,
                                        0.081252332929194,
                                        -1.083849060586449,
                                        -1.096110881845602,
                                        2.859440022030827,
                                        -0.655568367959557,
                                        -0.194421504490852};
  double deltaSum = 1.0 / utils::sum(delta);
  for (auto &sim : simCompartments) {
    sim->doRKInit();
  }
  for (std::size_t i = 0; i < 5; ++i) {
    doRKSubstep(dt, g1[i], g2[i], g3[i], beta[i], delta[i]);
  }
  for (auto &sim : simCompartments) {
    sim->doRKFinalise(deltaSum * delta[5], deltaSum, deltaSum * delta[6]);
  }
}

void PixelSim::doRKSubstep(double dt, double g1, double g2, double g3,
                           double beta, double delta) {
  calculateDcdt();
  for (auto &sim : simCompartments) {
    if (useTBB) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
      sim->doRKSubstep_tbb(dt, g1, g2, g3, beta, delta);
#endif
    } else {
      sim->doRKSubstep(dt, g1, g2, g3, beta, delta);
    }
  }
}

static double getErrorPower(PixelIntegratorType integrator) {
  double errPower{1.0};
  if (integrator == PixelIntegratorType::RK212) {
    errPower = 1.0 / 2.0;
  } else if (integrator == PixelIntegratorType::RK323) {
    errPower = 1.0 / 3.0;
  } else if (integrator == PixelIntegratorType::RK435) {
    errPower = 1.0 / 4.0;
  }
  return errPower;
}

double PixelSim::doRKAdaptive(double dtMax) {
  // Adaptive timestep Runge-Kutta
  PixelIntegratorError err;
  double dt;
  double errPower = getErrorPower(integrator);
  do {
    // do timestep
    dt = std::min(nextTimestep, dtMax);
    if (integrator == PixelIntegratorType::RK212) {
      doRK212(dt);
    } else if (integrator == PixelIntegratorType::RK323) {
      doRK323(dt);
    } else if (integrator == PixelIntegratorType::RK435) {
      doRK435(dt);
    }
    // calculate error
    err.abs = 0;
    err.rel = 0;
    for (const auto &sim : simCompartments) {
      auto compErr = sim->calculateRKError(epsilon);
      err.rel = std::max(err.rel, compErr.rel);
      err.abs = std::max(err.abs, compErr.abs);
    }
    // calculate new timestep
    double errFactor = std::min(errMax.abs / err.abs, errMax.rel / err.rel);
    errFactor = std::pow(errFactor, errPower);
    nextTimestep = std::min(0.95 * dt * errFactor, dtMax);
    SPDLOG_TRACE("dt = {} gave rel err = {}, abs err = {} -> new dt = {}", dt,
                 err.rel, err.abs, nextTimestep);
    if (nextTimestep / dtMax < 1e-20) {
      currentErrorImage = {};
      std::string problemSpecies{"unknown"};
      for (const auto &sim : simCompartments) {
        auto speciesName{sim->plotRKError(currentErrorImage, epsilon, err.rel)};
        if (!speciesName.empty()) {
          problemSpecies = speciesName;
        }
      }
      currentErrorMessage = fmt::format(
          "Failed to solve model to required accuracy. The largest "
          "relative integration error comes from species '{}'. The location "
          "of the pixels with the largest relative integration error are shown "
          "below in red:",
          problemSpecies);
      return nextTimestep;
    }
    if (err.abs > errMax.abs || err.rel > errMax.rel) {
      SPDLOG_TRACE("discarding step");
      ++discardedSteps;
      for (auto &sim : simCompartments) {
        sim->undoRKStep();
      }
    }
  } while (err.abs > errMax.abs || err.rel > errMax.rel);
  return dt;
}

PixelSim::PixelSim(
    const model::Model &sbmlDoc, const std::vector<std::string> &compartmentIds,
    const std::vector<std::vector<std::string>> &compartmentSpeciesIds,
    const PixelOptions &options)
    : doc{sbmlDoc}, integrator{options.integrator}, errMax{options.maxErr},
      maxTimestep{options.maxTimestep}, numMaxThreads{options.maxThreads} {
  try {
    // check if reactions explicitly depend on time or space
    auto xId{doc.getParameters().getSpatialCoordinates().x.id};
    auto yId{doc.getParameters().getSpatialCoordinates().y.id};
    bool timeDependent{doc.getReactions().dependOnVariable("time")};
    if (timeDependent) {
      ++nExtraVars;
    }
    bool spaceDependent{doc.getReactions().dependOnVariable(xId.c_str()) ||
                        doc.getReactions().dependOnVariable(yId.c_str())};
    if (spaceDependent) {
      nExtraVars += 2;
    }
    // add compartments
    for (std::size_t compIndex = 0; compIndex < compartmentIds.size();
         ++compIndex) {
      SPDLOG_ERROR("COMP: {}", compartmentIds[compIndex]);
      const auto &speciesIds{compartmentSpeciesIds[compIndex]};
      const auto *compartment{doc.getCompartments().getCompartment(
          compartmentIds[compIndex].c_str())};
      simCompartments.push_back(std::make_unique<SimCompartment>(
          doc, compartment, speciesIds, options.doCSE, options.optLevel,
          timeDependent, spaceDependent));
      maxStableTimestep = std::min(
          maxStableTimestep, simCompartments.back()->getMaxStableTimestep());
    }
    // add membranes
    for (const auto &membrane : doc.getMembranes().getMembranes()) {
      if (auto reacsInMembrane =
              doc.getReactions().getIds(membrane.getId().c_str());
          !reacsInMembrane.isEmpty()) {
        // look for the two membrane compartments in simCompartments
        std::string compIdA = membrane.getCompartmentA()->getId();
        std::string compIdB = membrane.getCompartmentB()->getId();
        auto iterA =
            std::find_if(simCompartments.begin(), simCompartments.end(),
                         [&compIdA](const auto &c) {
                           return c->getCompartmentId() == compIdA;
                         });
        auto iterB =
            std::find_if(simCompartments.begin(), simCompartments.end(),
                         [&compIdB](const auto &c) {
                           return c->getCompartmentId() == compIdB;
                         });
        SimCompartment *compA{nullptr};
        if (iterA != simCompartments.cend()) {
          compA = iterA->get();
        }
        SimCompartment *compB{nullptr};
        if (iterB != simCompartments.cend()) {
          compB = iterB->get();
        }
        simMembranes.push_back(std::make_unique<SimMembrane>(
            doc, &membrane, compA, compB, options.doCSE, options.optLevel,
            timeDependent, spaceDependent));
      }
    }
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
    if (options.enableMultiThreading) {
      useTBB = true;
    }
    if (numMaxThreads == 0) {
      // 0 means use all available threads
      numMaxThreads = static_cast<std::size_t>(
          tbb::task_scheduler_init::default_num_threads());
    }
#elif defined(SPATIAL_MODEL_EDITOR_WITH_OPENMP)
    if (!options.enableMultiThreading) {
      numMaxThreads = 1;
    }
    if (auto ompMaxThreads{static_cast<std::size_t>(omp_get_num_procs())};
        numMaxThreads == 0 || numMaxThreads > ompMaxThreads) {
      // 0 means use all available threads
      numMaxThreads = ompMaxThreads;
    }
    omp_set_num_threads(static_cast<int>(numMaxThreads));
#else
    if (options.enableMultiThreading) {
      SPDLOG_WARN(
          "Multithreading requested but not compiled with TBB or OpenMP "
          "support: ignoring");
    }
#endif
  } catch (const std::runtime_error &e) {
    SPDLOG_ERROR("runtime_error: {}", e.what());
    currentErrorMessage = e.what();
  }
}

PixelSim::~PixelSim() = default;

std::size_t PixelSim::run(double time, double timeout_ms) {
  SPDLOG_TRACE("  - max rel local err {}", errMax.rel);
  SPDLOG_TRACE("  - max abs local err {}", errMax.abs);
  SPDLOG_TRACE("  - max stepsize {}", maxTimestep);
#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
  tbb::global_control control(tbb::global_control::max_allowed_parallelism,
                              numMaxThreads);
#endif
  QElapsedTimer timer;
  timer.start();
  double tNow = 0;
  std::size_t steps = 0;
  discardedSteps = 0;
  // do timesteps until we reach t
  constexpr double relativeTolerance = 1e-12;
  while (tNow + time * relativeTolerance < time) {
    double maxDt = std::min(maxTimestep, time - tNow);
    if (integrator == PixelIntegratorType::RK101) {
      double timestep = std::min(maxDt, maxStableTimestep);
      doRK101(timestep);
      tNow += timestep;
    } else {
      tNow += doRKAdaptive(maxDt);
      if (!currentErrorMessage.empty()) {
        return steps;
      }
    }
    ++steps;
    if (timeout_ms >= 0.0 &&
        static_cast<double>(timer.elapsed()) >= timeout_ms) {
      SPDLOG_DEBUG("Simulation timeout: requesting stop");
      requestStop();
    }
    if (stopRequested.load()) {
      currentErrorMessage = "Simulation stopped early";
      SPDLOG_DEBUG("Simulation timeout or stopped early");
      return steps;
    }
  }
  SPDLOG_DEBUG("t={} integrated using {} steps ({:3.1f}% discarded)", time,
               steps + discardedSteps,
               static_cast<double>(100 * discardedSteps) /
                   static_cast<double>(steps + discardedSteps));
  return steps;
}

const std::vector<double> &
PixelSim::getConcentrations(std::size_t compartmentIndex) const {
  return simCompartments[compartmentIndex]->getConcentrations();
}

std::size_t PixelSim::getConcentrationPadding() const { return nExtraVars; }

const std::vector<double> &
PixelSim::getDcdt(std::size_t compartmentIndex) const {
  return simCompartments[compartmentIndex]->getDcdt();
}

double PixelSim::getLowerOrderConcentration(std::size_t compartmentIndex,
                                            std::size_t speciesIndex,
                                            std::size_t pixelIndex) const {
  return simCompartments[compartmentIndex]->getLowerOrderConcentration(
      speciesIndex, pixelIndex);
}

const std::string &PixelSim::errorMessage() const {
  return currentErrorMessage;
}

const QImage &PixelSim::errorImage() const { return currentErrorImage; }

void PixelSim::requestStop() { stopRequested.store(true); }

} // namespace simulate

} // namespace sme

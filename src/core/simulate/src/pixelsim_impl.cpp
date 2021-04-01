#include "pixelsim_impl.hpp"
#include "geometry.hpp"
#include "logger.hpp"
#include "model.hpp"
#include "pde.hpp"
#include "utils.hpp"
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
#include <tbb/parallel_for.h>
#include <tbb/task_scheduler_init.h>
#include <tbb/tick_count.h>
#endif

namespace sme {

namespace simulate {

ReacEval::ReacEval(const model::Model &doc,
                   const std::vector<std::string> &speciesIDs,
                   const std::vector<std::string> &reactionIDs,
                   double reactionScaleFactor, bool doCSE, unsigned optLevel,
                   bool timeDependent, bool spaceDependent) {
  // construct reaction expressions and stoich matrix
  PdeScaleFactors pdeScaleFactors;
  pdeScaleFactors.reaction = reactionScaleFactor;
  std::vector<std::string> extraVars{};
  if (timeDependent) {
    SPDLOG_TRACE("model reactions depend on time");
    extraVars.push_back("time");
  }
  if (spaceDependent) {
    SPDLOG_TRACE("model reactions depend on space");
    extraVars.push_back(doc.getParameters().getSpatialCoordinates().x.id);
    extraVars.push_back(doc.getParameters().getSpatialCoordinates().y.id);
  }
  Pde pde(&doc, speciesIDs, reactionIDs, {}, pdeScaleFactors, extraVars);
  // add dt/dt = 1 reaction term, and t,x,y "species"
  auto sIds{speciesIDs};
  sIds.insert(sIds.end(), extraVars.cbegin(), extraVars.cend());
  auto rhs{pde.getRHS()};
  if (timeDependent) {
    rhs.push_back("1"); // dt/dt = 1
  }
  if (spaceDependent) {
    rhs.push_back("0"); // dx/dt = 0
    rhs.push_back("0"); // dy/dt = 0
  }
  // compile all expressions with symengine
  sym = utils::Symbolic(rhs, sIds, {}, {}, true, doCSE, optLevel);
}

void ReacEval::evaluate(double *output, const double *input) const {
  sym.eval(output, input);
}

void SimCompartment::spatiallyAverageDcdt() {
  // for any non-spatial species: spatially average dc/dt:
  // roughly equivalent to infinite rate of diffusion
  for (std::size_t is : nonSpatialSpeciesIndices) {
    double av = 0;
    for (std::size_t ix = 0; ix < nPixels; ++ix) {
      av += dcdt[ix * nSpecies + is];
    }
    av /= static_cast<double>(nPixels);
    for (std::size_t ix = 0; ix < nPixels; ++ix) {
      dcdt[ix * nSpecies + is] = av;
    }
  }
}

SimCompartment::SimCompartment(const model::Model &doc,
                               const geometry::Compartment *compartment,
                               std::vector<std::string> sIds, bool doCSE,
                               unsigned optLevel, bool timeDependent,
                               bool spaceDependent)
    : comp{compartment}, nPixels{compartment->nPixels()}, nSpecies{sIds.size()},
      compartmentId{compartment->getId()}, speciesIds{std::move(sIds)} {
  // get species in compartment
  speciesNames.reserve(nSpecies);
  SPDLOG_DEBUG("compartment: {}", compartmentId);
  std::vector<const geometry::Field *> fields;
  for (const auto &s : speciesIds) {
    const auto *field = doc.getSpecies().getField(s.c_str());
    double pixelWidth = comp->getPixelWidth();
    SPDLOG_ERROR("{:x}", reinterpret_cast<std::size_t>(static_cast<const void*>(comp)));
    SPDLOG_ERROR("d {}, a {}", field->getDiffusionConstant(), pixelWidth);
    diffConstants.push_back(field->getDiffusionConstant() / pixelWidth /
                            pixelWidth);
    // forwards euler stability bound: dt < a^2/4D
    maxStableTimestep =
        std::min(maxStableTimestep, 1.0 / (4.0 * diffConstants.back()));
    fields.push_back(field);
    speciesNames.push_back(doc.getSpecies().getName(s.c_str()).toStdString());
    if (!field->getIsSpatial()) {
      nonSpatialSpeciesIndices.push_back(fields.size() - 1);
    }
    SPDLOG_INFO("  - adding species: {}, diff constant {}", s,
                 diffConstants.back());
  }
  // get reactions in compartment
  std::vector<std::string> reactionIDs;
  if (auto reacsInCompartment =
          doc.getReactions().getIds(compartmentId.c_str());
      !reacsInCompartment.isEmpty()) {
    reactionIDs = utils::toStdString(reacsInCompartment);
  }
  reacEval = ReacEval(doc, speciesIds, reactionIDs, 1.0, doCSE, optLevel,
                      timeDependent, spaceDependent);
  if (timeDependent) {
    speciesIds.push_back("time");
    diffConstants.push_back(0);
    ++nSpecies;
  }
  if (spaceDependent) {
    speciesIds.push_back(doc.getParameters().getSpatialCoordinates().x.id);
    diffConstants.push_back(0);
    speciesIds.push_back(doc.getParameters().getSpatialCoordinates().y.id);
    diffConstants.push_back(0);
    nSpecies += 2;
  }
  // setup concentrations vector with initial values
  conc.resize(nSpecies * nPixels);
  dcdt.resize(conc.size(), 0.0);
  double width{doc.getGeometry().getPixelWidth()};
  auto origin{doc.getGeometry().getPhysicalOrigin()};
  auto concIter = conc.begin();
  for (std::size_t ix = 0; ix < compartment->nPixels(); ++ix) {
    for (const auto *field : fields) {
      *concIter = field->getConcentration()[ix];
      ++concIter;
    }
    if (timeDependent) {
      *concIter = 0; // t
      ++concIter;
    }
    if (spaceDependent) {
      auto pixel{compartment->getPixel(ix)};
      // pixels have y=0 in top-left, convert to bottom-left:
      pixel.ry() = compartment->getCompartmentImage().height() - 1 - pixel.y();
      *concIter = origin.x() + static_cast<double>(pixel.x()) * width; // x
      ++concIter;
      *concIter = origin.y() + static_cast<double>(pixel.y()) * width; // y
      ++concIter;
    }
  }
  assert(concIter == conc.end());
}

void SimCompartment::evaluateDiffusionOperator(std::size_t begin,
                                               std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    std::size_t ix = i * nSpecies;
    std::size_t ix_upx = comp->up_x(i) * nSpecies;
    std::size_t ix_dnx = comp->dn_x(i) * nSpecies;
    std::size_t ix_upy = comp->up_y(i) * nSpecies;
    std::size_t ix_dny = comp->dn_y(i) * nSpecies;
    for (std::size_t is = 0; is < nSpecies; ++is) {
      dcdt[ix + is] +=
          diffConstants[is] *
          (conc[ix_upx + is] + conc[ix_dnx + is] + conc[ix_upy + is] +
           conc[ix_dny + is] - 4.0 * conc[ix + is]);
    }
  }
}

void SimCompartment::evaluateDiffusionOperator() {
  evaluateDiffusionOperator(0, nPixels);
}

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::evaluateDiffusionOperator_tbb() {
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, nPixels),
                    [this](const tbb::blocked_range<std::size_t> &r) {
                      evaluateDiffusionOperator(r.begin(), r.end());
                    });
}
#endif

void SimCompartment::evaluateReactions(std::size_t begin, std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    reacEval.evaluate(dcdt.data() + i * nSpecies, conc.data() + i * nSpecies);
  }
}

void SimCompartment::evaluateReactions() { evaluateReactions(0, nPixels); }

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::evaluateReactions_tbb() {
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, nPixels),
                    [this](const tbb::blocked_range<std::size_t> &r) {
                      evaluateReactions(r.begin(), r.end());
                    });
}
#endif

void SimCompartment::doForwardsEulerTimestep(double dt, std::size_t begin,
                                             std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    conc[i] += dt * dcdt[i];
  }
}

void SimCompartment::doForwardsEulerTimestep(double dt) {
  doForwardsEulerTimestep(dt, 0, conc.size());
}

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::doForwardsEulerTimestep_tbb(double dt) {
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, conc.size()),
                    [this, dt](const tbb::blocked_range<std::size_t> &r) {
                      doForwardsEulerTimestep(dt, r.begin(), r.end());
                    });
}
#endif

void SimCompartment::doRKInit() {
  s2.assign(conc.size(), 0.0);
  s3 = conc;
}

void SimCompartment::doRK212Substep1(double dt, std::size_t begin,
                                     std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    s3[i] = conc[i];
    conc[i] += dt * dcdt[i];
  }
}

void SimCompartment::doRK212Substep1(double dt) {
  s2.resize(conc.size());
  s3.resize(conc.size());
  doRK212Substep1(dt, 0, conc.size());
}

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::doRK212Substep1_tbb(double dt) {
  s2.resize(conc.size());
  s3.resize(conc.size());
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, conc.size()),
                    [this, dt](const tbb::blocked_range<std::size_t> &r) {
                      doRK212Substep1(dt, r.begin(), r.end());
                    });
}
#endif

void SimCompartment::doRK212Substep2(double dt, std::size_t begin,
                                     std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    s2[i] = conc[i];
    conc[i] = 0.5 * s3[i] + 0.5 * conc[i] + 0.5 * dt * dcdt[i];
  }
}

void SimCompartment::doRK212Substep2(double dt) {
  doRK212Substep2(dt, 0, conc.size());
}

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::doRK212Substep2_tbb(double dt) {
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, conc.size()),
                    [this, dt](const tbb::blocked_range<std::size_t> &r) {
                      doRK212Substep2(dt, r.begin(), r.end());
                    });
}
#endif

void SimCompartment::doRKSubstep(double dt, double g1, double g2, double g3,
                                 double beta, double delta, std::size_t begin,
                                 std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    s2[i] += delta * conc[i];
    conc[i] = g1 * conc[i] + g2 * s2[i] + g3 * s3[i] + beta * dt * dcdt[i];
  }
}

void SimCompartment::doRKSubstep(double dt, double g1, double g2, double g3,
                                 double beta, double delta) {
  doRKSubstep(dt, g1, g2, g3, beta, delta, 0, conc.size());
}

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::doRKSubstep_tbb(double dt, double g1, double g2, double g3,
                                     double beta, double delta) {
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, conc.size()),
                    [this, dt, g1, g2, g3, beta,
                     delta](const tbb::blocked_range<std::size_t> &r) {
                      doRKSubstep(dt, g1, g2, g3, beta, delta, r.begin(),
                                  r.end());
                    });
}
#endif

void SimCompartment::doRKFinalise(double cFactor, double s2Factor,
                                  double s3Factor, std::size_t begin,
                                  std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    s2[i] = cFactor * conc[i] + s2Factor * s2[i] + s3Factor * s3[i];
  }
}

void SimCompartment::doRKFinalise(double cFactor, double s2Factor,
                                  double s3Factor) {
  doRKFinalise(cFactor, s2Factor, s3Factor, 0, conc.size());
}

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::doRKFinalise_tbb(double cFactor, double s2Factor,
                                      double s3Factor) {
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, conc.size()),
                    [this, cFactor, s2Factor,
                     s3Factor](const tbb::blocked_range<std::size_t> &r) {
                      doRKFinalise(cFactor, s2Factor, s3Factor, r.begin(),
                                   r.end());
                    });
}
#endif

void SimCompartment::undoRKStep(std::size_t begin, std::size_t end) {
#ifdef SPATIAL_MODEL_EDITOR_WITH_OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = begin; i < end; ++i) {
    conc[i] = s3[i];
  }
}

void SimCompartment::undoRKStep() { undoRKStep(0, conc.size()); }

#ifdef SPATIAL_MODEL_EDITOR_WITH_TBB
void SimCompartment::undoRKStep_tbb() {
  tbb::parallel_for(tbb::blocked_range<std::size_t>(0, conc.size()),
                    [this](const tbb::blocked_range<std::size_t> &r) {
                      undoRKStep(r.begin(), r.end());
                    });
}
#endif

PixelIntegratorError SimCompartment::calculateRKError(double epsilon) const {
  PixelIntegratorError err{0.0, 0.0};
  std::size_t n{conc.size()};
  for (std::size_t i = 0; i < n; ++i) {
    double localErr = std::abs(conc[i] - s2[i]);
    err.abs = std::max(err.abs, localErr);
    // average current and previous concentrations and add a (hopefully) small
    // constant term to avoid dividing by c=0 issues
    double localNorm = 0.5 * (conc[i] + s3[i] + epsilon);
    err.rel = std::max(err.rel, localErr / localNorm);
  }
  return err;
}

std::string SimCompartment::plotRKError(QImage& image, double epsilon, double max) const{
  if(image.isNull()){
    image = QImage(comp->getCompartmentImage().size(), QImage::Format_RGB32);
    image.fill(qRgb(0,0,0));
  }
  std::size_t iSpecies{nSpecies+1};
  for (std::size_t i = 0; i < conc.size(); ++i) {
    double localErr = std::abs(conc[i] - s2[i]);
    double localNorm = 0.5 * (conc[i] + s3[i] + epsilon);
    double pixelIntensity{localErr/localNorm/max};
    auto red{static_cast<int>(255.0*pixelIntensity)};
    auto point{comp->getPixel(i/nSpecies)};
    auto oldRed{qRed(image.pixel(point))};
    if(red > oldRed){
      image.setPixel(point, qRgb(red, 0, 0));
      if(red > 254){
        // update index of species with largest error
        iSpecies = i % nSpecies;
      }
    }
  }
  if(iSpecies < nSpecies){
    return speciesNames[iSpecies];
  }
  return {};
}


const std::string &SimCompartment::getCompartmentId() const {
  return compartmentId;
}

const std::vector<std::string> &SimCompartment::getSpeciesIds() const {
  return speciesIds;
}

const std::vector<double> &SimCompartment::getConcentrations() const {
  return conc;
}

double
SimCompartment::getLowerOrderConcentration(std::size_t speciesIndex,
                                           std::size_t pixelIndex) const {
  if (s2.empty()) {
    return 0;
  }
  return s2[pixelIndex * nSpecies + speciesIndex];
}

const std::vector<QPoint> &SimCompartment::getPixels() const {
  return comp->getPixels();
}

std::vector<double> &SimCompartment::getDcdt() { return dcdt; }

double SimCompartment::getMaxStableTimestep() const {
  return maxStableTimestep;
}

SimMembrane::SimMembrane(const model::Model &doc,
                         const geometry::Membrane *membrane_ptr,
                         SimCompartment *simCompA, SimCompartment *simCompB,
                         bool doCSE, unsigned optLevel, bool timeDependent,
                         bool spaceDependent)
    : membrane(membrane_ptr), compA(simCompA), compB(simCompB) {
  if (timeDependent) {
    ++nExtraVars;
  }
  if (spaceDependent) {
    nExtraVars += 2;
  }
  if (compA != nullptr &&
      membrane->getCompartmentA()->getId() != compA->getCompartmentId()) {
    SPDLOG_ERROR("compA '{}' doesn't match simCompA '{}'",
                 membrane->getCompartmentA()->getId(),
                 compA->getCompartmentId());
  }
  if (compB != nullptr &&
      membrane->getCompartmentB()->getId() != compB->getCompartmentId()) {
    SPDLOG_ERROR("compB '{}' doesn't match simCompB '{}'",
                 membrane->getCompartmentB()->getId(),
                 compB->getCompartmentId());
  }
  SPDLOG_DEBUG("membrane: {}", membrane->getId());
  SPDLOG_DEBUG("  - compA: {}",
               compA != nullptr ? compA->getCompartmentId() : "");
  SPDLOG_DEBUG("  - compB: {}",
               compB != nullptr ? compB->getCompartmentId() : "");

  // make vector of species from compartments A and B
  std::vector<std::string> speciesIds;
  if (compA != nullptr) {
    for (std::size_t i = 0; i < compA->getSpeciesIds().size() - nExtraVars;
         ++i) {
      speciesIds.push_back(compA->getSpeciesIds()[i]);
    }
  }
  if (compB != nullptr) {
    for (std::size_t i = 0; i < compB->getSpeciesIds().size() - nExtraVars;
         ++i) {
      speciesIds.push_back(compB->getSpeciesIds()[i]);
    }
  }

  // get rescaling factor to convert flux to amount/length^3,
  // then length^3 to volume to give concentration
  const auto &lengthUnit = doc.getUnits().getLength();
  const auto &volumeUnit = doc.getUnits().getVolume();
  double volOverL3 = model::getVolOverL3(lengthUnit, volumeUnit);
  double pixelWidth{doc.getGeometry().getPixelWidth()};
  SPDLOG_INFO("  - [vol]/[length]^3 = {}", volOverL3);
  SPDLOG_INFO("  - pixel width = {}", pixelWidth);
  SPDLOG_INFO("  - multiplying reaction by '{}'", volOverL3 / pixelWidth);
  // make vector of reaction IDs from membrane
  std::vector<std::string> reactionID =
      utils::toStdString(doc.getReactions().getIds(membrane->getId().c_str()));
  reacEval = ReacEval(doc, speciesIds, reactionID, volOverL3 / pixelWidth,
                      doCSE, optLevel, timeDependent, spaceDependent);
}

void SimMembrane::evaluateReactions() {
  std::size_t nSpeciesA{0};
  const std::vector<double> *concA{nullptr};
  std::vector<double> *dcdtA{nullptr};
  if (compA != nullptr) {
    nSpeciesA = compA->getSpeciesIds().size() - nExtraVars;
    concA = &compA->getConcentrations();
    dcdtA = &compA->getDcdt();
  }
  std::size_t nSpeciesB{0};
  const std::vector<double> *concB{nullptr};
  std::vector<double> *dcdtB{nullptr};
  if (compB != nullptr) {
    nSpeciesB = compB->getSpeciesIds().size() - nExtraVars;
    concB = &compB->getConcentrations();
    dcdtB = &compB->getDcdt();
  }
  std::vector<double> species(nSpeciesA + nSpeciesB + nExtraVars, 0);
  std::vector<double> result(nSpeciesA + nSpeciesB + nExtraVars, 0);
  for (const auto &[ixA, ixB] : membrane->getIndexPairs()) {
    // populate species concentrations: first A, then B, then t,x,y
    if (concA != nullptr) {
      std::copy_n(&((*concA)[ixA * (nSpeciesA + nExtraVars)]), nSpeciesA,
                  &species[0]);
    }
    if (concB != nullptr) {
      std::copy_n(&((*concB)[ixB * (nSpeciesB + nExtraVars)]),
                  nSpeciesB + nExtraVars, &species[nSpeciesA]);
    } else if (concA != nullptr) {
      std::copy_n(&((*concA)[ixA * (nSpeciesA + nExtraVars) + nSpeciesA]),
                  nExtraVars, &species[nSpeciesA]);
    }

    // evaluate reaction terms
    reacEval.evaluate(result.data(), species.data());

    // add results to dc/dt: first A, then B
    for (std::size_t is = 0; is < nSpeciesA; ++is) {
      (*dcdtA)[ixA * (nSpeciesA + nExtraVars) + is] += result[is];
    }
    for (std::size_t is = 0; is < nSpeciesB; ++is) {
      (*dcdtB)[ixB * (nSpeciesB + nExtraVars) + is] += result[is + nSpeciesA];
    }
  }
}

} // namespace simulate

} // namespace sme

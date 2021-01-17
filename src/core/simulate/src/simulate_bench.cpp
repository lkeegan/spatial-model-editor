#include "bench.hpp"
#include "simulate.hpp"
#include "simulate_options.hpp"

template <typename T>
static void simulate_SimulationDUNE(benchmark::State &state) {
  T data;
  std::unique_ptr<simulate::Simulation> simulation;
  for (auto _ : state) {
    simulation.reset();
    simulation = std::make_unique<simulate::Simulation>(
        data.model, simulate::SimulatorType::DUNE);
  }
}

template <typename T>
static void simulate_SimulationPIXEL(benchmark::State &state) {
  T data;
  std::unique_ptr<simulate::Simulation> simulation;
  for (auto _ : state) {
    simulation = std::make_unique<simulate::Simulation>(
        data.model, simulate::SimulatorType::Pixel);
  }
}

template <typename T>
static void simulate_Simulation_getConcImage(benchmark::State &state) {
  T data;
  simulate::Simulation simulation(data.model);
  QImage img;
  for (auto _ : state) {
    img = simulation.getConcImage(0);
  }
}

SME_BENCHMARK(simulate_SimulationDUNE);
SME_BENCHMARK(simulate_SimulationPIXEL);
SME_BENCHMARK(simulate_Simulation_getConcImage);

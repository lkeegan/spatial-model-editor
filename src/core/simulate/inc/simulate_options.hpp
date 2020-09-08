// Simulator Options

#pragma once

#include <cstddef>
#include <limits>
#include <string>

namespace simulate {

enum class DuneDiscretizationType { FEM1 };

struct DuneOptions {
  DuneDiscretizationType discretization{DuneDiscretizationType::FEM1};
  std::string integrator{"alexander_2"};
  double dt{1e-1};
  double minDt{1e-10};
  double maxDt{1e4};
  double increase{1.5};
  double decrease{0.5};
  bool writeVTKfiles{false};
};

enum class PixelIntegratorType { RK101, RK212, RK323, RK435 };

struct PixelIntegratorError {
  double abs{std::numeric_limits<double>::max()};
  double rel{0.005};
};

struct PixelOptions {
  PixelIntegratorType integrator{PixelIntegratorType::RK212};
  PixelIntegratorError maxErr;
  double maxTimestep{std::numeric_limits<double>::max()};
  bool enableMultiThreading{false};
  std::size_t maxThreads{0};
  bool doCSE{true};
  unsigned optLevel{3};
};

struct Options {
  DuneOptions dune;
  PixelOptions pixel;
};

} // namespace simulate

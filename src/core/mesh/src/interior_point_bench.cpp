#include "interior_point.hpp"
#include "bench.hpp"

template <typename T> static void mesh_getInteriorPoints(benchmark::State &state) {
  T data;
  std::vector<std::vector<QPointF>> interiorPoints;
  for (auto _ : state) {
    interiorPoints = mesh::getInteriorPoints(data.img, data.colours);
  }
}

SME_BENCHMARK(mesh_getInteriorPoints);

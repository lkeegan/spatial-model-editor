#include "mesh.hpp"

#include <QPoint>

#include "catch.hpp"

#include "logger.hpp"

// note: only valid for vectors of unique values
static bool isCyclicPermutation(const std::vector<QPoint>& v1,
                                const std::vector<QPoint>& v2) {
  spdlog::info("isCyclicPermutation :: v1 {}", v1);
  spdlog::info("isCyclicPermutation :: v2 {}", v2);
  if (v1.size() != v2.size()) {
    // different size cannot be permutations
    return false;
  }
  auto iter = std::find(v1.cbegin(), v1.cend(), v2[0]);
  if (iter == v1.cend()) {
    // first element of v2 not found in v1
    return false;
  }
  // v1[i0] == v2[0]
  std::size_t i0 = static_cast<std::size_t>(iter - v1.cbegin());
  if (v1[i0 + 1] == v2[1]) {
    // match next element going forwards
    for (std::size_t i = 0; i < v1.size(); ++i) {
      // check the rest
      if (v1[(i0 + i) % v1.size()] != v2[i]) {
        return false;
      }
    }
  } else if (v1[(i0 + v1.size() - 1) % v1.size()] == v2[1]) {
    // match next element going backwards
    for (std::size_t i = 0; i < v1.size(); ++i) {
      // check the rest
      if (v1[(i0 + v1.size() - i) % v1.size()] != v2[i]) {
        return false;
      }
    }
  } else {
    // neither forwards or backwards match
    return false;
  }
  return true;
}

SCENARIO("image: empty image", "[mesh][boundary][non-gui]") {
  QImage img(24, 32, QImage::Format_RGB32);
  img.fill(QColor(0, 0, 0).rgb());
  // image boundary pixels
  std::vector<QPoint> imgBoundary;
  imgBoundary.emplace_back(0, 0);
  imgBoundary.emplace_back(0, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, 0);

  mesh::Mesh mesh(img);

  // check boundaries
  const auto& boundaries = mesh.getBoundaries();
  CAPTURE(boundaries[0].points);
  REQUIRE(boundaries.size() == 1);
  REQUIRE(boundaries[0].isLoop);
  REQUIRE(isCyclicPermutation(boundaries[0].points, imgBoundary));

  // check triangles
  const auto& triangles = mesh.getTriangles();
  REQUIRE(triangles.size() == 1);
  REQUIRE(triangles[0].size() == 2);
  REQUIRE(triangles[0][0] ==
          std::array<QPoint, 3>{QPoint(0, 31), QPoint(0, 0), QPoint(23, 0)});
  REQUIRE(triangles[0][1] ==
          std::array<QPoint, 3>{QPoint(23, 0), QPoint(23, 31), QPoint(0, 31)});

  // check gmsh output
  QStringList msh = mesh.getGMSH().split("\n");
  REQUIRE(msh[0] == "$MeshFormat");
  REQUIRE(msh[1] == "2.2 0 8");
  REQUIRE(msh[2] == "$EndMeshFormat");
  REQUIRE(msh[3] == "$Nodes");
  REQUIRE(msh[4] == "4");
  REQUIRE(msh[5] == "1 0 31 0");
  REQUIRE(msh[6] == "2 0 0 0");
  REQUIRE(msh[7] == "3 23 0 0");
  REQUIRE(msh[8] == "4 23 31 0");
  REQUIRE(msh[9] == "$EndNodes");
  REQUIRE(msh[10] == "$Elements");
  REQUIRE(msh[11] == "2");
  REQUIRE(msh[12] == "1 2 2 1 1 2 1 4");
  REQUIRE(msh[13] == "2 2 2 1 1 4 3 2");
  REQUIRE(msh[14] == "$EndElements");
}

SCENARIO("image: single convex compartment", "[mesh][boundary][non-gui]") {
  QImage img(24, 32, QImage::Format_RGB32);
  img.fill(QColor(0, 0, 0).rgb());
  // image boundary pixels
  std::vector<QPoint> imgBoundary;
  imgBoundary.emplace_back(0, 0);
  imgBoundary.emplace_back(0, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, 0);

  // compartment boundary pixels
  std::vector<QPoint> correctBoundary;
  correctBoundary.emplace_back(5, 5);
  correctBoundary.emplace_back(6, 5);
  correctBoundary.emplace_back(7, 6);
  correctBoundary.emplace_back(6, 7);
  correctBoundary.emplace_back(5, 7);
  correctBoundary.emplace_back(4, 6);
  QRgb col = QColor(123, 123, 123).rgb();
  for (const auto& p : correctBoundary) {
    img.setPixel(p, col);
  }
  // fill in internal compartment pixels
  img.setPixel(5, 6, col);
  img.setPixel(6, 6, col);

  mesh::Mesh mesh(img);

  // check boundaries
  const auto& boundaries = mesh.getBoundaries();
  CAPTURE(boundaries[0].points);
  CAPTURE(boundaries[1].points);
  REQUIRE(boundaries.size() == 2);
  REQUIRE(boundaries[0].isLoop);
  REQUIRE(boundaries[1].isLoop);
  REQUIRE(isCyclicPermutation(boundaries[0].points, imgBoundary));
  REQUIRE(isCyclicPermutation(boundaries[1].points, correctBoundary));
}

SCENARIO("image: larger convex compartment, degenerate points",
         "[mesh][boundary][non-gui]") {
  QImage img(24, 32, QImage::Format_RGB32);
  img.fill(QColor(0, 0, 0).rgb());
  // image boundary pixels
  std::vector<QPoint> imgBoundary;
  imgBoundary.emplace_back(0, 0);
  imgBoundary.emplace_back(0, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, 0);

  // compartment boundary pixels
  std::vector<QPoint> correctBoundary;
  correctBoundary.emplace_back(5, 5);
  correctBoundary.emplace_back(8, 5);
  correctBoundary.emplace_back(9, 6);
  correctBoundary.emplace_back(7, 8);
  correctBoundary.emplace_back(6, 8);
  correctBoundary.emplace_back(4, 6);
  QRgb col = QColor(123, 123, 123).rgb();
  for (const auto& p : correctBoundary) {
    img.setPixel(p, col);
  }
  // fill in straight boundary pixels
  img.setPixel(6, 5, col);
  img.setPixel(7, 5, col);
  img.setPixel(8, 7, col);
  img.setPixel(5, 7, col);
  // fill in internal compartment pixels
  img.setPixel(5, 6, col);
  img.setPixel(6, 6, col);
  img.setPixel(7, 6, col);
  img.setPixel(8, 6, col);
  img.setPixel(6, 7, col);
  img.setPixel(7, 7, col);
  mesh::Mesh mesh(img);
  const auto& boundaries = mesh.getBoundaries();
  CAPTURE(boundaries[0].points);
  CAPTURE(boundaries[1].points);
  REQUIRE(boundaries[0].isLoop);
  REQUIRE(boundaries[1].isLoop);
  REQUIRE(isCyclicPermutation(boundaries[0].points, imgBoundary));
  REQUIRE(isCyclicPermutation(boundaries[1].points, correctBoundary));
}

SCENARIO("image: single concave compartment", "[mesh][boundary][non-gui]") {
  QImage img(24, 32, QImage::Format_RGB32);
  img.fill(QColor(0, 0, 0).rgb());
  // image boundary pixels
  std::vector<QPoint> imgBoundary;
  imgBoundary.emplace_back(0, 0);
  imgBoundary.emplace_back(0, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, 0);

  // compartment boundary pixels
  std::vector<QPoint> correctBoundary;
  correctBoundary.emplace_back(12, 10);
  correctBoundary.emplace_back(15, 10);
  correctBoundary.emplace_back(15, 12);
  correctBoundary.emplace_back(14, 12);
  correctBoundary.emplace_back(13, 13);
  correctBoundary.emplace_back(15, 15);
  correctBoundary.emplace_back(15, 16);
  correctBoundary.emplace_back(12, 16);
  correctBoundary.emplace_back(10, 14);
  correctBoundary.emplace_back(10, 13);
  correctBoundary.emplace_back(12, 11);
  QRgb col = QColor(123, 123, 123).rgb();
  for (const auto& p : correctBoundary) {
    img.setPixel(p, col);
  }
  // fill in degenerate boundary points
  img.setPixel(13, 10, col);
  img.setPixel(14, 10, col);
  img.setPixel(15, 11, col);
  img.setPixel(14, 14, col);
  img.setPixel(14, 16, col);
  img.setPixel(13, 16, col);
  img.setPixel(11, 15, col);
  img.setPixel(11, 12, col);
  // fill in internal compartment pixels
  img.setPixel(13, 11, col);
  img.setPixel(14, 11, col);
  img.setPixel(12, 12, col);
  img.setPixel(13, 12, col);
  img.setPixel(13, 11, col);
  img.setPixel(11, 13, col);
  img.setPixel(12, 13, col);
  img.setPixel(11, 14, col);
  img.setPixel(12, 14, col);
  img.setPixel(12, 15, col);
  img.setPixel(13, 14, col);
  img.setPixel(13, 15, col);
  img.setPixel(14, 15, col);
  mesh::Mesh mesh(img);
  const auto& boundaries = mesh.getBoundaries();
  CAPTURE(boundaries[0].points);
  CAPTURE(boundaries[1].points);
  REQUIRE(boundaries[0].isLoop);
  REQUIRE(boundaries[1].isLoop);
  REQUIRE(isCyclicPermutation(boundaries[0].points, imgBoundary));
  REQUIRE(isCyclicPermutation(boundaries[1].points, correctBoundary));
}

SCENARIO("image: two touching comps, two fixed point pixels",
         "[mesh][boundary][non-gui]") {
  QImage img(21, 19, QImage::Format_RGB32);
  // background compartment
  img.fill(QColor(0, 0, 0).rgb());
  // left compartment
  QRgb col0 = QColor(223, 183, 243).rgb();
  img.setPixel(10, 10, col0);
  img.setPixel(11, 10, col0);
  img.setPixel(12, 10, col0);
  img.setPixel(10, 11, col0);
  img.setPixel(11, 11, col0);
  img.setPixel(12, 11, col0);
  img.setPixel(10, 12, col0);
  img.setPixel(11, 12, col0);
  img.setPixel(12, 12, col0);
  // right compartment
  QRgb col1 = QColor(77, 73, 11).rgb();
  img.setPixel(13, 10, col1);
  img.setPixel(14, 10, col1);
  img.setPixel(15, 10, col1);
  img.setPixel(13, 11, col1);
  img.setPixel(14, 11, col1);
  img.setPixel(15, 11, col1);
  img.setPixel(13, 12, col1);
  img.setPixel(14, 12, col1);
  img.setPixel(15, 12, col1);

  // image boundary pixels
  std::vector<QPoint> imgBoundary;
  imgBoundary.emplace_back(0, 0);
  imgBoundary.emplace_back(0, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, img.height() - 1);
  imgBoundary.emplace_back(img.width() - 1, 0);

  // boundary lines
  std::vector<QPoint> bound1;
  bound1.emplace_back(12, 10);
  bound1.emplace_back(12, 12);

  std::vector<QPoint> bound2;
  bound2.emplace_back(12, 12);
  bound2.emplace_back(10, 12);
  bound2.emplace_back(10, 10);
  bound2.emplace_back(12, 10);

  std::vector<QPoint> bound3;
  bound3.emplace_back(12, 10);
  bound3.emplace_back(15, 10);
  bound3.emplace_back(15, 12);
  bound3.emplace_back(12, 12);

  mesh::Mesh mesh(img);
  const auto& boundaries = mesh.getBoundaries();
  CAPTURE(boundaries[0].points);
  CAPTURE(boundaries[1].points);
  CAPTURE(boundaries[2].points);
  CAPTURE(boundaries[3].points);
  REQUIRE(boundaries[0].isLoop);
  REQUIRE(!boundaries[1].isLoop);
  REQUIRE(!boundaries[2].isLoop);
  REQUIRE(!boundaries[3].isLoop);
  REQUIRE(isCyclicPermutation(boundaries[0].points, imgBoundary));
  REQUIRE(isCyclicPermutation(boundaries[1].points, bound3));
  REQUIRE(isCyclicPermutation(boundaries[2].points, bound2));
  REQUIRE(isCyclicPermutation(boundaries[3].points, bound1));
}

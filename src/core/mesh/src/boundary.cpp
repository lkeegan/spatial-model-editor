#include "boundary.hpp"
#include "contour_map.hpp"
#include "interior_point.hpp"
#include "line_simplifier.hpp"
#include "logger.hpp"
#include "mesh_utils.hpp"
#include "pixel_corner_iterator.hpp"
#include "utils.hpp"
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include <utility>

namespace sme::mesh {

bool Boundary::isLoop() const { return lineSimplifier.isLoop(); }

bool Boundary::isValid() const { return lineSimplifier.isValid(); }

const std::vector<QPoint> &Boundary::getPoints() const { return points; }

const std::vector<QPoint> &Boundary::getAllPoints() const {
  return lineSimplifier.getAllVertices();
}

std::size_t Boundary::getMaxPoints() const { return maxPoints; }

void Boundary::setMaxPoints(std::size_t nMaxPoints) {
  maxPoints = nMaxPoints;
  lineSimplifier.getSimplifiedLine(points, maxPoints);
}

std::size_t Boundary::setMaxPoints() {
  lineSimplifier.getSimplifiedLine(points);
  maxPoints = points.size();
  return maxPoints;
}

Boundary::Boundary(const std::vector<QPoint> &boundaryPoints, bool isClosedLoop)
    : lineSimplifier(boundaryPoints, isClosedLoop) {
  if (!lineSimplifier.isValid()) {
    return;
  }
  setMaxPoints();
}

static std::vector<QPoint>
toQPointsInvertYAxis(const std::vector<cv::Point> &points, int height) {
  std::vector<QPoint> v;
  v.reserve(points.size());
  for (const auto &p : points) {
    v.push_back({p.x, height - 1 - p.y});
  }
  return v;
}

static void
extractContoursFromMask(const cv::Mat &mask,
                        std::vector<std::vector<cv::Point>> &edges) {
  // get contours of compartment as closed loops
  std::vector<std::vector<cv::Point>> compContours;
  // for each contour, last component of hierarchy is index of parent
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(mask, compContours, hierarchy, cv::RETR_CCOMP,
                   cv::CHAIN_APPROX_NONE);
  for (std::size_t i = 0; i < compContours.size(); ++i) {
    auto &edgeContour = edges.emplace_back();
    const auto &compContour = compContours[i];
    bool outer = hierarchy[i][3] == -1; // -1: no parent, i.e. outer contour
    SPDLOG_TRACE("  - {} pixels", compContour.size());
    SPDLOG_TRACE("  - outer: {}", outer);
    PixelCornerIterator cpi(compContour, outer);
    while (!cpi.done()) {
      edgeContour.push_back(cpi.vertex());
      SPDLOG_TRACE("    - ({},{})", edgeContour.back().x, edgeContour.back().y);
      ++cpi;
    }
  }
}

static Contours getContours(const QImage &img,
                            const std::vector<QRgb> &compartmentColours) {
  Contours contours;
  for (auto col : compartmentColours) {
    auto binaryMask = makeBinaryMask(img, col);
    SPDLOG_TRACE("comp {:x}", col);
    extractContoursFromMask(binaryMask, contours.compartmentEdges);
  }
  auto binaryMask = makeBinaryMask(img, compartmentColours);
  SPDLOG_TRACE("domain");
  extractContoursFromMask(binaryMask, contours.domainEdges);
  return contours;
}

static std::vector<Boundary> splitContours(const QImage &img,
                                           Contours &contours) {
  std::vector<Boundary> boundaries;
  std::vector<std::vector<cv::Point>> loops;
  std::vector<std::vector<cv::Point>> lines;
  auto contourMap = ContourMap(img.size(), contours);
  for (auto &edges : contours.compartmentEdges) {
    // find the first fixed point, if any
    std::size_t startPixel{0};
    while (startPixel < edges.size() &&
           !contourMap.isFixedPoint(edges[startPixel])) {
      ++startPixel;
    }
    // no FP: closed loop
    if (startPixel == edges.size()) {
      SPDLOG_TRACE("Found loop with {} points", edges.size());
      if (std::none_of(loops.cbegin(), loops.cend(), [&edges](const auto &l) {
            return utils::isCyclicPermutation(edges, l);
          })) {
        SPDLOG_TRACE("  - adding loop", edges.size());
        auto points = toQPointsInvertYAxis(edges, img.height() + 1);
        boundaries.emplace_back(points, true);
        loops.push_back(std::move(edges));
      }
    } else {
      // need to split the loop into lines connecting fixed points
      // rotate so the first point is a fixed point
      std::rotate(
          edges.begin(),
          edges.begin() +
              static_cast<std::vector<cv::Point>::difference_type>(startPixel),
          edges.end());
      std::vector<cv::Point> line;
      line.push_back(edges.front());
      for (std::size_t i = 1; i < edges.size(); ++i) {
        line.push_back(edges[i]);
        if (contourMap.isFixedPoint(edges[i])) {
          SPDLOG_TRACE("Finished line with {} points", line.size());
          if (std::none_of(lines.cbegin(), lines.cend(),
                           [&line](const auto &l) {
                             return utils::isCyclicPermutation(line, l);
                           })) {
            SPDLOG_TRACE("  - adding line", edges.size());
            auto points = toQPointsInvertYAxis(line, img.height() + 1);
            boundaries.emplace_back(points, false);
            lines.push_back(std::move(line));
          }
          // start new line from final FP of previous line
          line.clear();
          line.push_back(edges[i]);
        }
      }
      line.push_back(edges.front());
      SPDLOG_TRACE("Finished line with {} points", line.size());
      if (std::none_of(lines.cbegin(), lines.cend(), [&line](const auto &l) {
            return utils::isCyclicPermutation(line, l);
          })) {
        SPDLOG_TRACE("  - adding line", edges.size());
        auto points = toQPointsInvertYAxis(line, img.height() + 1);
        boundaries.emplace_back(points, false);
        lines.push_back(std::move(line));
      }
    }
  }
  return boundaries;
}

std::vector<Boundary>
constructBoundaries(const QImage &img,
                    const std::vector<QRgb> &compartmentColours) {
  auto edgeContours = getContours(img, compartmentColours);
  return splitContours(img, edgeContours);
}

} // namespace sme

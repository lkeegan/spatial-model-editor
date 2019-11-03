#include "mesh.hpp"

#include <QPainter>

#include "logger.hpp"
#include "triangulate.hpp"
#include "utils.hpp"

namespace mesh {

QPointF Mesh::pixelPointToPhysicalPoint(
    const QPointF& pixelPoint) const noexcept {
  return pixelPoint * pixel + origin;
}

Mesh::Mesh(
    const QImage& image, const std::vector<QPointF>& interiorPoints,
    const std::vector<std::size_t>& maxPoints,
    const std::vector<std::size_t>& maxTriangleArea,
    const std::vector<std::pair<std::string, ColourPair>>& membraneColourPairs,
    double pixelWidth, const QPointF& originPoint)
    : img(image),
      origin(originPoint),
      pixel(pixelWidth),
      compartmentInteriorPoints(interiorPoints),
      boundaryMaxPoints(maxPoints),
      compartmentMaxTriangleArea(maxTriangleArea) {
  // construct map from colourPair to membrane index
  std::map<ColourPair, std::pair<std::size_t, std::string>>
      mapColourPairToMembraneIndex;
  for (std::size_t membraneIndex = 0;
       membraneIndex < membraneColourPairs.size(); ++membraneIndex) {
    auto membraneID = membraneColourPairs.at(membraneIndex).first;
    auto colPair = membraneColourPairs.at(membraneIndex).second;
    mapColourPairToMembraneIndex[colPair] = {membraneIndex, membraneID};
    mapColourPairToMembraneIndex[{colPair.second, colPair.first}] = {
        membraneIndex, membraneID};
    SPDLOG_DEBUG("Colour pair ({},{}) -> membrane {}, index {}", colPair.first,
                 colPair.second, membraneID, membraneIndex);
  }

  boundaries =
      boundary::constructBoundaries(image, mapColourPairToMembraneIndex);
  SPDLOG_INFO("found {} boundaries", boundaries.size());
  for (const auto& boundary : boundaries) {
    SPDLOG_INFO("  - {} points, loop={}, membrane={} [{}]",
                boundary.points.size(), boundary.isLoop, boundary.isMembrane,
                boundary.membraneID);
  }
  if (boundaryMaxPoints.size() != boundaries.size()) {
    // if boundary points not correctly specified use default value
    SPDLOG_INFO(
        "boundaryMaxPoints has size {}, but there are {} boundaries - "
        "using default value: {}",
        boundaryMaxPoints.size(), boundaries.size(), defaultBoundaryMaxPoints);
    boundaryMaxPoints =
        std::vector<std::size_t>(boundaries.size(), defaultBoundaryMaxPoints);
  }
  for (std::size_t i = 0; i < boundaries.size(); ++i) {
    boundaries[i].setMaxPoints(boundaryMaxPoints[i]);
  }
  SPDLOG_INFO("simplified {} boundaries", boundaries.size());
  for (const auto& boundary : boundaries) {
    SPDLOG_INFO("  - {} points, loop={}, membrane={}", boundary.points.size(),
                boundary.isLoop, boundary.isMembrane);
  }
  if (compartmentMaxTriangleArea.empty()) {
    // if triangle areas not specified use default value
    compartmentMaxTriangleArea = std::vector<std::size_t>(
        interiorPoints.size(), defaultCompartmentMaxTriangleArea);
    SPDLOG_INFO("no max triangle areas specified, using default value: {}",
                defaultCompartmentMaxTriangleArea);
  }
  // add interior point for each membrane
  for (const auto& boundary : boundaries) {
    if (boundary.isMembrane) {
      QPointF membraneInterior =
          (boundary.points[0] + boundary.outerPoints[0]) / 2.0;
      compartmentInteriorPoints.push_back(membraneInterior);
      // no max area for membrane triangles
      compartmentMaxTriangleArea.push_back(99);
    }
  }
  constructMesh();
}

Mesh::Mesh(const std::vector<double>& inputVertices,
           const std::vector<std::vector<int>>& inputTriangleIndices,
           const std::vector<QPointF>& interiorPoints)
    : compartmentInteriorPoints(interiorPoints) {
  // we don't have the original image, so no boundary info: read only mesh
  readOnlyMesh = true;
  // import vertices: supplied as list of doubles, two for each vertex
  std::size_t nV = inputVertices.size() / 2;
  vertices.clear();
  vertices.reserve(nV);
  for (std::size_t i = 0; i < nV; ++i) {
    vertices.push_back(QPointF(inputVertices[2 * i], inputVertices[2 * i + 1]));
  }
  // import triangles: supplies as list of ints, three for each triangle
  nTriangles = 0;
  triangles = std::vector<std::vector<QTriangleF>>(interiorPoints.size(),
                                                   std::vector<QTriangleF>{});
  triangleIndices = std::vector<std::vector<std::array<std::size_t, 3>>>(
      interiorPoints.size(), std::vector<std::array<std::size_t, 3>>{});
  for (std::size_t compIndex = 0; compIndex < interiorPoints.size();
       ++compIndex) {
    const auto& t = inputTriangleIndices[compIndex];
    std::size_t nT = t.size() / 3;
    triangles[compIndex].reserve(nT);
    triangleIndices[compIndex].reserve(nT);
    for (std::size_t i = 0; i < nT; ++i) {
      auto t0 = static_cast<std::size_t>(t[3 * i]);
      auto t1 = static_cast<std::size_t>(t[3 * i + 1]);
      auto t2 = static_cast<std::size_t>(t[3 * i + 2]);
      triangleIndices[compIndex].push_back({{t0, t1, t2}});
      triangles[compIndex].push_back(
          {{vertices[t0], vertices[t1], vertices[t2]}});
      ++nTriangles;
    }
  }
  SPDLOG_INFO("Imported read-only mesh of {} vertices, {} triangles",
              vertices.size(), nTriangles);
}

bool Mesh::isReadOnly() const { return readOnlyMesh; }

bool Mesh::isMembrane(std::size_t boundaryIndex) const {
  return boundaries.at(boundaryIndex).isMembrane;
}

void Mesh::setBoundaryMaxPoints(std::size_t boundaryIndex,
                                std::size_t maxPoints) {
  if (readOnlyMesh) {
    SPDLOG_INFO("mesh is read only, ignoring.");
    return;
  }
  SPDLOG_INFO("boundaryIndex {}: max points {} -> {}", boundaryIndex,
              boundaries.at(boundaryIndex).getMaxPoints(), maxPoints);
  boundaries.at(boundaryIndex).setMaxPoints(maxPoints);
}

std::size_t Mesh::getBoundaryMaxPoints(std::size_t boundaryIndex) const {
  return boundaries.at(boundaryIndex).getMaxPoints();
}

std::vector<std::size_t> Mesh::getBoundaryMaxPoints() const {
  std::vector<std::size_t> v(boundaries.size());
  std::transform(boundaries.cbegin(), boundaries.cend(), v.begin(),
                 [](const auto& b) { return b.getMaxPoints(); });
  return v;
}

void Mesh::setBoundaryWidth(std::size_t boundaryIndex, double width) {
  if (readOnlyMesh) {
    SPDLOG_INFO("mesh is read only, ignoring.");
    return;
  }
  SPDLOG_INFO("boundaryIndex {}: width {} -> {}", boundaryIndex,
              boundaries.at(boundaryIndex).getMembraneWidth(), width);
  boundaries.at(boundaryIndex).setMembraneWidth(width);
}

double Mesh::getBoundaryWidth(std::size_t boundaryIndex) const {
  return boundaries.at(boundaryIndex).getMembraneWidth();
}

double Mesh::getMembraneWidth(const std::string& membraneID) const {
  auto iter = std::find_if(boundaries.cbegin(), boundaries.cend(),
                           [membraneID](const auto& boundary) {
                             return boundary.membraneID == membraneID;
                           });
  if (iter != boundaries.cend()) {
    return iter->getMembraneWidth();
  }
  return 0;
}

void Mesh::setCompartmentMaxTriangleArea(std::size_t compartmentIndex,
                                         std::size_t maxTriangleArea) {
  if (readOnlyMesh) {
    SPDLOG_INFO("mesh is read only, ignoring.");
    return;
  }
  SPDLOG_INFO("compIndex {}: max triangle area {} -> {}", compartmentIndex,
              compartmentMaxTriangleArea.at(compartmentIndex), maxTriangleArea);
  compartmentMaxTriangleArea.at(compartmentIndex) = maxTriangleArea;
  constructMesh();
}

std::size_t Mesh::getCompartmentMaxTriangleArea(
    std::size_t compartmentIndex) const {
  return compartmentMaxTriangleArea.at(compartmentIndex);
}

const std::vector<std::size_t>& Mesh::getCompartmentMaxTriangleArea() const {
  return compartmentMaxTriangleArea;
}

const std::vector<boundary::Boundary>& Mesh::getBoundaries() const {
  return boundaries;
}

void Mesh::setPhysicalGeometry(double pixelWidth, const QPointF& originPoint) {
  pixel = pixelWidth;
  origin = originPoint;
}

std::vector<double> Mesh::getVertices() const {
  // convert from pixels to physical coordinates
  std::vector<double> v;
  v.reserve(vertices.size() * 2);
  for (const auto& pixelPoint : vertices) {
    auto physicalPoint = pixelPointToPhysicalPoint(pixelPoint);
    v.push_back(physicalPoint.x());
    v.push_back(physicalPoint.y());
  }
  return v;
}

std::vector<int> Mesh::getTriangleIndices(std::size_t compartmentIndex) const {
  std::vector<int> out;
  const auto& indices = triangleIndices[compartmentIndex];
  out.reserve(indices.size() * 3);
  for (const auto& t : indices) {
    for (std::size_t ti : t) {
      out.push_back(static_cast<int>(ti));
    }
  }
  return out;
}

const std::vector<std::vector<QTriangleF>>& Mesh::getTriangles() const {
  return triangles;
}

void Mesh::constructMesh() {
  // pixel points may be used by multiple boundary lines,
  // so first construct a set of unique points with a map to their index
  auto pointIndex = utils::QPointUniqueIndexer(img.size());
  for (const auto& boundary : boundaries) {
    pointIndex.addPoints(boundary.points);
  }
  std::vector<QPointF> boundaryPoints;
  // convert unique QPoints vector into vector of QPointF
  boundaryPoints.reserve(pointIndex.getPoints().size());
  for (const auto& p : pointIndex.getPoints()) {
    boundaryPoints.push_back(p);
  }
  // add outer membrane boundary lines: non-integers
  // can't be used by multiple boundary lines - no check for duplicates
  std::vector<size_t> membraneIndexOffsets;
  std::size_t index = pointIndex.getPoints().size();
  for (const auto& boundary : boundaries) {
    if (boundary.isMembrane) {
      // index of first membrane outer line point:
      membraneIndexOffsets.push_back(index);
      index += boundary.outerPoints.size();
      boundaryPoints.insert(boundaryPoints.end(), boundary.outerPoints.cbegin(),
                            boundary.outerPoints.cend());
    }
  }

  // for each boundary line, replace each QPoint
  // with its index in the list of boundaryPoints
  std::vector<triangulate::BoundarySegments> boundarySegmentsVector;
  for (std::size_t i = 0; i < boundaries.size(); ++i) {
    const auto& points = boundaries[i].points;
    boundarySegmentsVector.emplace_back();
    for (std::size_t j = 0; j < points.size() - 1; ++j) {
      auto i0 = pointIndex.getIndex(points[j]).value();
      auto i1 = pointIndex.getIndex(points[j + 1]).value();
      boundarySegmentsVector.back().push_back({{i0, i1}});
    }
    if (boundaries[i].isLoop) {
      // connect last point to first point
      auto i0 = pointIndex.getIndex(points.back()).value();
      auto i1 = pointIndex.getIndex(points.front()).value();
      boundarySegmentsVector.back().push_back({{i0, i1}});
    }
  }
  // repeat for outer boundaries of membranes:
  std::size_t iMembrane = 0;
  for (const auto& boundary : boundaries) {
    if (boundary.isMembrane) {
      // index of first point in loop:
      std::size_t pIndex = membraneIndexOffsets[iMembrane];
      boundarySegmentsVector.emplace_back();
      for (std::size_t j = 0; j < boundary.outerPoints.size() - 1; ++j) {
        boundarySegmentsVector.back().push_back({{pIndex, pIndex + 1}});
        ++pIndex;
      }
      // connect last point to first point
      boundarySegmentsVector.back().push_back(
          {{pIndex, membraneIndexOffsets[iMembrane]}});
      ++iMembrane;
    }
  }

  // interior point & max triangle area for each compartment
  std::vector<triangulate::Compartment> compartments;
  for (std::size_t i = 0; i < compartmentInteriorPoints.size(); ++i) {
    compartments.emplace_back(compartmentInteriorPoints[i],
                              compartmentMaxTriangleArea[i]);
  }
  // generate mesh
  triangulate::Triangulate triangulate(boundaryPoints, boundarySegmentsVector,
                                       compartments);
  vertices = triangulate.getPoints();

  // construct triangles & indices for each compartment:
  triangleIndices = std::vector<std::vector<TriangleIndex>>(
      compartments.size(), std::vector<TriangleIndex>{});
  triangles = std::vector<std::vector<QTriangleF>>(compartments.size(),
                                                   std::vector<QTriangleF>{});
  nTriangles = 0;
  for (const auto& t : triangulate.getTriangleIndices()) {
    std::size_t compIndex = t[0] - 1;
    triangleIndices[compIndex].push_back({{t[1], t[2], t[3]}});
    triangles[compIndex].push_back(
        {{vertices[t[1]], vertices[t[2]], vertices[t[3]]}});
    ++nTriangles;
  }
  SPDLOG_INFO("{} vertices, {} triangles", vertices.size(), nTriangles);
}

static double getScaleFactor(const QImage& img, const QSize& size) {
  double scaleFactor = 1;
  if (img.width() * size.height() > img.height() * size.width()) {
    scaleFactor =
        static_cast<double>(size.width()) / static_cast<double>(img.width());
  } else {
    scaleFactor =
        static_cast<double>(size.height()) / static_cast<double>(img.height());
  }
  return scaleFactor;
}

std::pair<QImage, QImage> Mesh::getBoundariesImages(
    const QSize& size, std::size_t boldBoundaryIndex) const {
  double scaleFactor = getScaleFactor(img, size);
  // construct boundary image
  QImage boundaryImage(static_cast<int>(scaleFactor * img.width()),
                       static_cast<int>(scaleFactor * img.height()),
                       QImage::Format_ARGB32_Premultiplied);
  boundaryImage.fill(QColor(0, 0, 0, 0).rgba());

  QImage maskImage(boundaryImage.size(), QImage::Format_ARGB32_Premultiplied);
  maskImage.fill(QColor(255, 255, 255).rgba());

  QPainter p(&boundaryImage);
  p.setRenderHint(QPainter::Antialiasing);

  QPainter pMask(&maskImage);

  // draw boundary lines
  for (std::size_t k = 0; k < boundaries.size(); ++k) {
    const auto& points = boundaries[k].points;
    std::size_t maxPoint = points.size();
    if (!boundaries[k].isLoop) {
      --maxPoint;
    }
    int penSize = 2;
    if (k == boldBoundaryIndex) {
      penSize = 5;
    }
    p.setPen(QPen(utils::indexedColours()[k], penSize));
    pMask.setPen(QPen(QColor(0, 0, static_cast<int>(k)), 15));
    for (std::size_t i = 0; i < maxPoint; ++i) {
      p.drawEllipse(points[i] * scaleFactor, penSize, penSize);
      p.drawLine(points[i] * scaleFactor,
                 points[(i + 1) % points.size()] * scaleFactor);
      pMask.drawLine(points[i] * scaleFactor,
                     points[(i + 1) % points.size()] * scaleFactor);
    }
    if (boundaries[k].isMembrane) {
      const auto& outerPoints = boundaries[k].outerPoints;
      for (std::size_t i = 0; i < maxPoint; ++i) {
        p.drawEllipse(outerPoints[i] * scaleFactor, penSize, penSize);
        p.drawLine(outerPoints[i] * scaleFactor,
                   outerPoints[(i + 1) % outerPoints.size()] * scaleFactor);
        pMask.drawLine(outerPoints[i] * scaleFactor,
                       outerPoints[(i + 1) % outerPoints.size()] * scaleFactor);
      }
    }
  }
  p.end();
  pMask.end();
  // flip image on y-axis, to change (0,0) from bottom-left to top-left corner
  return std::make_pair(boundaryImage.mirrored(false, true),
                        maskImage.mirrored(false, true));
}

std::pair<QImage, QImage> Mesh::getMeshImages(
    const QSize& size, std::size_t compartmentIndex) const {
  double scaleFactor = getScaleFactor(img, size);
  // construct mesh image
  QImage meshImage(static_cast<int>(scaleFactor * img.width()),
                   static_cast<int>(scaleFactor * img.height()),
                   QImage::Format_ARGB32_Premultiplied);
  meshImage.fill(QColor(0, 0, 0, 0));

  QImage maskImage(meshImage.size(), QImage::Format_ARGB32_Premultiplied);
  maskImage.fill(QColor(255, 255, 255).rgba());

  QPainter p(&meshImage);
  p.setRenderHint(QPainter::Antialiasing);

  QPainter pMask(&maskImage);
  QBrush maskBrush(QColor(0, 0, static_cast<int>(compartmentIndex)));

  QBrush fillBrush(QColor(235, 235, 255));
  p.setPen(QPen(Qt::black, 2));
  // fill triangles in chosen compartment & outline with bold lines
  for (const auto& t : triangles.at(compartmentIndex)) {
    QPainterPath path(t.back() * scaleFactor);
    for (const auto& tp : t) {
      path.lineTo(tp * scaleFactor);
    }
    p.fillPath(path, fillBrush);
    pMask.fillPath(path, maskBrush);
    p.drawPath(path);
  }
  // outline all other triangles with gray lines
  p.setPen(QPen(Qt::gray, 1));
  for (std::size_t k = 0; k < triangles.size(); ++k) {
    maskBrush.setColor(QColor(0, 0, static_cast<int>(k)));
    if (k != compartmentIndex) {
      for (const auto& t : triangles.at(k)) {
        QPainterPath path(t.back() * scaleFactor);
        for (const auto& tp : t) {
          path.lineTo(tp * scaleFactor);
        }
        p.drawPath(path);
        pMask.fillPath(path, maskBrush);
      }
    }
  }
  // draw vertices
  p.setPen(QPen(Qt::red, 3));
  for (const auto& v : vertices) {
    p.drawPoint(v * scaleFactor);
  }
  p.end();
  pMask.end();

  return std::make_pair(meshImage.mirrored(false, true),
                        maskImage.mirrored(false, true));
}

QString Mesh::getGMSH(const std::unordered_set<int>& gmshCompIndices) const {
  // note: gmsh indexing starts with 1, so we need to add 1 to all indices
  // note: gmsh (0,0) is bottom left, but we use top left, so flip y axis
  // meshing is done in terms of pixels, to convert to physical points:
  //   - rescale each vertex by a factor pixel
  //   - add origin to each vertex
  QString msh;
  msh.append("$MeshFormat\n");
  msh.append("2.2 0 8\n");
  msh.append("$EndMeshFormat\n");
  msh.append("$Nodes\n");
  msh.append(QString("%1\n").arg(vertices.size()));
  for (std::size_t i = 0; i < vertices.size(); ++i) {
    msh.append(QString("%1 %2 %3 %4\n")
                   .arg(i + 1)
                   .arg(vertices[i].x() * pixel + origin.x())
                   .arg(vertices[i].y() * pixel + origin.y())
                   .arg(0));
  }
  msh.append("$EndNodes\n");
  msh.append("$Elements\n");
  int nT = 0;
  std::size_t compartmentIndex = 1;
  for (const auto& comp : triangleIndices) {
    if (gmshCompIndices.empty() ||
        gmshCompIndices.find(static_cast<int>(compartmentIndex)) !=
            gmshCompIndices.cend()) {
      nT += static_cast<int>(comp.size());
    }
    compartmentIndex++;
  }
  msh.append(QString("%1\n").arg(nT));
  std::size_t triangleIndex = 1;
  compartmentIndex = 1;
  std::size_t outputCompartmentIndex = 1;
  for (const auto& comp : triangleIndices) {
    if (gmshCompIndices.empty() ||
        gmshCompIndices.find(static_cast<int>(compartmentIndex)) !=
            gmshCompIndices.cend()) {
      for (const auto& t : comp) {
        msh.append(QString("%1 2 2 %2 %2 %3 %4 %5\n")
                       .arg(triangleIndex)
                       .arg(outputCompartmentIndex)
                       .arg(t[0] + 1)
                       .arg(t[1] + 1)
                       .arg(t[2] + 1));
        ++triangleIndex;
      }
      ++outputCompartmentIndex;
    }
    ++compartmentIndex;
  }
  msh.append("$EndElements\n");
  return msh;
}

}  // namespace mesh

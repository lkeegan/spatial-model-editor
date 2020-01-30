#include "geometry.hpp"

#include "logger.hpp"
#include "utils.hpp"

namespace geometry {

Compartment::Compartment(const std::string &compID, const QImage &img, QRgb col)
    : imgComp(img.size(), QImage::Format_Mono), compartmentID(compID) {
  imgComp.setColor(0, qRgba(0, 0, 0, 0));
  imgComp.setColor(1, col);
  imgComp.fill(0);
  ix.clear();
  // find pixels in compartment: store image QPoint for each
  for (int x = 0; x < img.width(); ++x) {
    for (int y = 0; y < img.height(); ++y) {
      if (img.pixel(x, y) == col) {
        // if colour matches, add pixel to field
        QPoint p = QPoint(x, y);
        ix.push_back(p);
        imgComp.setPixel(p, 1);
      }
    }
  }
  utils::QPointIndexer ixIndexer(img.size(), ix);
  // find nearest neighbours of each point
  nn.clear();
  nn.reserve(4 * ix.size());
  // find neighbours of each pixel in compartment
  for (std::size_t i = 0; i < ix.size(); ++i) {
    const QPoint &p = ix[i];
    for (const auto &pp :
         {QPoint(p.x() + 1, p.y()), QPoint(p.x() - 1, p.y()),
          QPoint(p.x(), p.y() + 1), QPoint(p.x(), p.y() - 1)}) {
      auto index = ixIndexer.getIndex(pp);
      if (index) {
        // neighbour of p is in same compartment
        nn.push_back(index.value());
      } else {
        // neighbour of p is outside compartment
        // Neumann zero flux bcs: set external neighbour of p to itself
        nn.push_back(i);
      }
    }
  }
  SPDLOG_INFO("compartmentID: {}", compartmentID);
  SPDLOG_INFO("n_pixels: {}", ix.size());
  SPDLOG_INFO("colour: {:x}", col);
}

const std::string &Compartment::getId() const { return compartmentID; }

double Compartment::getPixelWidth() const { return pixelWidth; }

void Compartment::setPixelWidth(double width) { pixelWidth = width; }

const QImage &Compartment::getCompartmentImage() const { return imgComp; }

Membrane::Membrane(const std::string &ID, const Compartment *A,
                   const Compartment *B,
                   const std::vector<std::pair<QPoint, QPoint>> &membranePairs)
    : membraneID(ID), compA(A), compB(B) {
  SPDLOG_INFO("membraneID: {}", membraneID);
  SPDLOG_INFO("compartment A: {}", compA->getId());
  SPDLOG_INFO("compartment B: {}", compB->getId());
  SPDLOG_INFO("number of point pairs: {}", membranePairs.size());
  // convert each QPoint into the corresponding index of the field
  indexPair.clear();
  utils::QPointIndexer Aindexer(A->getCompartmentImage().size(),
                                A->getPixels());
  utils::QPointIndexer Bindexer(B->getCompartmentImage().size(),
                                B->getPixels());
  for (const auto &p : membranePairs) {
    auto iA = Aindexer.getIndex(p.first);
    auto iB = Bindexer.getIndex(p.second);
    indexPair.push_back({iA.value(), iB.value()});
  }
}

Field::Field(const Compartment *geom, const std::string &specID,
             double diffConst, const QColor &col)
    : speciesID(specID),
      geometry(geom),
      diffusionConstant(diffConst),
      colour(col),
      conc(geom->nPixels(), 0.0),
      dcdt(geom->nPixels(), 0.0),
      init(geom->nPixels(), 0.0) {
  SPDLOG_INFO("speciesID: {}", speciesID);
  SPDLOG_INFO("compartmentID: {}", geom->getId());
}

void Field::importConcentration(
    const std::vector<double> &sbmlConcentrationArray) {
  SPDLOG_INFO("species {}, compartment {}", speciesID, geometry->getId());
  SPDLOG_INFO("  - importing from sbml array of size {}",
              sbmlConcentrationArray.size());
  const auto &img = geometry->getCompartmentImage();
  if (static_cast<int>(sbmlConcentrationArray.size()) !=
      img.width() * img.height()) {
    SPDLOG_ERROR(
        "  - mismatch between array size [{}]"
        " and compartment image size [{}x{} = {}]",
        sbmlConcentrationArray.size(), img.width(), img.height(),
        img.width() * img.height());
    throw std::invalid_argument("invalid array size");
  }
  // NOTE: order of concentration array is [ (x=0,y=0), (x=1,y=0), ... ]
  // NOTE: (0,0) point is at bottom-left
  // NOTE: QImage has (0,0) point at top-left, so flip y-coord here
  for (std::size_t i = 0; i < geometry->nPixels(); ++i) {
    const auto &point = geometry->getPixel(i);
    int arrayIndex = point.x() + img.width() * (img.height() - 1 - point.y());
    conc[i] = sbmlConcentrationArray[static_cast<std::size_t>(arrayIndex)];
  }
  init = conc;
  isUniformConcentration = false;
}

void Field::setUniformConcentration(double concentration) {
  SPDLOG_INFO("species {}, compartment {}", speciesID, geometry->getId());
  SPDLOG_INFO("  - concentration = {}", concentration);
  std::fill(conc.begin(), conc.end(), concentration);
  init = conc;
  isUniformConcentration = true;
}

QImage Field::getConcentrationImage() const {
  auto img = QImage(geometry->getCompartmentImage().size(),
                    QImage::Format_ARGB32_Premultiplied);
  img.fill(qRgba(0, 0, 0, 0));
  // for now rescale conc to [0,1] to multiply species colour
  double cmax = *std::max_element(conc.cbegin(), conc.cend());
  if (cmax < 1e-15) {
    cmax = 1.0;
  }
  for (std::size_t i = 0; i < geometry->nPixels(); ++i) {
    double scale = conc[i] / cmax;
    int r = static_cast<int>(scale * colour.red());
    int g = static_cast<int>(scale * colour.green());
    int b = static_cast<int>(scale * colour.blue());
    img.setPixel(geometry->getPixel(i), qRgb(r, g, b));
  }
  return img;
}

std::vector<double> Field::getConcentrationArray() const {
  const auto &img = geometry->getCompartmentImage();
  int size = img.width() * img.height();
  // NOTE: order of concentration array is [ (x=0,y=0), (x=1,y=0), ... ]
  // NOTE: (0,0) point is at bottom-left
  // NOTE: QImage has (0,0) point at top-left, so flip y-coord here
  std::vector<double> arr(static_cast<std::size_t>(size), 0.0);
  for (std::size_t i = 0; i < geometry->nPixels(); ++i) {
    const auto &point = geometry->getPixel(i);
    int arrayIndex = point.x() + img.width() * (img.height() - 1 - point.y());
    arr[static_cast<std::size_t>(arrayIndex)] = conc[i];
  }
  return arr;
}

void Field::applyDiffusionOperator() {
  const Compartment *g = geometry;
  double pixelWidth = g->getPixelWidth();
  double d = diffusionConstant / pixelWidth / pixelWidth;
  for (std::size_t i = 0; i < geometry->nPixels(); ++i) {
    dcdt[i] = d * (conc[g->up_x(i)] + conc[g->dn_x(i)] + conc[g->up_y(i)] +
                   conc[g->dn_y(i)] - 4.0 * conc[i]);
  }
}

double Field::getMeanConcentration() const {
  return std::accumulate(conc.cbegin(), conc.cend(), static_cast<double>(0)) /
         static_cast<double>(conc.size());
}

double Field::getMinConcentration() const {
  return *std::min_element(conc.cbegin(), conc.cend());
}

double Field::getMaxConcentration() const {
  return *std::max_element(conc.cbegin(), conc.cend());
}

void Field::setCompartment(const Compartment *compartment) {
  if (geometry == compartment) {
    return;
  }
  SPDLOG_DEBUG("Changing compartment to {}", compartment->getId());
  geometry = compartment;
  conc.assign(geometry->nPixels(), 0.0);
  dcdt = conc;
  init = conc;
}

}  // namespace geometry

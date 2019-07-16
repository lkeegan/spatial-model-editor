#include "geometry.hpp"

#include <unordered_map>

#include "logger.hpp"

namespace geometry {

static int qPointToInt(const QPoint &point, int imgHeight) {
  return point.x() * imgHeight + point.y();
}

Compartment::Compartment(const std::string &compID, const QImage &img, QRgb col)
    : compartmentID(compID) {
  imgComp = QImage(img.size(), QImage::Format_Mono);
  imgComp.setColor(0, qRgba(0, 0, 0, 0));
  imgComp.setColor(1, col);
  imgComp.fill(0);
  ix.clear();
  // find pixels in compartment: store image QPoint for each
  for (int x = 0; x < img.width(); ++x) {
    for (int y = 0; y < img.height(); ++y) {
      if (img.pixel(x, y) == col) {
        // if colour matches, add pixel to field
        // qDebug("Compartment::init :: add Qpoint (%d, %d)", x, y);
        QPoint p = QPoint(x, y);
        ix.push_back(p);
        imgComp.setPixel(p, 1);
      }
    }
  }
  // find nearest neighbours of each point
  nn.clear();
  nn.reserve(4 * ix.size());
  // construct temporary map from qPoint to index
  std::unordered_map<int, std::size_t> nn_index;
  for (std::size_t i = 0; i < ix.size(); ++i) {
    nn_index[qPointToInt(ix[i], img.height())] = i;
  }
  // find neighbours of each pixel in compartment
  for (std::size_t i = 0; i < ix.size(); ++i) {
    const QPoint &p = ix[i];
    for (const auto &pp :
         {QPoint(p.x() + 1, p.y()), QPoint(p.x() - 1, p.y()),
          QPoint(p.x(), p.y() + 1), QPoint(p.x(), p.y() - 1)}) {
      const auto iter = nn_index.find(qPointToInt(pp, img.height()));
      if (iter != nn_index.cend()) {
        // neighbour of p is in same compartment
        nn.push_back(iter->second);
      } else {
        // neighbour of p is outside compartment
        // Neumann zero flux bcs: set external neighbour of p to itself
        nn.push_back(i);
      }
    }
  }
  spdlog::info("Compartment::Compartment :: compartmentID: {}", compartmentID);
  spdlog::info("Compartment::Compartment :: n_pixels: {}", ix.size());
  spdlog::info("Compartment::Compartment :: colour: {:x}", col);
}

const QImage &Compartment::getCompartmentImage() const { return imgComp; }

Membrane::Membrane(const std::string &ID, const Compartment *A,
                   const Compartment *B,
                   const std::vector<std::pair<QPoint, QPoint>> &membranePairs)
    : membraneID(ID), compA(A), compB(B) {
  spdlog::info("Membrane::Membrane :: membraneID: {}", membraneID);
  spdlog::info("Membrane::Membrane :: compartment A: {}", compA->compartmentID);
  spdlog::info("Membrane::Membrane :: compartment B: {}", compB->compartmentID);
  spdlog::info("Membrane::Membrane :: number of point pairs: {}",
               membranePairs.size());
  // convert each QPoint into the corresponding index of the field
  indexPair.clear();
  CompartmentIndexer indexA(*A);
  CompartmentIndexer indexB(*B);
  for (const auto &p : membranePairs) {
    auto iA = indexA.getIndex(p.first);
    auto iB = indexB.getIndex(p.second);
    indexPair.push_back({iA, iB});
  }
}

Field::Field(const Compartment *geom, const std::string &specID,
             double diffConst, const QColor &col)
    : speciesID(specID),
      geometry(geom),
      diffusionConstant(diffConst),
      colour(col) {
  spdlog::info("Field::Field :: speciesID: {}", speciesID);
  spdlog::info("Field::Field :: compartmentID: {}", geom->compartmentID);
  imgConc = QImage(geom->getCompartmentImage().size(), QImage::Format_ARGB32);
  conc.resize(geom->ix.size(), 0.0);
  dcdt = conc;
  init = conc;
  isUniformConcentration = true;
  isSpatial = false;
  red = colour.red();
  green = colour.green();
  blue = colour.blue();
}

void Field::importConcentration(const QImage &img, double scale_factor) {
  spdlog::info("Field::importConcentration :: species {}, compartment {}",
               speciesID, geometry->compartmentID);
  spdlog::info("Field::importConcentration ::   - importing from {}x{} image",
               img.width(), img.height());
  // rescaling [min, max] pixel values to the range [0, scale_factor] for now
  QRgb min = std::numeric_limits<QRgb>::max();
  QRgb max = 0;
  for (int x = 0; x < img.width(); ++x) {
    for (int y = 0; y < img.height(); ++y) {
      min = std::min(min, img.pixel(x, y) & 0x00ffffff);
      max = std::max(max, img.pixel(x, y) & 0x00ffffff);
    }
  }
  if (max == min) {
    if (max == 0) {
      // if all pixels zero, then set conc to zero
      spdlog::info(
          "Field::importConcentration ::   - all pixels black: rescaling -> 0");
      scale_factor = 0;
      max = 1;
    } else {
      // if all pixels equal, then set conc to scale_factor
      spdlog::info(
          "Field::importConcentration ::   - all pixels equal: rescaling -> {}",
          scale_factor);
      min -= 1;
    }
  } else {
    spdlog::info(
        "Field::importConcentration ::   - rescaling [{},{}] -> [{},{}]", min,
        max, 0.0, scale_factor);
  }
  for (std::size_t i = 0; i < geometry->ix.size(); ++i) {
    conc[i] =
        scale_factor *
        static_cast<double>((img.pixel(geometry->ix[i]) & 0x00ffffff) - min) /
        static_cast<double>(max - min);
  }
  init = conc;
  isUniformConcentration = false;
}

void Field::importConcentration(
    const std::vector<double> &sbmlConcentrationArray) {
  spdlog::info("Field::importConcentration :: species {}, compartment {}",
               speciesID, geometry->compartmentID);
  spdlog::info(
      "Field::importConcentration ::   - importing from sbml array of size {}",
      sbmlConcentrationArray.size());
  if (static_cast<int>(sbmlConcentrationArray.size()) !=
      geometry->getCompartmentImage().width() *
          geometry->getCompartmentImage().height()) {
    spdlog::warn(
        "Field::importConcentration ::   - mismatch between array size [{}] "
        "and compartment image size [{}x{} = {}]",
        sbmlConcentrationArray.size(), geometry->getCompartmentImage().width(),
        geometry->getCompartmentImage().height(),
        geometry->getCompartmentImage().width() *
            geometry->getCompartmentImage().height());
  }
  for (std::size_t i = 0; i < geometry->ix.size(); ++i) {
    const auto &point = geometry->ix[i];
    int Lx = geometry->getCompartmentImage().width();
    int arrayIndex = point.x() + Lx * point.y();
    conc[i] = sbmlConcentrationArray[static_cast<std::size_t>(arrayIndex)];
  }
  init = conc;
  isUniformConcentration = false;
}

void Field::setUniformConcentration(double concentration) {
  spdlog::info("Field::setUniformConcentration :: species {}, compartment {}",
               speciesID, geometry->compartmentID);
  spdlog::info("Field::setUniformConcentration ::   - concentration = {}",
               concentration);
  std::fill(conc.begin(), conc.end(), concentration);
  init = conc;
  isUniformConcentration = true;
}

const QImage &Field::getConcentrationImage() {
  imgConc.fill(qRgba(0, 0, 0, 0));
  // for now rescale conc to [0,1] to multiply species colour
  double cmax = *std::max_element(conc.begin(), conc.end());
  if (cmax < 1e-15) {
    cmax = 1.0;
  }
  for (std::size_t i = 0; i < geometry->ix.size(); ++i) {
    double scale = conc[i] / cmax;
    int r = static_cast<int>(std::round(scale * red));
    int g = static_cast<int>(std::round(scale * green));
    int b = static_cast<int>(std::round(scale * blue));
    imgConc.setPixel(geometry->ix[i], qRgb(r, g, b));
  }
  return imgConc;
}

std::vector<double> Field::getConcentrationArray() const {
  int size = geometry->getCompartmentImage().width() *
             geometry->getCompartmentImage().height();
  std::vector<double> arr(static_cast<std::size_t>(size), 0.0);
  for (std::size_t i = 0; i < geometry->ix.size(); ++i) {
    const auto &point = geometry->ix[i];
    int Lx = geometry->getCompartmentImage().width();
    int arrayIndex = point.x() + Lx * point.y();
    arr[static_cast<std::size_t>(arrayIndex)] = conc[i];
  }
  return arr;
}

void Field::applyDiffusionOperator() {
  const Compartment *g = geometry;
  for (std::size_t i = 0; i < geometry->ix.size(); ++i) {
    dcdt[i] = diffusionConstant *
              (conc[g->up_x(i)] + conc[g->dn_x(i)] + conc[g->up_y(i)] +
               conc[g->dn_y(i)] - 4.0 * conc[i]);
  }
}

double Field::getMeanConcentration() const {
  return std::accumulate(conc.cbegin(), conc.cend(), 0.0) /
         static_cast<double>(conc.size());
}

CompartmentIndexer::CompartmentIndexer(const Compartment &c)
    : comp(c), imgHeight(c.getCompartmentImage().height()) {
  // construct map from QPoint in image to index in compartment vector
  std::size_t i = 0;
  for (const auto &point : comp.ix) {
    index[qPointToInt(point, imgHeight)] = i++;
  }
}

std::size_t CompartmentIndexer::getIndex(const QPoint &point) {
  return index.at(qPointToInt(point, imgHeight));
}

}  // namespace geometry

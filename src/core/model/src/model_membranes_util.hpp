// Membrane utils

#pragma once

#include <QColor>
#include <QImage>
#include <QStringList>
#include <optional>
#include <string>
#include <vector>

#include "geometry.hpp"

namespace libsbml {
class Model;
}

namespace model {

using QPointPair = std::pair<QPoint, QPoint>;

class OrderedIntPairIndex {
private:
  int maxValue;
  std::size_t nItems{0};
  std::vector<std::size_t> values = {};
  std::size_t toIndex(int smaller, int larger) const;

public:
  explicit OrderedIntPairIndex(int maxKeyValue = 4);
  void clear();
  std::size_t findOrInsert(int smaller, int larger);
  std::optional<std::size_t> find(int smaller, int larger) const;
  std::size_t size() const;
};

class ImageMembranePixels {
private:
  std::vector<std::vector<QPointPair>> points;
  OrderedIntPairIndex colourIndexPairIndex;
  QVector<QRgb> colours;
  std::vector<QPointPair> nullPointsVector = {};
  QSize imageSize{0, 0};

public:
  explicit ImageMembranePixels();
  explicit ImageMembranePixels(const QImage &img);
  ~ImageMembranePixels();
  void setImage(const QImage &img);
  int getColourIndex(QRgb colour) const;
  const std::vector<QPointPair> *getPoints(int iA, int iB) const;
  const QSize &getImageSize() const;
};

} // namespace model

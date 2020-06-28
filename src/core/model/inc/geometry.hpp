// Spatial Geometry
//  - Compartment class: defines set of points that make up a compartment &
//  nearest neighbours for each point
//  - Membrane class: defines set of points on either side of the boundary
//  between two compartments, aka the membrane
//  - Field class: species concentration within a compartment

#pragma once

#include <QImage>
#include <vector>

namespace geometry {

class Compartment {
private:
  // indices of nearest neighbours
  std::vector<std::size_t> nn;
  std::string compartmentId;
  double pixelWidth = 1.0;
  // vector of points that make up compartment
  std::vector<QPoint> ix;
  QRgb colour{0};
  QImage image;

public:
  Compartment() = default;
  // create compartment geometry from all pixels in `img` of colour `col`
  Compartment(std::string compId, const QImage &img, QRgb col);
  const std::string &getId() const;
  QRgb getColour() const;
  double getPixelWidth() const;
  void setPixelWidth(double width);
  inline const std::vector<QPoint> &getPixels() const { return ix; }
  inline const QPoint &getPixel(std::size_t i) const { return ix[i]; }
  inline std::size_t nPixels() const { return ix.size(); }
  // e.g. ix[up_x[i]] is the +x neigbour of point ix[i]
  // a field stores the concentration at point ix[i] at index i
  // zero flux Neumann bcs: outside neighbour of point on boundary is itself
  inline std::size_t up_x(std::size_t i) const { return nn[4 * i]; }
  inline std::size_t dn_x(std::size_t i) const { return nn[4 * i + 1]; }
  inline std::size_t up_y(std::size_t i) const { return nn[4 * i + 2]; }
  inline std::size_t dn_y(std::size_t i) const { return nn[4 * i + 3]; }
  // return a QImage of the compartment geometry
  const QImage &getCompartmentImage() const;
};

class Membrane {
private:
  std::vector<std::pair<std::size_t, std::size_t>> indexPair;
  std::string id;
  const Compartment *compA;
  const Compartment *compB;
  QImage image;
  const std::vector<std::pair<QPoint, QPoint>> *pointPairs;

public:
  Membrane() = default;
  Membrane(std::string membraneId, const Compartment *A,
           const Compartment *B,
           const std::vector<std::pair<QPoint, QPoint>> *membranePairs);
  const std::string &getId() const;
  void setId(const std::string &membraneId);
  const Compartment *getCompartmentA() const;
  const Compartment *getCompartmentB() const;
  std::vector<std::pair<std::size_t, std::size_t>> &getIndexPairs();
  const std::vector<std::pair<std::size_t, std::size_t>> &getIndexPairs() const;
  const QImage &getImage() const;
};

class Field {
private:
  std::string id;
  const Compartment *comp;
  double diffusionConstant;
  QRgb colour;
  std::vector<double> conc;
  bool isUniformConcentration{true};
  bool isSpatial{true};

public:
  Field() = default;
  Field(const Compartment *compartment, std::string specID,
        double diffConst = 1.0, QRgb col = qRgb(255, 0, 0));
  const std::string &getId() const;
  QRgb getColour() const;
  void setColour(QRgb col);
  bool getIsSpatial() const;
  void setIsSpatial(bool spatial);
  bool getIsUniformConcentration() const;
  void setIsUniformConcentration(bool uniform);
  double getDiffusionConstant() const;
  void setDiffusionConstant(double diffConst);
  const Compartment *getCompartment() const;
  const std::vector<double> &getConcentration() const;
  void setConcentration(std::size_t index, double concentration);

  void setUniformConcentration(double concentration);
  void importConcentration(const std::vector<double> &sbmlConcentrationArray);
  QImage getConcentrationImage() const;
  std::vector<double> getConcentrationImageArray() const;
  void setCompartment(const Compartment *comp);
};

} // namespace geometry

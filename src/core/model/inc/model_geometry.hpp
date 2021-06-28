// SBML Model Geometry
//   - import geometry from SBML sampled field geometry

#pragma once

#include <QImage>
#include <QRgb>
#include <memory>
#include <string>
#include <vector>

#include "geometry.hpp"

namespace libsbml {
class Model;
}

namespace sme {

namespace mesh {
class Mesh;
}

namespace model {

class ModelCompartments;
class ModelMembranes;
struct Settings;

class ModelGeometry {
private:
  double pixelWidth{1.0};
  QPointF physicalOrigin{QPointF(0, 0)};
  QSizeF physicalSize{QSizeF(0, 0)};
  int numDimensions{3};
  QImage image;
  std::unique_ptr<mesh::Mesh> mesh;
  bool isValid{false};
  bool hasImage{false};
  libsbml::Model *sbmlModel{nullptr};
  ModelCompartments *modelCompartments{nullptr};
  ModelMembranes *modelMembranes{nullptr};
  Settings *sbmlAnnotation = nullptr;
  bool hasUnsavedChanges{false};
  int importDimensions(const libsbml::Model *model);
  void convertSBMLGeometryTo3d();
  void writeDefaultGeometryToSBML();

public:
  ModelGeometry();
  explicit ModelGeometry(libsbml::Model *model, ModelCompartments *compartments,
                         ModelMembranes *membranes, Settings *annotation);
  void importSampledFieldGeometry(const libsbml::Model *model);
  void importParametricGeometry(const libsbml::Model *model,
                                const Settings *settings);
  void importSampledFieldGeometry(const QString &filename);
  void importGeometryFromImage(const QImage &img);
  void updateMesh();
  void clear();
  int getNumDimensions() const;
  double getPixelWidth() const;
  void setPixelWidth(double width);
  const QPointF &getPhysicalOrigin() const;
  const QSizeF &getPhysicalSize() const;
  const QImage &getImage() const;
  mesh::Mesh *getMesh() const;
  bool getIsValid() const;
  bool getHasImage() const;
  void writeGeometryToSBML() const;
  bool getHasUnsavedChanges() const;
  void setHasUnsavedChanges(bool unsavedChanges);
};

} // namespace model

} // namespace sme

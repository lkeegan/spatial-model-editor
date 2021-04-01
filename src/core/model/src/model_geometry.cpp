#include "model_geometry.hpp"

#include <sbml/SBMLTypes.h>
#include <sbml/extension/SBMLDocumentPlugin.h>
#include <sbml/packages/spatial/common/SpatialExtensionTypes.h>
#include <sbml/packages/spatial/extension/SpatialExtension.h>

#include <memory>

#include "geometry_analytic.hpp"
#include "geometry_parametric.hpp"
#include "geometry_sampled_field.hpp"
#include "logger.hpp"
#include "mesh.hpp"
#include "model_compartments.hpp"
#include "model_membranes.hpp"
#include "sbml_utils.hpp"
#include "utils.hpp"
#include "xml_annotation.hpp"

namespace sme {

namespace model {

bool ModelGeometry::importDimensions(const libsbml::Model *model) {
  const auto *geom = getGeometry(model);
  if (geom == nullptr) {
    return false;
  }
  SPDLOG_INFO("Importing existing {}d SBML model geometry",
              geom->getNumCoordinateComponents());
  if (geom->getNumCoordinateComponents() != 2) {
    SPDLOG_WARN("Only 2d models are currently supported");
  }
  const auto *xcoord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X);
  if (xcoord == nullptr) {
    SPDLOG_ERROR("No x-coordinate found in SBML model");
    return false;
  }
  const auto *ycoord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y);
  if (ycoord == nullptr) {
    SPDLOG_ERROR("No y-coordinate found in SBML model");
    return false;
  }

  // import xy coordinates
  double xmin = xcoord->getBoundaryMin()->getValue();
  double xmax = xcoord->getBoundaryMax()->getValue();
  double ymin = ycoord->getBoundaryMin()->getValue();
  double ymax = ycoord->getBoundaryMax()->getValue();
  SPDLOG_INFO("  - found x range [{},{}]", xmin, xmax);
  SPDLOG_INFO("  - found y range [{},{}]", ymin, ymax);
  physicalOrigin = QPointF(xmin, ymin);
  SPDLOG_INFO("  -> origin [{},{}]", physicalOrigin.x(), physicalOrigin.y());
  physicalSize = QSizeF(xmax - xmin, ymax - ymin);
  SPDLOG_INFO("  -> size [{},{}]", physicalSize.width(), physicalSize.height());

  return true;
}

void ModelGeometry::writeDefaultGeometryToSBML() {
  SPDLOG_INFO("Creating new 2d SBML model geometry");
  numDimensions = 2;
  auto *spatial = static_cast<libsbml::SpatialModelPlugin *>(
      sbmlModel->getPlugin("spatial"));
  auto *geom = spatial->createGeometry();
  geom->setCoordinateSystem(
      libsbml::GeometryKind_t::SPATIAL_GEOMETRYKIND_CARTESIAN);
  for (int i = 0; i < numDimensions; ++i) {
    geom->createCoordinateComponent();
  }
  for (unsigned i = 0; i < sbmlModel->getNumCompartments(); ++i) {
    auto *comp = sbmlModel->getCompartment(i);
    comp->setSpatialDimensions(static_cast<unsigned int>(numDimensions));
  }

  // set-up xy coordinates
  auto *coordX = geom->getCoordinateComponent(0);
  coordX->setType(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X);
  coordX->setId("xCoord");
  auto *paramX = sbmlModel->createParameter();
  paramX->setId("x");
  paramX->setUnits(sbmlModel->getLengthUnits());
  paramX->setConstant(true);
  paramX->setValue(0);
  auto *ssr = static_cast<libsbml::SpatialParameterPlugin *>(
                  paramX->getPlugin("spatial"))
                  ->createSpatialSymbolReference();
  ssr->setSpatialRef(coordX->getId());
  SPDLOG_INFO("  - creating Parameter: {}", paramX->getId());
  SPDLOG_INFO("  - with spatialSymbolReference: {}", ssr->getSpatialRef());
  auto *minX = coordX->createBoundaryMin();
  minX->setId("xBoundaryMin");
  minX->setValue(0);
  auto *maxX = coordX->createBoundaryMax();
  maxX->setId("xBoundaryMax");
  maxX->setValue(pixelWidth * static_cast<double>(image.width()));
  SPDLOG_INFO("  - x in range [{},{}]", minX->getValue(), maxX->getValue());

  auto *coordY = geom->getCoordinateComponent(1);
  coordY->setType(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y);
  coordY->setId("yCoord");
  auto *paramY = sbmlModel->createParameter();
  paramY->setId("y");
  paramY->setUnits(sbmlModel->getLengthUnits());
  paramY->setConstant(true);
  paramY->setValue(0);
  ssr = static_cast<libsbml::SpatialParameterPlugin *>(
            paramY->getPlugin("spatial"))
            ->createSpatialSymbolReference();
  ssr->setSpatialRef(coordY->getId());
  SPDLOG_INFO("  - creating Parameter: {}", ssr->getId());
  SPDLOG_INFO("  - with spatialSymbolReference: {}", ssr->getSpatialRef());
  auto *minY = coordY->createBoundaryMin();
  minY->setId("yBoundaryMin");
  minY->setValue(0);
  auto *maxY = coordY->createBoundaryMax();
  maxY->setId("yBoundaryMax");
  maxY->setValue(pixelWidth * static_cast<double>(image.height()));
  SPDLOG_INFO("  - y in range [{},{}]", minY->getValue(), maxY->getValue());
  // the above overwrote any existing compartment geometry, so re-create
  // defaults here
  createDefaultCompartmentGeometryIfMissing(sbmlModel);
}

ModelGeometry::ModelGeometry() = default;

ModelGeometry::ModelGeometry(libsbml::Model *model,
                             ModelCompartments *compartments,
                             ModelMembranes *membranes)
    : sbmlModel{model}, modelCompartments{compartments}, modelMembranes{
                                                             membranes} {
  if (!importDimensions(model)) {
    SPDLOG_WARN("Failed to import geometry");
    writeDefaultGeometryToSBML();
    return;
  }
}

static double calculatePixelWidth(const QSize &imageSize,
                                  const QSizeF &physicalSize) {
  // calculate pixel size from image dimensions
  double xPixels = static_cast<double>(imageSize.width());
  double xPixelSize = physicalSize.width() / xPixels;
  double yPixels = static_cast<double>(imageSize.height());
  double yPixelSize = physicalSize.height() / yPixels;
  constexpr double maxRelativeDifference{1e-12};
  if (std::abs((xPixelSize - yPixelSize) / xPixelSize) >
      maxRelativeDifference) {
    SPDLOG_WARN("Pixels are not square: {} x {}", xPixelSize, yPixelSize);
  }
  return xPixelSize;
}

void ModelGeometry::importSampledFieldGeometry(const libsbml::Model *model) {
  importDimensions(model);
  auto gsf = importGeometryFromSampledField(getGeometry(model));
  if (gsf.image.isNull()) {
    SPDLOG_INFO(
        "No Sampled Field Geometry found - looking for Analytic Geometry...");
    gsf =
        importGeometryFromAnalyticGeometry(model, physicalOrigin, physicalSize);
    if (gsf.image.isNull()) {
      SPDLOG_INFO("No Analytic Geometry found");
      return;
    }
  }
  hasUnsavedChanges = true;
  SPDLOG_INFO("  - found {}x{} geometry image", gsf.image.width(),
              gsf.image.height());
  image = gsf.image.convertToFormat(QImage::Format_Indexed8);
  hasImage = true;
  pixelWidth = calculatePixelWidth(image.size(), physicalSize);
  setPixelWidth(pixelWidth);
  modelMembranes->updateCompartmentImage(image);
  for (const auto &[id, colour] : gsf.compartmentIdColourPairs) {
    SPDLOG_INFO("setting compartment {} colour to {:x}", id, colour);
    modelCompartments->setColour(id.c_str(), colour);
  }
  auto *geom = getOrCreateGeometry(sbmlModel);
  exportSampledFieldGeometry(geom, image);
}

void ModelGeometry::importParametricGeometry(const libsbml::Model *model) {
  auto importedMesh =
      importParametricGeometryFromSBML(model, this, modelCompartments);
  if (importedMesh != nullptr) {
    hasUnsavedChanges = true;
    mesh = std::move(importedMesh);
  }
}

void ModelGeometry::importSampledFieldGeometry(const QString &filename) {
  std::unique_ptr<libsbml::SBMLDocument> doc{
      libsbml::readSBMLFromFile(filename.toStdString().c_str())};
  importSampledFieldGeometry(doc->getModel());
}

void ModelGeometry::importGeometryFromImage(const QImage &img) {
  hasUnsavedChanges = true;
  for (const auto &id : modelCompartments->getIds()) {
    modelCompartments->setColour(id, 0);
  }
  constexpr auto flagNoDither = Qt::AvoidDither | Qt::ThresholdDither |
                                Qt::ThresholdAlphaDither |
                                Qt::NoOpaqueDetection;
  auto imgNoAlpha = img;
  if (img.hasAlphaChannel()) {
    SPDLOG_WARN("ignoring alpha channel");
    imgNoAlpha = img.convertToFormat(QImage::Format_RGB32, flagNoDither);
  }
  image = imgNoAlpha.convertToFormat(QImage::Format_Indexed8, flagNoDither);
  modelMembranes->updateCompartmentImage(image);
  auto *geom = getOrCreateGeometry(sbmlModel);
  exportSampledFieldGeometry(geom, image);
  hasImage = true;
}

void ModelGeometry::updateMesh() {
  // geometry only valid if all compartments have a colour
  isValid = hasImage && !modelCompartments->getColours().empty() && !modelCompartments->getColours().contains(0);
  if (!isValid) {
    mesh.reset();
    return;
  }
  hasUnsavedChanges = true;
  const auto &colours{modelCompartments->getColours()};
  const auto &ids{modelCompartments->getIds()};
  mesh = std::make_unique<mesh::Mesh>(image, std::vector<std::size_t>{},
                                      std::vector<std::size_t>{}, pixelWidth,
                                      physicalOrigin, utils::toStdVec(colours));
  for (int i = 0; i < ids.size(); ++i) {
    modelCompartments->setInteriorPoints(
        ids[i],
        mesh->getCompartmentInteriorPoints()[static_cast<std::size_t>(i)]);
  }
}

void ModelGeometry::clear() {
  mesh.reset();
  hasImage = false;
  isValid = false;
  image = {};
  auto *model = sbmlModel;
  hasUnsavedChanges = true;
  if (model == nullptr) {
    return;
  }
  // remove any SBML geometry-related stuff
  auto *geom = getOrCreateGeometry(model);
  removeMeshParamsAnnotation(getParametricGeometry(geom));
  for (unsigned i = 0; i < model->getNumCompartments(); ++i) {
    auto *comp = model->getCompartment(i);
    auto *scp = static_cast<libsbml::SpatialCompartmentPlugin *>(
        comp->getPlugin("spatial"));
    if (scp != nullptr && scp->isSetCompartmentMapping()) {
      scp->unsetCompartmentMapping();
    }
  }
  auto *spatial = static_cast<libsbml::SpatialModelPlugin *>(
      sbmlModel->getPlugin("spatial"));
  if (spatial != nullptr && spatial->isSetGeometry()) {
    auto *g = spatial->getGeometry();
    for (unsigned i = 0; i < g->getNumGeometryDefinitions(); ++i) {
      std::unique_ptr<libsbml::GeometryDefinition> gd(
          g->removeGeometryDefinition(i));
      SPDLOG_INFO("removing GeometryDefinition {}", gd->getId());
    }
    for (unsigned i = 0; i < g->getNumDomainTypes(); ++i) {
      std::unique_ptr<libsbml::DomainType> dt(g->removeDomainType(i));
      SPDLOG_INFO("removing DomainType {}", dt->getId());
    }
    for (unsigned i = 0; i < g->getNumDomains(); ++i) {
      std::unique_ptr<libsbml::Domain> d(g->removeDomain(i));
      SPDLOG_INFO("removing Domain {}", d->getId());
    }
    for (unsigned i = 0; i < g->getNumSampledFields(); ++i) {
      std::unique_ptr<libsbml::SampledField> sf(g->removeSampledField(i));
      SPDLOG_INFO("removing SampledField {}", sf->getId());
    }
  }
}

int ModelGeometry::getNumDimensions() const { return numDimensions; }

double ModelGeometry::getPixelWidth() const { return pixelWidth; }

void ModelGeometry::setPixelWidth(double width) {
  SPDLOG_INFO("Setting pixel width to {}", width);
  hasUnsavedChanges = true;
  double oldWidth = pixelWidth;
  pixelWidth = width;
  // update pixelWidth for each compartment
  for (const auto &id : modelCompartments->getIds()) {
    SPDLOG_ERROR("{}", id.toStdString());
    if (auto *compartment = modelCompartments->getCompartment(id);
        compartment != nullptr) {
      SPDLOG_ERROR("{:x}", reinterpret_cast<std::size_t>(static_cast<const void*>(compartment)));
      SPDLOG_ERROR("  -> {}", width);
      compartment->setPixelWidth(width);
    }
  }
  SPDLOG_INFO("New pixel width = {}", pixelWidth);

  auto *geom = getOrCreateGeometry(sbmlModel);
  // update compartment interior points
  for (const auto &compartmentId : modelCompartments->getIds()) {
    SPDLOG_INFO("  - compartmentId: {}", compartmentId.toStdString());
    auto *comp = sbmlModel->getCompartment(compartmentId.toStdString());
    auto *scp = static_cast<libsbml::SpatialCompartmentPlugin *>(
        comp->getPlugin("spatial"));
    const std::string &domainType =
        scp->getCompartmentMapping()->getDomainType();
    auto *domain = geom->getDomainByDomainType(domainType);
    if (domain != nullptr) {
      auto *interiorPoint = domain->getInteriorPoint(0);
      if (interiorPoint != nullptr) {
        double c1 = interiorPoint->getCoord1();
        double c2 = interiorPoint->getCoord2();
        double newc1 = c1 * width / oldWidth;
        double newc2 = c2 * width / oldWidth;
        SPDLOG_INFO("    -> rescaling interior point from ({},{}) to ({},{})",
                    c1, c2, newc1, newc2);
        interiorPoint->setCoord1(newc1);
        interiorPoint->setCoord2(newc2);
      }
    }
  }

  // update origin
  physicalOrigin *= width / oldWidth;
  SPDLOG_INFO("  - origin rescaled to ({},{})", physicalOrigin.x(),
              physicalOrigin.y());

  if (mesh != nullptr) {
    mesh->setPhysicalGeometry(width, physicalOrigin);
  }
  // update xy coordinates
  auto *coord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X);
  auto *min = coord->getBoundaryMin();
  auto *max = coord->getBoundaryMax();
  min->setValue(physicalOrigin.x());
  max->setValue(physicalOrigin.x() +
                pixelWidth * static_cast<double>(image.width()));
  SPDLOG_INFO("  - x now in range [{},{}]", min->getValue(), max->getValue());
  coord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y);
  min = coord->getBoundaryMin();
  max = coord->getBoundaryMax();
  min->setValue(physicalOrigin.y());
  max->setValue(physicalOrigin.y() +
                pixelWidth * static_cast<double>(image.height()));
  SPDLOG_INFO("  - y now in range [{},{}]", min->getValue(), max->getValue());
}

const QPointF &ModelGeometry::getPhysicalOrigin() const {
  return physicalOrigin;
}

const QSizeF &ModelGeometry::getPhysicalSize() const { return physicalSize; }

const QImage &ModelGeometry::getImage() const { return image; }

mesh::Mesh *ModelGeometry::getMesh() const { return mesh.get(); }

bool ModelGeometry::getIsValid() const { return isValid; }

bool ModelGeometry::getHasImage() const { return hasImage; }

void ModelGeometry::writeGeometryToSBML() const {
  writeGeometryMeshToSBML(sbmlModel, mesh.get(), *modelCompartments);
}

bool ModelGeometry::getHasUnsavedChanges() const { return hasUnsavedChanges; }

void ModelGeometry::setHasUnsavedChanges(bool unsavedChanges) {
  hasUnsavedChanges = unsavedChanges;
}

} // namespace model

} // namespace sme

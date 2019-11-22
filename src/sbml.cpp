#include "sbml.hpp"

#include <sbml/SBMLTypes.h>
#include <sbml/extension/SBMLDocumentPlugin.h>
#include <sbml/packages/spatial/common/SpatialExtensionTypes.h>
#include <sbml/packages/spatial/extension/SpatialExtension.h>

#include <unordered_set>

#include "logger.hpp"
#include "mesh.hpp"
#include "symbolic.hpp"
#include "utils.hpp"

namespace sbml {

static std::string ASTtoString(const libsbml::ASTNode *node) {
  std::unique_ptr<char, decltype(&std::free)> charAST(
      libsbml::SBML_formulaToL3String(node), &std::free);
  return charAST.get();
}

void SbmlDocWrapper::clearAllModelData() {
  compartments.clear();
  compartmentNames.clear();
  membranes.clear();
  species.clear();
  reactions.clear();
  functions.clear();
  mapSpeciesIdToColour.clear();
  mapCompIdToGeometry.clear();
  mapSpeciesIdToField.clear();
  membraneVec.clear();
  mapCompartmentToColour.clear();
  mapColourToCompartment.clear();
  mapMembraneToIndex.clear();
  mapMembraneToImage.clear();
  isValid = false;
  hasValidGeometry = false;
}

void SbmlDocWrapper::clearAllGeometryData() {
  membranes.clear();
  reactions.clear();
  mapCompIdToGeometry.clear();
  mapSpeciesIdToField.clear();
  membraneVec.clear();
  mapCompartmentToColour.clear();
  mapColourToCompartment.clear();
  membranePairs.clear();
  mapColPairToIndex.clear();
  mapMembraneToIndex.clear();
  mapMembraneToImage.clear();
  mesh = std::make_shared<mesh::Mesh>();
  hasGeometryImage = false;
  compartmentImage = {};
  hasValidGeometry = false;
}

SbmlDocWrapper::SbmlDocWrapper()
    : modelUnits(
          units::UnitVector{{{"hour", "h", "second", 0, 1, 3600},
                             {"minute", "m", "second", 0, 1, 60},
                             {"second", "s", "second", 0},
                             {"millisecond", "ms", "second", -3},
                             {"microsecond", "us", "second", -6}}},
          units::UnitVector{{{"metre", "m", "metre", 0},
                             {"decimetre", "dm", "metre", -1},
                             {"centimetre", "cm", "metre", -2},
                             {"millimetre", "mm", "metre", -3},
                             {"micrometre", "um", "metre", -6},
                             {"nanometre", "nm", "metre", -9}}},
          units::UnitVector{{{"litre", "L", "litre", 0},
                             {"decilitre", "dL", "litre", -1},
                             {"centilitre", "cL", "litre", -2},
                             {"millilitre", "mL", "litre", -3},
                             {"cubic metre", "m^3", "metre", 0, 3},
                             {"cubic decimetre", "dm^3", "metre", -1, 3},
                             {"cubic centimetre", "cm^3", "metre", -2, 3},
                             {"cubic millimetre", "mm^3", "metre", -3, 3}}},
          units::UnitVector{
              {{"mole", "mol", "mole", 0}, {"millimole", "mmol", "mole", -3}}}),
      mesh(std::make_shared<mesh::Mesh>()) {}

void SbmlDocWrapper::importSBMLString(const std::string &xml) {
  clearAllModelData();
  clearAllGeometryData();
  SPDLOG_INFO("Importing SBML from string...");
  doc.reset(libsbml::readSBMLFromString(xml.c_str()));
  initModelData();
}

void SbmlDocWrapper::importSBMLFile(const std::string &filename) {
  clearAllModelData();
  clearAllGeometryData();
  currentFilename = filename.c_str();
  SPDLOG_INFO("Loading SBML file {}...", filename);
  doc.reset(libsbml::readSBMLFromFile(filename.c_str()));
  initModelData();
}

void SbmlDocWrapper::writeDefaultGeometryToSBML() {
  SPDLOG_INFO("Creating new 2d SBML model geometry");
  geom = plugin->createGeometry();
  geom->setCoordinateSystem(
      libsbml::GeometryKind_t::SPATIAL_GEOMETRYKIND_CARTESIAN);
  for (std::size_t i = 0; i < nDimensions; ++i) {
    geom->createCoordinateComponent();
  }

  // set-up xy coordinates
  auto *coord = geom->getCoordinateComponent(0);
  coord->setType(libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X);
  coord->setId("xCoord");
  auto *param = model->createParameter();
  param->setId("x");
  param->setConstant(false);
  auto *ssr = dynamic_cast<libsbml::SpatialParameterPlugin *>(
                  param->getPlugin("spatial"))
                  ->createSpatialSymbolReference();
  ssr->setSpatialRef(coord->getId());
  SPDLOG_INFO("  - creating Parameter: {}", param->getId());
  SPDLOG_INFO("  - with spatialSymbolReference: {}", ssr->getSpatialRef());
  auto *min = coord->createBoundaryMin();
  min->setId("xBoundaryMin");
  min->setValue(0);
  auto *max = coord->createBoundaryMax();
  max->setId("xBoundaryMax");
  max->setValue(pixelWidth * static_cast<double>(compartmentImage.width()));
  SPDLOG_INFO("  - x in range [{},{}]", min->getValue(), max->getValue());

  coord = geom->getCoordinateComponent(1);
  coord->setType(libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y);
  coord->setId("yCoord");
  param = model->createParameter();
  param->setId("y");
  param->setConstant(false);
  ssr = dynamic_cast<libsbml::SpatialParameterPlugin *>(
            param->getPlugin("spatial"))
            ->createSpatialSymbolReference();
  ssr->setSpatialRef(coord->getId());
  SPDLOG_INFO("  - creating Parameter: {}", ssr->getId());
  SPDLOG_INFO("  - with spatialSymbolReference: {}", ssr->getSpatialRef());
  min = coord->createBoundaryMin();
  min->setId("yBoundaryMin");
  min->setValue(0);
  max = coord->createBoundaryMax();
  max->setId("yBoundaryMax");
  max->setValue(pixelWidth * static_cast<double>(compartmentImage.height()));
  SPDLOG_INFO("  - y in range [{},{}]", min->getValue(), max->getValue());

  // set isSpatial to true for all non-constant species
  for (unsigned i = 0; i < model->getNumSpecies(); ++i) {
    auto *spec = model->getSpecies(i);
    if (isSpeciesReactive(spec->getId())) {
      dynamic_cast<libsbml::SpatialSpeciesPlugin *>(spec->getPlugin("spatial"))
          ->setIsSpatial(true);
    }
  }

  // create sampled field geometry with empty SampledField
  sfgeom = geom->createSampledFieldGeometry();
  sfgeom->setId("sampledFieldGeometry");
  sfgeom->setIsActive(true);
  auto *sf = geom->createSampledField();
  sf->setId("geometryImage");
  sf->setDataType(libsbml::DataKind_t::SPATIAL_DATAKIND_UINT32);
  sf->setInterpolationType(
      libsbml::InterpolationKind_t::SPATIAL_INTERPOLATIONKIND_NEARESTNEIGHBOR);
  sf->setCompression(
      libsbml::CompressionKind_t::SPATIAL_COMPRESSIONKIND_UNCOMPRESSED);
  sfgeom->setSampledField(sf->getId());

  // for each compartment:
  //  - create DomainType
  //  - create CompartmentMapping from compartment to DomainType
  //  - create Domain with this DomainType
  //  - create SampledVolume with with DomainType (pixel geometry)
  for (unsigned i = 0; i < model->getNumCompartments(); ++i) {
    auto *comp = model->getCompartment(i);
    auto *scp = dynamic_cast<libsbml::SpatialCompartmentPlugin *>(
        comp->getPlugin("spatial"));
    const std::string &compartmentID = comp->getId();
    SPDLOG_INFO("  - compartment {}", compartmentID);
    auto *dt = geom->createDomainType();
    dt->setId(compartmentID + "_domainType");
    dt->setSpatialDimensions(static_cast<int>(nDimensions));
    SPDLOG_INFO("    * {}", dt->getId());
    auto *cmap = scp->createCompartmentMapping();
    cmap->setId(compartmentID + "_compartmentMapping");
    cmap->setDomainType(dt->getId());
    cmap->setUnitSize(1.0);
    SPDLOG_INFO("    * {}", cmap->getId());
    auto *dom = geom->createDomain();
    dom->setId(compartmentID + "_domain");
    dom->setDomainType(dt->getId());
    SPDLOG_INFO("    * {}", dom->getId());
    auto *sfvol = sfgeom->createSampledVolume();
    sfvol->setId(compartmentID + "_sampledVolume");
    sfvol->setDomainType(dt->getId());
    SPDLOG_INFO("    * {}", sfvol->getId());
  }
}

void SbmlDocWrapper::importSpatialData() {
  importGeometryDimensions();
  importSampledFieldGeometry();
  importParametricGeometry();
}

void SbmlDocWrapper::importGeometryDimensions() {
  geom = plugin->getGeometry();
  SPDLOG_INFO("Importing existing {}d SBML model geometry",
              geom->getNumCoordinateComponents());
  if (geom->getNumCoordinateComponents() != nDimensions) {
    SPDLOG_CRITICAL("Error: Only {}d models are currently supported",
                    nDimensions);
    // todo: offer to ignore spatial part of model & import anyway
  }
  // import xy coordinates
  const auto *xcoord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X);
  if (xcoord == nullptr) {
    SPDLOG_CRITICAL("Error: no x-coordinate found in SBML model");
  }
  SPDLOG_INFO("- found x range [{},{}]", xcoord->getBoundaryMin()->getValue(),
              xcoord->getBoundaryMax()->getValue());
  const auto *ycoord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y);
  if (ycoord == nullptr) {
    SPDLOG_CRITICAL("Error: no y-coordinate found in SBML model");
  }
  SPDLOG_INFO("- found y range [{},{}]", ycoord->getBoundaryMin()->getValue(),
              ycoord->getBoundaryMax()->getValue());
  origin = QPointF(xcoord->getBoundaryMin()->getValue(),
                   ycoord->getBoundaryMin()->getValue());
}

void SbmlDocWrapper::importSampledFieldGeometry() {
  // get sampled field geometry
  sfgeom = nullptr;
  for (unsigned i = 0; i < geom->getNumGeometryDefinitions(); ++i) {
    auto *def = geom->getGeometryDefinition(i);
    if (def->getIsActive() && def->isSampledFieldGeometry()) {
      sfgeom = dynamic_cast<libsbml::SampledFieldGeometry *>(def);
    }
  }
  if (sfgeom == nullptr) {
    SPDLOG_CRITICAL("Error: Failed to load sampled field geometry");
    qFatal(
        "SbmlDocWrapper::initModelData :: Error: Failed to load sampled field "
        "geometry");
  }

  // import geometry image
  auto *sf = geom->getSampledField(sfgeom->getSampledField());
  int xVals = sf->getNumSamples1();
  int yVals = sf->getNumSamples2();
  int totalVals = sf->getSamplesLength();
  auto samples = utils::stringToVector<QRgb>(sf->getSamples());
  if (static_cast<std::size_t>(totalVals) != samples.size()) {
    SPDLOG_WARN("Number of ints in string {} doesn't match samplesLength {}",
                samples.size(), totalVals);
  }
  // convert values into 2d pixmap
  // NOTE: order of samples is [ (x=0,y=0), (x=1,y=0), ... ]
  // NOTE: (0,0) point is at bottom-left
  // NOTE: QImage has (0,0) point at top-left, so flip y-coord here
  QImage img(xVals, yVals, QImage::Format_RGB32);
  auto iter = samples.begin();
  for (int y = 0; y < img.height(); ++y) {
    for (int x = 0; x < img.width(); ++x) {
      img.setPixel(x, img.height() - 1 - y, *iter);
      ++iter;
    }
  }
  SPDLOG_INFO("  - found {}x{} geometry image", img.size().width(),
              img.size().height());
  importGeometryFromImage(img, false);

  // calculate pixel size from image dimensions
  const auto *xcoord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X);
  double xPixels = static_cast<double>(compartmentImage.width());
  double xPixelSize = (xcoord->getBoundaryMax()->getValue() -
                       xcoord->getBoundaryMin()->getValue()) /
                      xPixels;
  const auto *ycoord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y);
  double yPixels = static_cast<double>(compartmentImage.height());
  double yPixelSize = (ycoord->getBoundaryMax()->getValue() -
                       ycoord->getBoundaryMin()->getValue()) /
                      yPixels;
  if (std::abs((xPixelSize - yPixelSize) / xPixelSize) > 1e-12) {
    SPDLOG_WARN("Pixels are not square: {} x {}", xPixelSize, yPixelSize);
  }
  pixelWidth = xPixelSize;
  SPDLOG_INFO("  - pixel size: {}", pixelWidth);

  // assign each compartment to a colour
  for (const auto &compartmentID : compartments) {
    auto *comp = model->getCompartment(compartmentID.toStdString());
    auto *scp = dynamic_cast<libsbml::SpatialCompartmentPlugin *>(
        comp->getPlugin("spatial"));
    if (scp->isSetCompartmentMapping()) {
      const std::string &domainTypeID =
          scp->getCompartmentMapping()->getDomainType();
      auto *sfvol = sfgeom->getSampledVolumeByDomainType(domainTypeID);
      QRgb col = static_cast<QRgb>(sfvol->getSampledValue());
      SPDLOG_INFO("setting compartment {} colour to {:x}",
                  compartmentID.toStdString(), col);
      SPDLOG_INFO("  - DomainType: {}", domainTypeID);
      SPDLOG_INFO("  - SampledFieldVolume: {}", sfvol->getId());
      setCompartmentColour(compartmentID, col, false);
    }
  }
}

void SbmlDocWrapper::importParametricGeometry() {
  // get sampled field geometry
  parageom = nullptr;
  for (unsigned i = 0; i < geom->getNumGeometryDefinitions(); ++i) {
    auto *def = geom->getGeometryDefinition(i);
    if (def->getIsActive() && def->isParametricGeometry()) {
      parageom = dynamic_cast<libsbml::ParametricGeometry *>(def);
    }
  }
  if (parageom == nullptr) {
    SPDLOG_WARN("Failed to load Parametric Field geometry");
    return;
  }

  auto interiorPoints = getInteriorPixelPoints();

  // get maxBoundaryPoints, maxTriangleAreas, membraneWidths
  if (parageom->isSetAnnotation()) {
    auto *node = parageom->getAnnotation();
    for (unsigned i = 0; i < node->getNumChildren(); ++i) {
      auto &child = node->getChild(i);
      if (child.getURI() == annotationURI &&
          child.getPrefix() == annotationPrefix && child.getName() == "mesh") {
        auto maxPoints = utils::stringToVector<std::size_t>(
            child.getAttrValue("maxBoundaryPoints", annotationURI));
        SPDLOG_INFO("  - maxBoundaryPoints: {}",
                    utils::vectorToString(maxPoints));
        auto maxAreas = utils::stringToVector<std::size_t>(
            child.getAttrValue("maxTriangleAreas", annotationURI));
        SPDLOG_INFO("  - maxTriangleAreas: {}",
                    utils::vectorToString(maxAreas));
        auto membraneWidths = utils::stringToVector<double>(
            child.getAttrValue("membraneWidths", annotationURI));
        SPDLOG_INFO("  - membraneWidths: {}",
                    utils::vectorToString(membraneWidths));
        // generate Mesh
        SPDLOG_INFO("  - re-generating mesh");
        mesh = std::make_shared<mesh::Mesh>(
            compartmentImage, interiorPoints, maxPoints, maxAreas,
            vecMembraneColourPairs, membraneWidths, pixelWidth, origin);
      }
    }
    return;
  }

  // If we didn't find mesh parameters generated by our code,
  // then the ParametricGeometry was created by some other software:
  // load it and use as a read-only mesh
  SPDLOG_INFO("loading as read-only mesh");
  // import vertices:
  int nVertices = parageom->getSpatialPoints()->getArrayDataLength();
  std::vector<double> vertices(static_cast<std::size_t>(nVertices), 0);
  parageom->getSpatialPoints()->getArrayData(vertices.data());
  SPDLOG_INFO("  - found {} vertices", nVertices / 2);
  // import triangles for each compartment
  std::vector<std::vector<int>> triangles;
  for (const auto &compartmentID : compartments) {
    auto *po = getParametricObject(compartmentID.toStdString());
    auto nPoints = static_cast<std::size_t>(po->getPointIndexLength());
    SPDLOG_INFO("  - compartment {}: found {} triangles",
                compartmentID.toStdString(), nPoints / 3);
    triangles.emplace_back(nPoints, 0);
    po->getPointIndex(triangles.back().data());
  }
  mesh = std::make_shared<mesh::Mesh>(vertices, triangles, interiorPoints);
}

void SbmlDocWrapper::initModelData() {
  if (doc->getErrorLog()->getNumErrors() > 0) {
    std::stringstream ss;
    doc->printErrors(ss);
    SPDLOG_INFO("SBML document warnings/errors:\n\n{}", ss.str());
  }
  if (doc->getErrorLog()->getNumFailsWithSeverity(libsbml::LIBSBML_SEV_ERROR) >
      0) {
    SPDLOG_WARN("Errors while reading SBML file (continuing anyway...)");
    isValid = true;
  } else {
    SPDLOG_INFO("Successfully imported SBML Level {}, Version {} model",
                doc->getLevel(), doc->getVersion());
    isValid = true;
  }

  // upgrade SBML document to latest version
  if (!(doc->getLevel() == libsbml::SBMLDocument::getDefaultLevel() &&
        doc->getVersion() == libsbml::SBMLDocument::getDefaultVersion())) {
    if (doc->setLevelAndVersion(libsbml::SBMLDocument::getDefaultLevel(),
                                libsbml::SBMLDocument::getDefaultVersion())) {
      SPDLOG_INFO("Successfully upgraded SBML model to Level {}, Version {}",
                  doc->getLevel(), doc->getVersion());
    } else {
      SPDLOG_CRITICAL(
          "Error - failed to upgrade SBML file (continuing anyway...)");
    }
    if (doc->getErrorLog()->getNumFailsWithSeverity(
            libsbml::LIBSBML_SEV_ERROR) > 0) {
      std::stringstream ss;
      doc->printErrors(ss);
      SPDLOG_WARN("SBML document errors:\n\n{}", ss.str());
    }
  }

  model = doc->getModel();

  // get list of compartments
  compartments.clear();
  compartmentNames.clear();
  species.clear();
  for (unsigned int i = 0; i < model->getNumCompartments(); ++i) {
    auto *comp = model->getCompartment(i);
    if (comp->getName().empty()) {
      // if compartment has no name, use Id
      SPDLOG_INFO("Compartment with Id {} has no Name, using Id",
                  comp->getId());
      comp->setName(comp->getId());
    }
    QString id = comp->getId().c_str();
    QString name = comp->getName().c_str();
    SPDLOG_TRACE("compartmentID: {}", id.toStdString());
    SPDLOG_TRACE("compartmentName: {}", name.toStdString());
    compartments.push_back(id);
    compartmentNames.push_back(name);
    species[id] = QStringList();
  }

  // get all species, make a list for each compartment
  for (unsigned int i = 0; i < model->getNumSpecies(); ++i) {
    auto *spec = model->getSpecies(i);
    if (spec->getName().empty()) {
      // if species has no name, use Id
      SPDLOG_INFO("Species with Id {} has no Name, using Id", spec->getId());
      spec->setName(spec->getId());
    }
    if (spec->isSetHasOnlySubstanceUnits() &&
        spec->getHasOnlySubstanceUnits()) {
      // equations expect amount, not concentration for this species
      // for now this is not supported:
      std::string errorMessage(
          "SbmlDocWrapper::importSBMLFile :: Error: "
          "HasOnlySubstanceUnits=true "
          "is not supported for spatial models.");
      SPDLOG_CRITICAL(errorMessage);
      qFatal("%s", errorMessage.c_str());
    }
    const auto id = spec->getId().c_str();
    species[spec->getCompartment().c_str()].push_back(QString(id));
    // assign a default colour for displaying the species
    mapSpeciesIdToColour[id] = utils::indexedColours()[i];
  }

  // get list of functions
  for (unsigned int i = 0; i < model->getNumFunctionDefinitions(); ++i) {
    auto *func = model->getFunctionDefinition(i);
    if (func->getName().empty()) {
      SPDLOG_INFO("Function with Id {} has no Name, using Id", func->getId());
      func->setName(func->getId());
    }
    functions.push_back(func->getId().c_str());
  }

  importTimeUnitsFromSBML(2);
  importLengthUnitsFromSBML(2);
  importVolumeUnitsFromSBML(3);
  importAmountUnitsFromSBML(1);

  if (!doc->isPackageEnabled("spatial")) {
    doc->enablePackage(libsbml::SpatialExtension::getXmlnsL3V1V1(), "spatial",
                       true);
    doc->setPackageRequired("spatial", true);
    SPDLOG_INFO("Enabling spatial extension");
  }

  plugin =
      dynamic_cast<libsbml::SpatialModelPlugin *>(model->getPlugin("spatial"));
  if (plugin == nullptr) {
    SPDLOG_ERROR("Failed to get SpatialModelPlugin from SBML document");
  }

  if (plugin->isSetGeometry()) {
    importSpatialData();
  } else {
    writeDefaultGeometryToSBML();
  }
}

void SbmlDocWrapper::exportSBMLFile(const std::string &filename) {
  if (isValid) {
    writeGeometryMeshToSBML();
    SPDLOG_INFO("Exporting SBML model to {}", filename);
    if (!libsbml::SBMLWriter().writeSBML(doc.get(), filename)) {
      SPDLOG_ERROR("Failed to write to {}", filename);
    }
  }
}

QString SbmlDocWrapper::getXml() {
  QString xml;
  if (!isValid) {
    return {};
  }
  writeGeometryMeshToSBML();
  doc->checkInternalConsistency();
  if (doc->getNumErrors() > 0) {
    std::stringstream ss;
    doc->printErrors(ss);
    SPDLOG_WARN("SBML internal consistency check warnings/errors:\n\n{}",
                ss.str());
  }
  doc->checkConsistency();
  if (doc->getNumErrors() > 0) {
    std::stringstream ss;
    doc->printErrors(ss);
    SPDLOG_WARN("SBML consistency check warnings/errors:\n\n{}", ss.str());
  }
  std::unique_ptr<char, decltype(&std::free)> xmlChar(
      libsbml::writeSBMLToString(doc.get()), &std::free);
  xml = QString(xmlChar.get());
  return xml;
}

void SbmlDocWrapper::importGeometryFromImage(const QImage &img,
                                             bool updateSBML) {
  clearAllGeometryData();
  compartmentImage = img;
  initMembraneColourPairs();
  if (isValid && updateSBML) {
    writeGeometryImageToSBML();
  }
  hasGeometryImage = true;
}

void SbmlDocWrapper::importGeometryFromImage(const QString &filename,
                                             bool updateSBML) {
  QImage img(filename);
  importGeometryFromImage(img, updateSBML);
}

void SbmlDocWrapper::writeGeometryImageToSBML() {
  // export image to SampledField
  auto *sf = geom->getSampledField(sfgeom->getSampledField());
  SPDLOG_INFO("Writing {}x{} geometry image to SampledField {}",
              compartmentImage.width(), compartmentImage.height(),
              sfgeom->getSampledField());
  sf->setNumSamples1(compartmentImage.width());
  sf->setNumSamples2(compartmentImage.height());
  sf->setSamplesLength(compartmentImage.width() * compartmentImage.height());

  std::vector<QRgb> samples;
  samples.reserve(static_cast<std::size_t>(compartmentImage.width() *
                                           compartmentImage.height()));
  // convert 2d pixmap into array of uints
  // NOTE: order of samples is [ (x=0,y=0), (x=1,y=0), ... ]
  // NOTE: (0,0) point is at bottom-left
  // NOTE: QImage has (0,0) point at top-left, so flip y-coord here
  for (int y = 0; y < compartmentImage.height(); ++y) {
    for (int x = 0; x < compartmentImage.width(); ++x) {
      samples.push_back(
          compartmentImage.pixel(x, compartmentImage.height() - 1 - y));
    }
  }
  std::string samplesString = utils::vectorToString(samples);
  sf->setSamples(samplesString);
  SPDLOG_INFO("SampledField '{}': assigned an array of length {}", sf->getId(),
              sf->getSamplesLength());
}

void SbmlDocWrapper::setFieldConcAnalytic(geometry::Field &field,
                                          const std::string &expr) {
  SPDLOG_INFO("expr: {}", expr);
  auto inlinedExpr = inlineExpr(expr);
  SPDLOG_INFO("  - inlined expr: {}", inlinedExpr);
  symbolic::Symbolic sym(inlinedExpr, {"x", "y"}, getGlobalConstants());
  SPDLOG_INFO("  - parsed expr: {}", sym.simplify());
  auto result = std::vector<double>(1, 0);
  auto vars = std::vector<double>(2, 0);
  for (std::size_t i = 0; i < field.geometry->ix.size(); ++i) {
    // position in pixels (with (0,0) in top-left of image):
    const auto &point = field.geometry->ix.at(i);
    // rescale to physical x,y point (with (0,0) in bottom-left):
    double minX =
        geom->getCoordinateComponentByKind(
                libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X)
            ->getBoundaryMin()
            ->getValue();
    vars[0] = minX + pixelWidth * static_cast<double>(point.x());
    double minY =
        geom->getCoordinateComponentByKind(
                libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y)
            ->getBoundaryMin()
            ->getValue();
    vars[1] =
        minY + pixelWidth * static_cast<double>(
                                field.geometry->getCompartmentImage().height() -
                                1 - point.y());
    sym.evalLLVM(result, vars);
    field.conc[i] = result[0];
  }
  field.init = field.conc;
  field.isUniformConcentration = false;
}

void SbmlDocWrapper::initMembraneColourPairs() {
  // convert geometry image to 8-bit indexed format:
  // each pixel points to an index in the colorTable
  // which contains an RGB value for each color in the image
  compartmentImage = compartmentImage.convertToFormat(QImage::Format_Indexed8);

  // for each pair of different colours: map to a continuous index
  // NOTE: colours ordered by ascending numerical value
  membranePairs.clear();
  mapColPairToIndex.clear();
  std::size_t i = 0;
  for (int iB = 1; iB < compartmentImage.colorCount(); ++iB) {
    for (int iA = 0; iA < iB; ++iA) {
      QRgb colA = compartmentImage.colorTable()[iA];
      QRgb colB = compartmentImage.colorTable()[iB];
      membranePairs.push_back(std::vector<std::pair<QPoint, QPoint>>());
      mapColPairToIndex[{std::min(colA, colB), std::max(colA, colB)}] = i++;
      SPDLOG_DEBUG("ColourPair iA,iB: {},{}: {:x},{:x}", iA, iB,
                   std::min(colA, colB), std::max(colA, colB));
    }
  }
  // for each pair of adjacent pixels of different colour,
  // add the pair of QPoints to the vector for this pair of colours,
  // i.e. the membrane between compartments of these two colours
  // NOTE: colour pairs are ordered in ascending order
  // x-neighbours
  for (int y = 0; y < compartmentImage.height(); ++y) {
    QRgb prevPix = compartmentImage.pixel(0, y);
    for (int x = 1; x < compartmentImage.width(); ++x) {
      QRgb currPix = compartmentImage.pixel(x, y);
      if (currPix != prevPix) {
        if (currPix < prevPix) {
          membranePairs[mapColPairToIndex.at({currPix, prevPix})].push_back(
              {QPoint(x, y), QPoint(x - 1, y)});
        } else {
          membranePairs[mapColPairToIndex.at({prevPix, currPix})].push_back(
              {QPoint(x - 1, y), QPoint(x, y)});
        }
      }
      prevPix = currPix;
    }
  }
  // y-neighbours
  for (int x = 0; x < compartmentImage.width(); ++x) {
    QRgb prevPix = compartmentImage.pixel(x, 0);
    for (int y = 1; y < compartmentImage.height(); ++y) {
      QRgb currPix = compartmentImage.pixel(x, y);
      if (currPix != prevPix) {
        if (currPix < prevPix) {
          membranePairs[mapColPairToIndex.at({currPix, prevPix})].push_back(
              {QPoint(x, y), QPoint(x, y - 1)});
        } else {
          membranePairs[mapColPairToIndex.at({prevPix, currPix})].push_back(
              {QPoint(x, y - 1), QPoint(x, y)});
        }
      }
      prevPix = currPix;
    }
  }
}

const QImage &SbmlDocWrapper::getMembraneImage(
    const QString &membraneID) const {
  if (!hasValidGeometry) {
    return compartmentImage;
  }
  return mapMembraneToImage.at(membraneID);
}

void SbmlDocWrapper::updateMembraneList() {
  // construct membrane list & images
  membranes.clear();
  membraneNames.clear();
  vecMembraneColourPairs.clear();
  mapMembraneToIndex.clear();
  mapMembraneToImage.clear();
  // clear vector of Membrane objects
  membraneVec.clear();
  // iterate over pairs of compartments
  for (int i = 1; i < compartments.size(); ++i) {
    for (int j = 0; j < i; ++j) {
      QRgb colA = mapCompartmentToColour[compartments[i]];
      QRgb colB = mapCompartmentToColour[compartments[j]];
      auto iter =
          mapColPairToIndex.find({std::min(colA, colB), std::max(colA, colB)});
      // if membrane for pair of colours exists, generate name and image, and
      // add to list
      if (iter != mapColPairToIndex.cend()) {
        std::size_t index = iter->second;
        if (!membranePairs[index].empty()) {
          // generate membrane name, compartments ordered by colour
          QString id = compartments[i] + "_" + compartments[j];
          QString name = compartmentNames[i] + " <-> " + compartmentNames[j];
          if (colA > colB) {
            id = compartments[j] + "_" + compartments[i];
            name = compartmentNames[j] + " <-> " + compartmentNames[i];
          }
          membranes.push_back(id);
          membraneNames.push_back(name);
          // also add the colour pairs for use by the mesh for membranes
          vecMembraneColourPairs.push_back(
              {id.toStdString(), {std::min(colA, colB), std::max(colA, colB)}});
          // map name to index of membrane location pairs
          mapMembraneToIndex[id] = index;
          // generate image
          QImage img = QImage(compartmentImage.size(),
                              QImage::Format_ARGB32_Premultiplied);
          img.fill(0);
          for (const auto &pair : membranePairs[index]) {
            img.setPixel(pair.first, std::min(colA, colB));
            img.setPixel(pair.second, std::max(colA, colB));
          }
          mapMembraneToImage[id] = img;
          // create Membrane object
          geometry::Compartment *compA =
              &mapCompIdToGeometry.at(compartments[i]);
          geometry::Compartment *compB =
              &mapCompIdToGeometry.at(compartments[j]);
          if (colA > colB) {
            std::swap(compA, compB);
          }
          assert(mapCompartmentToColour.at(compA->compartmentID.c_str()) <
                 mapCompartmentToColour.at(compB->compartmentID.c_str()));
          membraneVec.emplace_back(id.toStdString(), compA, compB,
                                   membranePairs[index]);
        }
      }
    }
  }
}

void SbmlDocWrapper::updateReactionList() {
  reactions.clear();
  for (const auto &container : {compartments, membranes}) {
    for (const auto &comp : container) {
      reactions[comp] = QStringList();
    }
  }
  for (unsigned int i = 0; i < model->getNumReactions(); ++i) {
    auto *reac = model->getReaction(i);
    auto *srp = dynamic_cast<libsbml::SpatialReactionPlugin *>(
        model->getReaction(i)->getPlugin("spatial"));
    if (reac->getName().empty()) {
      SPDLOG_INFO("Reaction with Id {} has no Name, using Id", reac->getId());
      reac->setName(reac->getId());
    }
    QString reacID = reac->getId().c_str();
    // construct the set of compartments where reaction takes place
    std::unordered_set<std::string> comps;
    for (unsigned int k = 0; k < reac->getNumProducts(); ++k) {
      const std::string &specID = reac->getProduct(k)->getSpecies();
      comps.insert(model->getSpecies(specID)->getCompartment());
    }
    for (unsigned int k = 0; k < reac->getNumReactants(); ++k) {
      const std::string &specID = reac->getReactant(k)->getSpecies();
      comps.insert(model->getSpecies(specID)->getCompartment());
    }
    for (unsigned int k = 0; k < reac->getNumModifiers(); ++k) {
      const std::string &specID = reac->getModifier(k)->getSpecies();
      comps.insert(model->getSpecies(specID)->getCompartment());
    }
    // single compartment
    if (comps.size() == 1) {
      QString comp = comps.begin()->c_str();
      reactions[comp].push_back(reacID);
      // if reaction `isLocal` is not set, then the reaction rate is for
      // amount, so here we divide the reaction formula by compartment factor
      // to convert it to concentration, and set `isLocal` to true
      if (!srp->isSetIsLocal()) {
        SPDLOG_INFO("Reaction {} takes place in compartment {}", reac->getId(),
                    comp.toStdString());
        SPDLOG_INFO(
            "  - isLocal has not been set, so dividing reaction rate by "
            "compartment volume:");
        auto expr = ASTtoString(reac->getKineticLaw()->getMath());
        SPDLOG_INFO("  - {}", expr);
        auto newExpr = symbolic::divide(expr, comp.toStdString());
        SPDLOG_INFO("  --> {}", newExpr);
        std::unique_ptr<libsbml::ASTNode> argAST(
            libsbml::SBML_parseL3Formula(newExpr.c_str()));
        if (argAST != nullptr) {
          reac->getKineticLaw()->setMath(argAST.get());
          SPDLOG_INFO("  - new math: {}",
                      ASTtoString(reac->getKineticLaw()->getMath()));
          SPDLOG_INFO("  - setting isLocal to true");
          srp->setIsLocal(true);
        } else {
          SPDLOG_ERROR("  - libSBML failed to parse expression");
        }
        reac->setCompartment(comp.toStdString());
      }
    } else if (comps.size() == 2) {
      auto iter = comps.cbegin();
      QString compA = iter->c_str();
      ++iter;
      QString compB = iter->c_str();
      // two compartments: want the membrane between them
      // membrane name is compA-compB, ordered by colour
      QRgb colA = mapCompartmentToColour[compA];
      QRgb colB = mapCompartmentToColour[compB];
      // check that compartments map to colours - if not do nothing
      if (colA != 0 && colB != 0) {
        QString membraneID = compA + "_" + compB;
        if (colA > colB) {
          membraneID = compB + "_" + compA;
        }
        reactions[membraneID].push_back(QString(reac->getId().c_str()));
        if (!srp->isSetIsLocal()) {
          SPDLOG_INFO("Reaction {} takes place on the {} membrane",
                      reac->getId(), membraneID.toStdString());
          SPDLOG_INFO(
              "  - isLocal has not been set, so dividing reaction rate by "
              "membrane area:");
          auto expr = ASTtoString(reac->getKineticLaw()->getMath());
          auto nPixels =
              membranePairs.at(mapMembraneToIndex.at(membraneID)).size();
          SPDLOG_INFO("  - membrane has {} pixels", nPixels);
          // model has unit height in z-direction
          double membraneArea = pixelWidth * 1.0 * static_cast<double>(nPixels);
          SPDLOG_INFO("  - pixelWidth {}", pixelWidth);
          SPDLOG_INFO("  - membrane Area {}", membraneArea);
          auto newExpr = symbolic::divide(expr, std::to_string(membraneArea));
          SPDLOG_INFO("  --> {}", newExpr);
          std::unique_ptr<libsbml::ASTNode> argAST(
              libsbml::SBML_parseL3Formula(newExpr.c_str()));
          if (argAST != nullptr) {
            reac->getKineticLaw()->setMath(argAST.get());
            SPDLOG_INFO("  - new math: {}",
                        ASTtoString(reac->getKineticLaw()->getMath()));
            SPDLOG_INFO("  - setting isLocal to true");
            srp->setIsLocal(true);
          } else {
            SPDLOG_ERROR("  - libSBML failed to parse expression");
          }
          reac->setCompartment(compA.toStdString());
        }
      }
    } else {
      // invalid reaction: number of compartments for reaction must be 1 or 2
      SPDLOG_ERROR("Reaction involves {} compartments - not supported",
                   comps.size());
      exit(1);
    }
  }
}

const units::ModelUnits &SbmlDocWrapper::getModelUnits() const {
  return modelUnits;
}

const QImage &SbmlDocWrapper::getCompartmentImage() const {
  return compartmentImage;
}

double SbmlDocWrapper::getCompartmentSize(const QString &compartmentID) const {
  return model->getCompartment(compartmentID.toStdString())->getSize();
}

// returns UnitDef with supplied Id, or if it doesn't exist,
// creates one with Id=defaultId and returns it
libsbml::UnitDefinition *SbmlDocWrapper::getOrCreateUnitDef(
    const std::string &Id, const std::string &defaultId) {
  libsbml::UnitDefinition *unitdef = nullptr;
  unitdef = model->getUnitDefinition(Id);
  if (unitdef == nullptr) {
    // if no existing unitdef, create one
    std::string newId = defaultId;
    // ensure it is unique among unitdef Ids
    // note: they are in a different namespace to other SBML objects
    while (model->getUnitDefinition(newId) != nullptr) {
      newId.append("_");
      SPDLOG_DEBUG("  -> {}", newId);
    }
    SPDLOG_DEBUG("creating UnitDefinition {}", newId);
    unitdef = model->createUnitDefinition();
    unitdef->setId(newId);
    unitdef->setName(defaultId);
  }
  return unitdef;
}

static void setSBMLUnitDef(libsbml::UnitDefinition *unitdef,
                           const units::Unit &u) {
  if (unitdef == nullptr) {
    return;
  }
  unitdef->setName(u.name.toStdString());
  unitdef->getListOfUnits()->clear();
  auto *unit = unitdef->createUnit();
  unit->setKind(libsbml::UnitKind_forName(u.kind.toStdString().c_str()));
  unit->setMultiplier(u.multiplier);
  unit->setScale(u.scale);
  unit->setExponent(u.exponent);
  return;
}

void SbmlDocWrapper::setUnitsTimeIndex(int index) {
  modelUnits.setTime(index);
  auto *unitdef = getOrCreateUnitDef(model->getTimeUnits(), "unit_of_time");
  model->setTimeUnits(unitdef->getId());
  setSBMLUnitDef(unitdef, modelUnits.getTime());
}

void SbmlDocWrapper::setUnitsLengthIndex(int index) {
  modelUnits.setLength(index);
  auto *unitdef = getOrCreateUnitDef(model->getLengthUnits(), "unit_of_length");
  model->setLengthUnits(unitdef->getId());
  setSBMLUnitDef(unitdef, modelUnits.getLength());

  // also set units of area as length^2
  unitdef = getOrCreateUnitDef(model->getAreaUnits(), "unit_of_area");
  auto u = modelUnits.getLength();
  u.name.append(" squared");
  u.exponent *= 2;
  model->setAreaUnits(unitdef->getId());
  setSBMLUnitDef(unitdef, u);
}

void SbmlDocWrapper::setUnitsVolumeIndex(int index) {
  modelUnits.setVolume(index);
  auto *unitdef = getOrCreateUnitDef(model->getVolumeUnits(), "unit_of_volume");
  model->setVolumeUnits(unitdef->getId());
  setSBMLUnitDef(unitdef, modelUnits.getVolume());
}

void SbmlDocWrapper::setUnitsAmountIndex(int index) {
  modelUnits.setAmount(index);
  auto *unitdef =
      getOrCreateUnitDef(model->getSubstanceUnits(), "units_of_substance");
  model->setSubstanceUnits(unitdef->getId());
  model->setExtentUnits(unitdef->getId());
  setSBMLUnitDef(unitdef, modelUnits.getAmount());
}

static std::optional<int> getUnitIndex(libsbml::Model *model,
                                       const std::string &id,
                                       const QVector<units::Unit> &units) {
  SPDLOG_INFO("SId: {}", id);
  // by default assume id is a SBML base unit
  std::string kind = id;
  double multiplier = 1.0;
  int exponent = 1;
  int scale = 0;
  const auto *unitdef = model->getUnitDefinition(id);
  if (unitdef != nullptr && unitdef->getNumUnits() == 1) {
    // if id is a UnitDefinition, then get unit kind & scaling factors
    const auto *unit = unitdef->getUnit(0);
    kind = libsbml::UnitKind_toString(unit->getKind());
    multiplier = unit->getMultiplier();
    exponent = unit->getExponent();
    scale = unit->getScale();
  }
  SPDLOG_INFO("  = ({} * 1e{} {})^{}", multiplier, scale, kind, exponent);
  for (int i = 0; i < units.size(); ++i) {
    const auto &u = units.at(i);
    if (u.kind.toStdString() == kind &&
        std::fabs((u.multiplier - multiplier) / multiplier) < 1e-10 &&
        u.exponent == exponent && u.scale == scale) {
      SPDLOG_INFO("  -> {}", u.name.toStdString());
      return i;
    }
  }
  SPDLOG_WARN("  -> matching unit not found");
  return {};
}

void SbmlDocWrapper::importTimeUnitsFromSBML(int defaultUnitIndex) {
  SPDLOG_INFO("SId {}:", model->getTimeUnits());
  auto uIndex =
      getUnitIndex(model, model->getTimeUnits(), modelUnits.getTimeUnits());
  setUnitsTimeIndex(uIndex.value_or(defaultUnitIndex));
  SPDLOG_INFO("  -> {}", modelUnits.getTime().name.toStdString());
  return;
}

void SbmlDocWrapper::importLengthUnitsFromSBML(int defaultUnitIndex) {
  SPDLOG_INFO("SId {}:", model->getLengthUnits());
  auto uIndex =
      getUnitIndex(model, model->getLengthUnits(), modelUnits.getLengthUnits());
  setUnitsLengthIndex(uIndex.value_or(defaultUnitIndex));
  SPDLOG_INFO("  -> {}", modelUnits.getLength().name.toStdString());
  return;
}

void SbmlDocWrapper::importVolumeUnitsFromSBML(int defaultUnitIndex) {
  SPDLOG_INFO("SId {}:", model->getVolumeUnits());
  auto uIndex =
      getUnitIndex(model, model->getVolumeUnits(), modelUnits.getVolumeUnits());
  setUnitsVolumeIndex(uIndex.value_or(defaultUnitIndex));
  SPDLOG_INFO("  -> {}", modelUnits.getVolume().name.toStdString());
  return;
}

void SbmlDocWrapper::importAmountUnitsFromSBML(int defaultUnitIndex) {
  SPDLOG_INFO("SId {}:", model->getSubstanceUnits());
  auto uIndex = getUnitIndex(model, model->getSubstanceUnits(),
                             modelUnits.getAmountUnits());
  setUnitsAmountIndex(uIndex.value_or(defaultUnitIndex));
  SPDLOG_INFO("  -> {}", modelUnits.getAmount().name.toStdString());
  return;
}

double SbmlDocWrapper::getSpeciesCompartmentSize(
    const QString &speciesID) const {
  QString compID =
      model->getSpecies(speciesID.toStdString())->getCompartment().c_str();
  return getCompartmentSize(compID);
}

SpeciesGeometry SbmlDocWrapper::getSpeciesGeometry(
    const QString &speciesID) const {
  return {getCompartmentImage().size(),
          mapSpeciesIdToField.at(speciesID).geometry->ix, getOrigin(),
          getPixelWidth(), getModelUnits()};
}

QString SbmlDocWrapper::getCompartmentID(QRgb colour) const {
  auto iter = mapColourToCompartment.find(colour);
  if (iter == mapColourToCompartment.cend()) {
    return "";
  }
  return iter->second;
}

QRgb SbmlDocWrapper::getCompartmentColour(const QString &compartmentID) const {
  auto iter = mapCompartmentToColour.find(compartmentID);
  if (iter == mapCompartmentToColour.cend()) {
    return 0;
  }
  return iter->second;
}

void SbmlDocWrapper::createField(const QString &speciesID,
                                 const QString &compartmentID) {
  std::string sId = speciesID.toStdString();
  SPDLOG_INFO("creating field for species {} in compartment {}", sId,
              compartmentID.toStdString());
  geometry::Compartment &comp = mapCompIdToGeometry.at(compartmentID);
  mapSpeciesIdToField[speciesID] =
      geometry::Field(&comp, sId, 1.0, mapSpeciesIdToColour.at(speciesID));
  geometry::Field &field = mapSpeciesIdToField.at(speciesID);
  // set all species concentrations to their initial values
  const auto *spec = model->getSpecies(sId);
  // start by setting the uniform concentration
  field.setUniformConcentration(spec->getInitialConcentration());
  // if sampled field or analytic are present, they override the above
  if (auto sf = getSpeciesSampledFieldInitialAssignment(sId); !sf.empty()) {
    auto arr = getSampledFieldConcentration(speciesID);
    field.importConcentration(arr);
  } else if (auto expr = getAnalyticConcentration(speciesID); !expr.isEmpty()) {
    setFieldConcAnalytic(field, expr.toStdString());
  }
  // set isSpatial flag
  const auto *ssp = dynamic_cast<const libsbml::SpatialSpeciesPlugin *>(
      spec->getPlugin("spatial"));
  bool isSpatial =
      (ssp != nullptr && ssp->isSetIsSpatial() && ssp->getIsSpatial());
  field.isSpatial = isSpatial;
  // set diffusion constant
  for (unsigned i = 0; i < model->getNumParameters(); ++i) {
    const auto *param = model->getParameter(i);
    if (const auto *spp = dynamic_cast<const libsbml::SpatialParameterPlugin *>(
            param->getPlugin("spatial"));
        (spp != nullptr) && spp->isSetDiffusionCoefficient() &&
        (spp->getDiffusionCoefficient()->getVariable() == sId)) {
      field.diffusionConstant = param->getValue();
      break;
    }
  }
}

void SbmlDocWrapper::setCompartmentColour(const QString &compartmentID,
                                          QRgb colour, bool updateSBML) {
  SPDLOG_INFO("assigning colour {:x} to compartment {}", colour,
              compartmentID.toStdString());
  // todo: add check that colour exists in geometry image?
  QRgb oldColour = getCompartmentColour(compartmentID);
  if (oldColour != 0) {
    SPDLOG_INFO(
        "setting previous compartment colour {} to point to null compartment",
        oldColour);
    // if there was already a colour mapped to this compartment, map it to a
    // null compartment
    mapColourToCompartment[oldColour] = "";
  }
  auto oldCompartmentID = getCompartmentID(colour);
  if (oldCompartmentID != "") {
    SPDLOG_INFO("colour {:x} used to point to compartment to {}: removing",
                colour, oldCompartmentID.toStdString());
    // if the new colour was already mapped to another compartment, set the
    // colour of that compartment to null
    mapCompartmentToColour[oldCompartmentID] = 0;
  }
  mapColourToCompartment[colour] = compartmentID;
  mapCompartmentToColour[compartmentID] = colour;
  // create compartment geometry for this colour
  mapCompIdToGeometry[compartmentID] = geometry::Compartment(
      compartmentID.toStdString(), getCompartmentImage(), colour);
  // create a field for each species in this compartment
  for (const auto &speciesID : species[compartmentID]) {
    createField(speciesID, compartmentID);
  }
  // update list of possible inter-compartment membranes
  updateMembraneList();
  // update list of reactions for each compartment/membrane
  updateReactionList();

  hasValidGeometry = std::none_of(
      compartments.cbegin(), compartments.cend(),
      [this](const auto &c) { return getCompartmentColour(c) == 0; });

  // update mesh
  updateMesh();

  // update all compartment mappings in SBML
  // (don't want to do this when importing an SBML file)
  if (updateSBML) {
    for (unsigned int i = 0; i < model->getNumCompartments(); ++i) {
      const std::string &compID = model->getCompartment(i)->getId();
      // set SampledValue (aka colour) of SampledFieldVolume
      auto *scp = dynamic_cast<libsbml::SpatialCompartmentPlugin *>(
          model->getCompartment(compID)->getPlugin("spatial"));
      const std::string &domainType =
          scp->getCompartmentMapping()->getDomainType();
      auto *sfvol = sfgeom->getSampledVolumeByDomainType(domainType);
      sfvol->setSampledValue(
          static_cast<double>(getCompartmentColour(compID.c_str())));
    }
  }
}

void SbmlDocWrapper::updateMesh() {
  auto interiorPoints = getInteriorPixelPoints();
  if (interiorPoints.empty()) {
    SPDLOG_DEBUG(
        "some compartments are missing interiorPoint: skip mesh update");
    return;
  }
  SPDLOG_INFO("Updating mesh interior points");
  mesh = std::make_shared<mesh::Mesh>(
      compartmentImage, interiorPoints, std::vector<std::size_t>{},
      std::vector<std::size_t>{}, vecMembraneColourPairs, std::vector<double>{},
      pixelWidth, origin);
}

libsbml::ParametricObject *SbmlDocWrapper::getParametricObject(
    const std::string &compartmentID) const {
  // NB: parageom->getParametricObjectByDomainType returned null when compiled
  // with gcc9 - have not understood why, for now just avoiding using it
  const auto *comp = model->getCompartment(compartmentID);
  const auto *scp = dynamic_cast<const libsbml::SpatialCompartmentPlugin *>(
      comp->getPlugin("spatial"));
  const std::string &domainTypeID =
      scp->getCompartmentMapping()->getDomainType();
  for (unsigned j = 0; j < parageom->getNumParametricObjects(); ++j) {
    auto *p = parageom->getParametricObject(j);
    if (p->getDomainType() == domainTypeID) {
      return p;
    }
  }
  return nullptr;
}

void SbmlDocWrapper::writeMeshParamsAnnotation(
    libsbml::ParametricGeometry *pg) {
  // if there is already an annotation set by us, remove it
  if (pg->isSetAnnotation()) {
    auto *node = pg->getAnnotation();
    for (unsigned i = 0; i < node->getNumChildren(); ++i) {
      auto &child = node->getChild(i);
      if (child.getURI() == annotationURI &&
          child.getPrefix() == annotationPrefix && child.getName() == "mesh") {
        SPDLOG_INFO("removing annotation {} : {}", i, node->toXMLString());
        // Note: removeChild returns a pointer to the child,
        // and removes this pointer from the SBML doc,
        // but does not delete the Child,
        // so now we own it and it will be deleted when
        // this unique pointer goes out of scope
        std::unique_ptr<libsbml::XMLNode> childNode(node->removeChild(i));
        break;
      }
    }
  }
  // append annotation with mesh info
  std::string xml = "<";
  xml.append(annotationPrefix);
  xml.append(":mesh xmlns:");
  xml.append(annotationPrefix);
  xml.append("=\"");
  xml.append(annotationURI);
  xml.append("\" ");
  xml.append(annotationPrefix);
  xml.append(":maxBoundaryPoints=\"");
  xml.append(utils::vectorToString(mesh->getBoundaryMaxPoints()));
  xml.append("\" ");
  xml.append(annotationPrefix);
  xml.append(":maxTriangleAreas=\"");
  xml.append(utils::vectorToString(mesh->getCompartmentMaxTriangleArea()));
  xml.append("\" ");
  xml.append(annotationPrefix);
  xml.append(":membraneWidths=\"");
  xml.append(utils::vectorToString(mesh->getBoundaryWidths()));
  xml.append("\"/>");
  pg->appendAnnotation(xml);
  SPDLOG_INFO("appending annotation: {}", xml);
}

void SbmlDocWrapper::writeGeometryMeshToSBML() {
  if (mesh->getVertices().empty()) {
    SPDLOG_INFO("No mesh to export to SBML");
    return;
  }
  if (parageom == nullptr) {
    SPDLOG_INFO("No ParametricGeometry found, creating...");
    parageom = geom->createParametricGeometry();
    parageom->setId("parametricGeometry");
    parageom->setIsActive(true);
    auto *sp = parageom->createSpatialPoints();
    sp->setId("spatialPoints");
    sp->setDataType(libsbml::DataKind_t::SPATIAL_DATAKIND_DOUBLE);
    sp->setCompression(
        libsbml::CompressionKind_t::SPATIAL_COMPRESSIONKIND_UNCOMPRESSED);
    for (unsigned i = 0; i < model->getNumCompartments(); ++i) {
      auto *comp = model->getCompartment(i);
      auto *scp = dynamic_cast<libsbml::SpatialCompartmentPlugin *>(
          comp->getPlugin("spatial"));
      const std::string &compartmentID = comp->getId();
      SPDLOG_INFO("  - compartment {}", compartmentID);
      std::string domainTypeID = scp->getCompartmentMapping()->getDomainType();
      SPDLOG_INFO("  - domainType {}", domainTypeID);
      auto *po = parageom->createParametricObject();
      po->setId(compartmentID + "_triangles");
      po->setPolygonType(libsbml::PolygonKind_t::SPATIAL_POLYGONKIND_TRIANGLE);
      po->setDomainType(domainTypeID);
      po->setDataType(libsbml::DataKind_t::SPATIAL_DATAKIND_UINT32);
      po->setCompression(
          libsbml::CompressionKind_t::SPATIAL_COMPRESSIONKIND_UNCOMPRESSED);
      SPDLOG_INFO("  - parametricObject {}", po->getId());
      SPDLOG_INFO("  - domainType {}", po->getDomainType());
    }
  }

  if (!mesh->isReadOnly()) {
    // if we constructed the mesh, add the parameters required
    // to reconstruct it from the geometry image as an annotation
    writeMeshParamsAnnotation(parageom);
  }

  // write vertices
  std::vector<double> vertices = mesh->getVertices();
  auto *sp = parageom->getSpatialPoints();
  int sz = static_cast<int>(vertices.size());
  sp->setArrayData(vertices.data(), sz);
  sp->setArrayDataLength(sz);
  SPDLOG_INFO("  - added {} doubles ({} vertices)", sz, sz / 2);

  SPDLOG_INFO(" Writing mesh triangles:");
  // write mesh triangles for each compartment
  for (int i = 0; i < compartments.size(); ++i) {
    auto compartmentID = compartments[i].toStdString();
    SPDLOG_INFO("  - compartment {}", compartmentID);
    auto *po = getParametricObject(compartmentID);
    if (po == nullptr) {
      SPDLOG_CRITICAL("    - no parametricObject found");
    }
    SPDLOG_INFO("    - parametricObject: {}", po->getId());
    std::vector<int> triangleInts =
        mesh->getTriangleIndices(static_cast<std::size_t>(i));
    int size = static_cast<int>(triangleInts.size());
    po->setPointIndexLength(size);
    po->setPointIndex(triangleInts.data(), size);
    SPDLOG_INFO("    - added {} uints ({} triangles)", size, size / 2);
  }
  return;
}

std::vector<QPointF> SbmlDocWrapper::getInteriorPixelPoints() const {
  // get interiorPoints in terms of physical location
  // & convert them to integer pixel points
  // if any interior points are missing: return an empty vector
  std::vector<QPointF> interiorPoints;
  for (const auto &compartmentID : compartments) {
    SPDLOG_DEBUG("Found interior point:");
    auto interiorFloatPhysical = getCompartmentInteriorPoint(compartmentID);
    if (!interiorFloatPhysical) {
      return {};
    }
    SPDLOG_DEBUG("  - physical location: ({},{})",
                 interiorFloatPhysical.value().x(),
                 interiorFloatPhysical.value().y());
    QPointF interiorFloatPixel =
        (interiorFloatPhysical.value() - origin) / pixelWidth;
    SPDLOG_DEBUG("  - pixel location: ({},{})", interiorFloatPixel.x(),
                 interiorFloatPixel.y());
    interiorPoints.push_back(interiorFloatPixel);
  }
  return interiorPoints;
}

std::optional<QPointF> SbmlDocWrapper::getCompartmentInteriorPoint(
    const QString &compartmentID) const {
  SPDLOG_INFO("compartmentID: {}", compartmentID.toStdString());
  auto *comp = model->getCompartment(compartmentID.toStdString());
  auto *scp = dynamic_cast<libsbml::SpatialCompartmentPlugin *>(
      comp->getPlugin("spatial"));
  const std::string &domainType = scp->getCompartmentMapping()->getDomainType();
  SPDLOG_INFO("  - domainType: {}", domainType);
  auto *domain = geom->getDomainByDomainType(domainType);
  SPDLOG_INFO("  - domain: {}", domain->getId());
  SPDLOG_INFO("  - numInteriorPoints: {}", domain->getNumInteriorPoints());
  if (domain->getNumInteriorPoints() == 0) {
    SPDLOG_INFO("  - no interior point found");
    return {};
  }
  auto *interiorPoint = domain->getInteriorPoint(0);
  QPointF point(interiorPoint->getCoord1(), interiorPoint->getCoord2());
  SPDLOG_INFO("  - interior point ({},{})", point.x(), point.y());
  return point;
}

void SbmlDocWrapper::setCompartmentInteriorPoint(const QString &compartmentID,
                                                 const QPointF &point) {
  SPDLOG_INFO("compartmentID: {}", compartmentID.toStdString());
  SPDLOG_INFO("  - setting interior pixel point ({},{})", point.x(), point.y());
  auto *comp = model->getCompartment(compartmentID.toStdString());
  auto *scp = dynamic_cast<libsbml::SpatialCompartmentPlugin *>(
      comp->getPlugin("spatial"));
  const std::string &domainType = scp->getCompartmentMapping()->getDomainType();
  SPDLOG_INFO("  - domainType: {}", domainType);
  auto *domain = geom->getDomainByDomainType(domainType);
  SPDLOG_INFO("  - domain: {}", domain->getId());
  auto *interiorPoint = domain->getInteriorPoint(0);
  if (interiorPoint == nullptr) {
    SPDLOG_INFO("  - creating new interior point");
    interiorPoint = domain->createInteriorPoint();
  }
  // convert from QPoint with (0,0) in top-left to (0,0) in bottom-left
  // and to physical units with pixelWidth and origin
  interiorPoint->setCoord1(origin.x() + pixelWidth * point.x());
  interiorPoint->setCoord2(
      origin.y() + pixelWidth * (compartmentImage.height() - 1 - point.y()));
  // update mesh with new interior point
  updateMesh();
}

void SbmlDocWrapper::removeInitialAssignment(const std::string &speciesID) {
  auto sampledFieldID = getSpeciesSampledFieldInitialAssignment(speciesID);
  if (!sampledFieldID.empty()) {
    // remove sampled field
    std::unique_ptr<libsbml::SampledField> sf(
        geom->removeSampledField(sampledFieldID));
    // remove parameter with spatialref to sampled field
    std::string paramID =
        model->getInitialAssignmentBySymbol(speciesID)->getMath()->getName();
    std::unique_ptr<libsbml::Parameter> p(model->removeParameter(paramID));
  }
  std::unique_ptr<libsbml::InitialAssignment> ia(
      model->removeInitialAssignment(speciesID));
}

void SbmlDocWrapper::setAnalyticConcentration(
    const QString &speciesID, const QString &analyticExpression) {
  SPDLOG_INFO("speciesID: {}", speciesID.toStdString());
  SPDLOG_INFO("  - expression: {}", analyticExpression.toStdString());
  std::unique_ptr<libsbml::ASTNode> argAST(
      libsbml::SBML_parseL3Formula(analyticExpression.toStdString().c_str()));
  if (argAST == nullptr) {
    SPDLOG_ERROR("  - libSBML failed to parse expression");
    return;
  }
  removeInitialAssignment(speciesID.toStdString());
  auto asgn = model->createInitialAssignment();
  asgn->setSymbol(speciesID.toStdString());
  asgn->setId(speciesID.toStdString() + "_initialConcentration");
  SPDLOG_INFO("  - creating new assignment: {}", asgn->getId());
  asgn->setMath(argAST.get());
  setFieldConcAnalytic(mapSpeciesIdToField.at(speciesID),
                       analyticExpression.toStdString());
}

QString SbmlDocWrapper::getAnalyticConcentration(
    const QString &speciesID) const {
  auto sf = getSpeciesSampledFieldInitialAssignment(speciesID.toStdString());
  if (!sf.empty()) {
    return {};
  }
  const auto *asgn =
      model->getInitialAssignmentBySymbol(speciesID.toStdString());
  if (asgn != nullptr) {
    return ASTtoString(asgn->getMath()).c_str();
  }
  return {};
}

std::string SbmlDocWrapper::getSpeciesSampledFieldInitialAssignment(
    const std::string &speciesID) const {
  // look for existing initialAssignment to a sampledField
  std::string sampledFieldID;
  if (model->getInitialAssignmentBySymbol(speciesID) != nullptr) {
    auto *asgn = model->getInitialAssignmentBySymbol(speciesID);
    if (asgn->getMath()->isName()) {
      std::string paramID = asgn->getMath()->getName();
      SPDLOG_INFO("  - found initialAssignment: {}", paramID);
      auto *param = model->getParameter(paramID);
      if (param != nullptr) {
        auto *spp = dynamic_cast<libsbml::SpatialParameterPlugin *>(
            param->getPlugin("spatial"));
        if (spp != nullptr) {
          const auto &ref = spp->getSpatialSymbolReference()->getSpatialRef();
          SPDLOG_INFO("  - found spatialSymbolReference: {}", ref);
          if (geom->getSampledField(ref) != nullptr) {
            sampledFieldID = ref;
            SPDLOG_INFO("  - this is a reference to a SampledField");
          }
        }
      }
    }
  }
  return sampledFieldID;
}

void SbmlDocWrapper::setSampledFieldConcentration(
    const QString &speciesID, const std::vector<double> &concentrationArray) {
  std::string sId = speciesID.toStdString();
  SPDLOG_INFO("speciesID: {}", sId);
  removeInitialAssignment(sId);
  // sampled field
  auto *sf = geom->createSampledField();
  std::string id = speciesID.toStdString() + "_initialConcentration";
  while (!isSpatialIdAvailable(id)) {
    id.append("_");
  }
  sf->setId(id);
  SPDLOG_INFO("  - creating SampledField: {}", sf->getId());
  sf->setSamples(concentrationArray);
  sf->setNumSamples1(compartmentImage.width());
  sf->setNumSamples2(compartmentImage.height());
  SPDLOG_INFO("  - set samples to {}x{} array", sf->getNumSamples1(),
              sf->getNumSamples2());
  sf->setDataType(libsbml::DataKind_t::SPATIAL_DATAKIND_DOUBLE);
  sf->setInterpolationType(
      libsbml::InterpolationKind_t::SPATIAL_INTERPOLATIONKIND_LINEAR);
  sf->setCompression(
      libsbml::CompressionKind_t::SPATIAL_COMPRESSIONKIND_UNCOMPRESSED);
  // create SBML parameter with spatial ref to sampled field
  auto *param = model->createParameter();
  id = speciesID.toStdString() + "_initialConcentration";
  while (!isSIdAvailable(id)) {
    id.append("_");
  }
  param->setId(id);
  param->setConstant(true);
  SPDLOG_INFO("  - creating Parameter: {}", param->getId());
  auto *spp = dynamic_cast<libsbml::SpatialParameterPlugin *>(
      param->getPlugin("spatial"));
  auto *ssr = spp->createSpatialSymbolReference();
  ssr->setSpatialRef(sf->getId());
  SPDLOG_INFO("  - with spatialSymbolReference: {}", ssr->getSpatialRef());
  auto *asgn = model->createInitialAssignment();
  asgn->setSymbol(sId);
  std::unique_ptr<libsbml::ASTNode> argAST(
      libsbml::SBML_parseL3Formula(param->getId().c_str()));
  asgn->setMath(argAST.get());
  SPDLOG_INFO("  - creating initialAssignment: {}", asgn->getMath()->getName());

  mapSpeciesIdToField.at(speciesID).importConcentration(concentrationArray);
}

std::vector<double> SbmlDocWrapper::getSampledFieldConcentration(
    const QString &speciesID) const {
  std::vector<double> array;
  std::string sampledFieldID =
      getSpeciesSampledFieldInitialAssignment(speciesID.toStdString());
  if (!sampledFieldID.empty()) {
    const auto *sf = geom->getSampledField(sampledFieldID);
    sf->getSamples(array);
  }
  SPDLOG_DEBUG("returning array of size {}", array.size());
  return array;
}

QImage SbmlDocWrapper::getConcentrationImage(const QString &speciesID) const {
  if (!hasValidGeometry) {
    return compartmentImage;
  }
  return mapSpeciesIdToField.at(speciesID).getConcentrationImage();
}

void SbmlDocWrapper::setIsSpatial(const QString &speciesID, bool isSpatial) {
  mapSpeciesIdToField.at(speciesID).isSpatial = isSpatial;
  std::string sId = speciesID.toStdString();
  auto *spec = model->getSpecies(sId);
  if (spec == nullptr) {
    SPDLOG_ERROR("Failed to get species {}", sId);
    return;
  }
  auto *ssp =
      dynamic_cast<libsbml::SpatialSpeciesPlugin *>(spec->getPlugin("spatial"));
  if (ssp == nullptr) {
    SPDLOG_ERROR("Failed to get SpatialSpeciesPlugin for species {}", sId);
    return;
  }
  ssp->setIsSpatial(isSpatial);
  if (isSpatial) {
    // for now spatial species cannot be constant
    setIsSpeciesConstant(speciesID.toStdString(), false);
  }
  if (!isSpatial) {
    removeInitialAssignment(speciesID.toStdString());
    setDiffusionConstant(speciesID, 0.0);
  }
}

bool SbmlDocWrapper::getIsSpatial(const QString &speciesID) const {
  return mapSpeciesIdToField.at(speciesID).isSpatial;
}

void SbmlDocWrapper::setDiffusionConstant(const QString &speciesID,
                                          double diffusionConstant) {
  SPDLOG_INFO("SpeciesID: {}", speciesID.toStdString());
  libsbml::Parameter *param = nullptr;
  // look for existing diffusion constant parameter
  for (unsigned i = 0; i < model->getNumParameters(); ++i) {
    auto *par = model->getParameter(i);
    if (auto *spp = dynamic_cast<const libsbml::SpatialParameterPlugin *>(
            par->getPlugin("spatial"));
        (spp != nullptr) && spp->isSetDiffusionCoefficient() &&
        (spp->getDiffusionCoefficient()->getVariable() ==
         speciesID.toStdString())) {
      param = par;
      SPDLOG_INFO("  - found existing diffusion constant: {}", param->getId());
    }
  }
  if (param == nullptr) {
    // create new diffusion constant parameter
    param = model->createParameter();
    param->setConstant(true);
    std::string id = speciesID.toStdString() + "_diffusionConstant";
    while (!isSIdAvailable(id)) {
      id.append("_");
    }
    param->setId(id);
    auto *pplugin = dynamic_cast<libsbml::SpatialParameterPlugin *>(
        param->getPlugin("spatial"));
    auto *diffCoeff = pplugin->createDiffusionCoefficient();
    diffCoeff->setVariable(speciesID.toStdString());
    diffCoeff->setType(
        libsbml::DiffusionKind_t::SPATIAL_DIFFUSIONKIND_ISOTROPIC);
    SPDLOG_INFO("  - created new diffusion constant: {}", param->getId());
  }
  param->setValue(diffusionConstant);
  SPDLOG_INFO("  - new value: {}", param->getValue());
  mapSpeciesIdToField.at(speciesID).diffusionConstant = diffusionConstant;
}

double SbmlDocWrapper::getDiffusionConstant(const QString &speciesID) const {
  return mapSpeciesIdToField.at(speciesID).diffusionConstant;
}

void SbmlDocWrapper::setInitialConcentration(const QString &speciesID,
                                             double concentration) {
  removeInitialAssignment(speciesID.toStdString());
  model->getSpecies(speciesID.toStdString())
      ->setInitialConcentration(concentration);
  mapSpeciesIdToField.at(speciesID).setUniformConcentration(concentration);
}

double SbmlDocWrapper::getInitialConcentration(const QString &speciesID) const {
  return model->getSpecies(speciesID.toStdString())->getInitialConcentration();
}

void SbmlDocWrapper::setSpeciesColour(const QString &speciesID,
                                      const QColor &colour) {
  mapSpeciesIdToField.at(speciesID).colour = colour;
}

const QColor &SbmlDocWrapper::getSpeciesColour(const QString &speciesID) const {
  return mapSpeciesIdToField.at(speciesID).colour;
}

void SbmlDocWrapper::setIsSpeciesConstant(const std::string &speciesID,
                                          bool constant) {
  auto *spec = model->getSpecies(speciesID);
  spec->setConstant(constant);
  if (constant) {
    // for now: constant species must be non-spatial
    setIsSpatial(speciesID.c_str(), false);
  }
  // todo: think about how to deal with boundaryCondition properly
  // for now, just set it to false here: species is either constant, or not
  spec->setBoundaryCondition(false);
}

bool SbmlDocWrapper::getIsSpeciesConstant(const std::string &speciesID) const {
  auto *spec = model->getSpecies(speciesID);
  if (spec->isSetConstant() && spec->getConstant()) {
    // `Constant` species is a constant:
    //  - cannot be altered by Reactions or by RateRules
    return true;
  }
  if ((spec->isSetBoundaryCondition() && spec->getBoundaryCondition()) &&
      model->getRateRule(speciesID) == nullptr) {
    // `BoundaryCondition` species is a constant:
    //   - unless it is altered by a RateRule
    return true;
  }
  return false;
}

QString SbmlDocWrapper::getSpeciesCompartment(const QString &speciesID) const {
  auto sID = speciesID.toStdString();
  const auto *spec = model->getSpecies(sID);
  if (spec == nullptr) {
    SPDLOG_WARN("Species {} not found", sID);
    return {};
  }
  return spec->getCompartment().c_str();
}

void SbmlDocWrapper::setSpeciesCompartment(const QString &speciesID,
                                           const QString &compartmentID) {
  if (model->getCompartment(compartmentID.toStdString()) == nullptr) {
    SPDLOG_WARN("Compartment {} not found", compartmentID.toStdString());
    return;
  }
  auto *spec = model->getSpecies(speciesID.toStdString());
  if (spec == nullptr) {
    SPDLOG_WARN("Species {} not found", speciesID.toStdString());
    return;
  }
  // update species list
  species.at(spec->getCompartment().c_str()).removeOne(speciesID);
  species.at(compartmentID).push_back(speciesID);
  // update species compartment in SBML
  spec->setCompartment(compartmentID.toStdString());
  // update field with new compartment
  auto &field = mapSpeciesIdToField.at(speciesID);
  field.setCompartment(&mapCompIdToGeometry.at(compartmentID));
  // set initial concentration to spatially uniform
  setInitialConcentration(speciesID, spec->getInitialConcentration());
  // update reaction list
  updateReactionList();
}

bool SbmlDocWrapper::isSIdAvailable(const std::string &id) const {
  return model->getElementBySId(id) == nullptr;
}

bool SbmlDocWrapper::isSpatialIdAvailable(const std::string &id) const {
  return geom->getElementBySId(id) == nullptr;
}

QString SbmlDocWrapper::nameToSId(const QString &name) const {
  SPDLOG_DEBUG("name: '{}'", name.toStdString());
  const std::string charsToConvertToUnderscore = " -_/";
  std::string id;
  // remove any non-alphanumeric chars, convert spaces etc to underscores
  for (auto c : name.toStdString()) {
    if (std::isalnum(c, std::locale::classic())) {
      id.push_back(c);
    } else if (charsToConvertToUnderscore.find(c) != std::string::npos) {
      id.push_back('_');
    }
  }
  // first char must be a letter or underscore
  if (!std::isalpha(id.front())) {
    id = "_" + id;
  }
  SPDLOG_DEBUG("  -> '{}'", id);
  // ensure it is unique, i.e. doesn't clash with any other SId in model
  while (!isSIdAvailable(id)) {
    id.append("_");
    SPDLOG_DEBUG("  -> '{}'", id);
  }
  return id.c_str();
}

void SbmlDocWrapper::addSpecies(const QString &speciesName,
                                const QString &compartmentID) {
  SPDLOG_INFO("Adding new species");
  auto *spec = model->createSpecies();
  SPDLOG_INFO("  - name: {}", speciesName.toStdString());
  spec->setName(speciesName.toStdString());
  auto speciesID = nameToSId(speciesName);
  SPDLOG_INFO("  - id: {}", speciesID.toStdString());
  spec->setId(speciesID.toStdString());
  SPDLOG_INFO("  - compartment: {}", compartmentID.toStdString());
  spec->setCompartment(compartmentID.toStdString());
  // add to species list
  species.at(compartmentID).push_back(speciesID);
  // set default colour
  mapSpeciesIdToColour[speciesID] =
      utils::indexedColours()[model->getNumSpecies() - 1];
  // create field
  mapSpeciesIdToField[speciesID] = geometry::Field(
      &mapCompIdToGeometry.at(compartmentID), speciesID.toStdString(), 1.0,
      mapSpeciesIdToColour.at(speciesID));
  // set initial spatial parameters
  setIsSpatial(speciesID, true);
  setDiffusionConstant(speciesID, 1.0);
  setInitialConcentration(speciesID, 0.0);
}

std::vector<std::string> SbmlDocWrapper::getSpeciesReactions(
    const QString &speciesID) const {
  std::vector<std::string> reactionIds;
  std::string sId = speciesID.toStdString();
  for (unsigned i = 0; i < model->getNumReactions(); ++i) {
    const auto *reac = model->getReaction(i);
    bool containsSpecies = false;
    for (unsigned j = 0; j < reac->getNumProducts(); ++j) {
      if (reac->getProduct(j)->getSpecies() == sId) {
        containsSpecies = true;
      }
    }
    for (unsigned j = 0; j < reac->getNumReactants(); ++j) {
      if (reac->getReactant(j)->getSpecies() == sId) {
        containsSpecies = true;
      }
    }
    for (unsigned j = 0; j < reac->getNumModifiers(); ++j) {
      if (reac->getModifier(j)->getSpecies() == sId) {
        containsSpecies = true;
      }
    }
    if (containsSpecies) {
      reactionIds.push_back(reac->getId());
    }
  }
  return reactionIds;
}

void SbmlDocWrapper::removeSpecies(const QString &speciesID) {
  std::string sId = speciesID.toStdString();
  SPDLOG_INFO("Removing species {}", sId);
  std::unique_ptr<libsbml::Species> spec(model->removeSpecies(sId));
  if (spec == nullptr) {
    SPDLOG_WARN("  - species {} not found", sId);
    return;
  }
  // remove species from species list
  species.at(spec->getCompartment().c_str()).removeOne(speciesID);
  // remove any reactions that depend on it
  for (const auto &reacID : getSpeciesReactions(speciesID)) {
    removeReaction(reacID.c_str(), false);
  }
  updateReactionList();
  SPDLOG_INFO("  - species {} removed", spec->getId());
}

void SbmlDocWrapper::removeReaction(const QString &reactionID,
                                    bool callUpdateReactionList) {
  std::string sId = reactionID.toStdString();
  SPDLOG_INFO("Removing reaction {}", sId);
  std::unique_ptr<libsbml::Reaction> reac(model->removeReaction(sId));
  if (reac == nullptr) {
    SPDLOG_WARN("  - reaction {} not found", sId);
    return;
  }
  if (callUpdateReactionList) {
    updateReactionList();
  }
  SPDLOG_INFO("  - reaction {} removed", reac->getId());
}

void SbmlDocWrapper::setSpeciesName(const QString &speciesID,
                                    const QString &name) {
  auto *spec = model->getSpecies(speciesID.toStdString());
  SPDLOG_INFO("Setting species {} name to {}", spec->getId(),
              name.toStdString());
  spec->setName(name.toStdString());
}

QString SbmlDocWrapper::getSpeciesName(const QString &speciesID) const {
  return model->getSpecies(speciesID.toStdString())->getName().c_str();
}

QString SbmlDocWrapper::getReactionName(const QString &reactionID) const {
  return model->getReaction(reactionID.toStdString())->getName().c_str();
}

bool SbmlDocWrapper::isSpeciesReactive(const std::string &speciesID) const {
  // true if this species should have a PDE generated for it
  // by the Reactions that involve it
  auto *spec = model->getSpecies(speciesID);
  if ((spec->isSetConstant() && spec->getConstant()) ||
      (spec->isSetBoundaryCondition() && spec->getBoundaryCondition())) {
    return false;
  }
  return true;
}

std::map<std::string, double> SbmlDocWrapper::getGlobalConstants() const {
  std::map<std::string, double> constants;
  // add all *constant* species as constants
  for (unsigned k = 0; k < model->getNumSpecies(); ++k) {
    const auto *spec = model->getSpecies(k);
    if (getIsSpeciesConstant(spec->getId())) {
      SPDLOG_TRACE("found constant species {}", spec->getId());
      double init_conc = spec->getInitialConcentration();
      constants[spec->getId()] = init_conc;
      SPDLOG_TRACE("parameter {} = {}", spec->getId(), init_conc);
    }
  }
  // add any parameters (that are not replaced by an AssignmentRule)
  for (unsigned k = 0; k < model->getNumParameters(); ++k) {
    const auto *param = model->getParameter(k);
    if (model->getAssignmentRule(param->getId()) == nullptr) {
      SPDLOG_TRACE("parameter {} = {}", param->getId(), param->getValue());
      constants[param->getId()] = param->getValue();
    }
  }
  // also get compartment volumes (the compartmentID may be used in the
  // reaction equation, and it should be replaced with the value of the "Size"
  // parameter for this compartment)
  for (unsigned int k = 0; k < model->getNumCompartments(); ++k) {
    const auto *comp = model->getCompartment(k);
    SPDLOG_TRACE("parameter {} = {}", comp->getId(), comp->getSize());
    constants[comp->getId()] = comp->getSize();
  }
  // remove x and y if present, as these are not really parameters
  // (we want them to remain as variables to be parsed by symbolic parser)
  // todo: check if this can be done in a better way
  for (const auto &c : {"x", "y"}) {
    auto iter = constants.find(c);
    if (iter != constants.cend()) {
      constants.erase(iter);
    }
  }
  return constants;
}

double SbmlDocWrapper::getPixelWidth() const { return pixelWidth; }

void SbmlDocWrapper::setPixelWidth(double width) {
  double oldWidth = pixelWidth;

  pixelWidth = width;
  // update pixelWidth for each compartment
  for (auto &pair : mapCompIdToGeometry) {
    pair.second.pixelWidth = width;
  }
  SPDLOG_INFO("New pixel width = {}", pixelWidth);

  // update compartment interior points
  for (const auto &compartmentID : compartments) {
    SPDLOG_INFO("  - compartmentID: {}", compartmentID.toStdString());
    auto *comp = model->getCompartment(compartmentID.toStdString());
    auto *scp = dynamic_cast<libsbml::SpatialCompartmentPlugin *>(
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
  origin *= width / oldWidth;
  SPDLOG_INFO("  - origin rescaled to ({},{})", origin.x(), origin.y());

  mesh->setPhysicalGeometry(width, origin);
  // update xy coordinates
  auto *coord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_X);
  auto *min = coord->getBoundaryMin();
  auto *max = coord->getBoundaryMax();
  min->setValue(origin.x());
  max->setValue(origin.x() +
                pixelWidth * static_cast<double>(compartmentImage.width()));
  SPDLOG_INFO("  - x now in range [{},{}]", min->getValue(), max->getValue());
  coord = geom->getCoordinateComponentByKind(
      libsbml::CoordinateKind_t::SPATIAL_COORDINATEKIND_CARTESIAN_Y);
  min = coord->getBoundaryMin();
  max = coord->getBoundaryMax();
  min->setValue(origin.y());
  max->setValue(origin.y() +
                pixelWidth * static_cast<double>(compartmentImage.height()));
  SPDLOG_INFO("  - y now in range [{},{}]", min->getValue(), max->getValue());
}

const QPointF &SbmlDocWrapper::getOrigin() const { return origin; }

void SbmlDocWrapper::setCompartmentSizeFromImage(
    const std::string &compartmentID) {
  // update compartment Size based on pixel count & pixel size
  auto *comp = model->getCompartment(compartmentID);
  if (comp == nullptr) {
    return;
  }
  SPDLOG_INFO("compartmentID {}", comp->getId());
  SPDLOG_INFO("  - previous size: {}", comp->getSize());
  std::size_t nPixels = mapCompIdToGeometry.at(comp->getId().c_str()).ix.size();
  SPDLOG_INFO("  - number of pixels: {}", nPixels);
  SPDLOG_INFO("  - pixel width: {} {}", pixelWidth,
              modelUnits.getLength().name.toStdString());
  double pixelVol = units::pixelWidthToVolume(
      pixelWidth, modelUnits.getLength(), modelUnits.getVolume());
  SPDLOG_INFO("  - pixel volume (width*width*1): {} {}", pixelVol,
              modelUnits.getVolume().name.toStdString());
  double newSize = static_cast<double>(nPixels) * pixelVol;
  comp->setSize(newSize);
  comp->setUnits(model->getVolumeUnits());
  SPDLOG_INFO("  - new size: {} {}", comp->getSize(),
              modelUnits.getVolume().name.toStdString());
}

std::string SbmlDocWrapper::inlineExpr(
    const std::string &mathExpression) const {
  std::string inlined;
  // inline any Function calls in expr
  inlined = inlineFunctions(mathExpression);
  // inline any Assignment Rules in expr
  inlined = inlineAssignments(inlined);
  return inlined;
}

std::string SbmlDocWrapper::inlineFunctions(
    const std::string &mathExpression) const {
  std::string expr = mathExpression;
  SPDLOG_DEBUG("inlining {}", expr);
  for (unsigned int i = 0; i < model->getNumFunctionDefinitions(); ++i) {
    const auto *func = model->getFunctionDefinition(i);
    // get copy of function body as AST node
    std::unique_ptr<libsbml::ASTNode> funcBody(func->getBody()->deepCopy());
    // search for function call in expression
    std::string funcCallString = func->getId() + "(";
    auto loc = expr.find(funcCallString);
    auto fn_loc = loc;
    while (loc != std::string::npos) {
      // function call found
      fn_loc = loc;
      loc += func->getId().size() + 1;
      for (unsigned int j = 0; j < func->getNumArguments(); ++j) {
        // compare each argument used in the function call (arg)
        // to the corresponding variable in the function definition
        while (expr[loc] == ' ') {
          // trim any leading spaces
          ++loc;
        }
        auto arg_len = expr.find_first_of(",)", loc + 1) - loc;
        std::string arg = expr.substr(loc, arg_len);
        if (func->getArgument(j)->getName() != arg) {
          // create desired new argument as AST node
          std::unique_ptr<libsbml::ASTNode> argAST(
              libsbml::SBML_parseL3Formula(arg.c_str()));
          // replace existing argument with new argument
          funcBody->replaceArgument(func->getArgument(j)->getName(),
                                    argAST.get());
        }
        loc += arg_len + 1;
      }
      // replace function call with inlined body of function
      std::string funcBodyString = ASTtoString(funcBody.get());
      // wrap function body in parentheses
      std::string pre_expr = expr.substr(0, fn_loc);
      std::string post_expr = expr.substr(loc);
      expr = pre_expr + "(" + funcBodyString + ")" + post_expr;
      // go to end of inlined function body in expr
      loc = fn_loc + funcBodyString.size() + 2;
      SPDLOG_DEBUG("  - new expr = {}", expr);
      // search for next call to same function in expr
      loc = expr.find(funcCallString, loc);
    }
  }
  return expr;
}

std::string SbmlDocWrapper::inlineAssignments(
    const std::string &mathExpression) const {
  const std::string delimeters = "()-^*/+, ";
  std::string expr = mathExpression;
  std::string old_expr;
  SPDLOG_DEBUG("inlining {}", expr);
  // iterate through names in expression
  // where names are things in between any of these chars:
  // "()^*/+, "
  // http://sbml.org/Special/Software/libSBML/docs/formatted/cpp-api/class_a_s_t_node.html
  while (expr != old_expr) {
    old_expr = expr;
    auto start = expr.find_first_not_of(delimeters);
    while (start != std::string::npos) {
      auto end = expr.find_first_of(delimeters, start);
      std::string name = expr.substr(start, end - start);
      const auto *assignment = model->getAssignmentRule(name);
      if (assignment != nullptr) {
        // replace name with inlined body of Assignment rule
        const std::string &assignmentBody =
            model->getAssignmentRule(name)->getFormula();
        // wrap function body in parentheses
        std::string pre_expr = expr.substr(0, start);
        std::string post_expr = expr.substr(end);
        expr = pre_expr + "(" + assignmentBody + ")" + post_expr;
        SPDLOG_DEBUG("  - new expr = {}", expr);
        // go to end of inlined assignment body in expr
        end = start + assignmentBody.size() + 2;
      }
      start = expr.find_first_not_of(delimeters, end);
    }
  }
  return expr;
}

Reac SbmlDocWrapper::getReaction(const QString &reactionID) const {
  Reac r;
  const auto *reac = model->getReaction(reactionID.toStdString());
  if (reac == nullptr) {
    SPDLOG_WARN("reaction {} does not exist", reactionID.toStdString());
    return {};
  }
  const auto *kin = reac->getKineticLaw();
  if (kin == nullptr) {
    SPDLOG_WARN("reaction {} has no KineticLaw", reactionID.toStdString());
    return {};
  }
  r.id = reac->getId();
  r.name = reac->getName();
  r.fullExpression = kin->getFormula();
  r.inlinedExpression = inlineExpr(kin->getFormula());
  r.products.reserve(reac->getNumProducts());
  for (unsigned k = 0; k < reac->getNumProducts(); ++k) {
    const auto *s = reac->getProduct(k);
    r.products.push_back({s->getSpecies(), s->getStoichiometry()});
  }
  r.reactants.reserve(reac->getNumReactants());
  for (unsigned k = 0; k < reac->getNumReactants(); ++k) {
    const auto *s = reac->getReactant(k);
    r.reactants.push_back({s->getSpecies(), s->getStoichiometry()});
  }

  // todo: modifiers??

  // add all local parameters that are not
  // replaced by an assignment rule
  for (unsigned k = 0; k < kin->getNumLocalParameters(); ++k) {
    const auto *param = kin->getLocalParameter(k);
    if (model->getAssignmentRule(param->getId()) == nullptr) {
      r.constants.push_back({param->getId(), param->getValue()});
    }
  }
  return r;
}

std::string SbmlDocWrapper::getRateRule(const std::string &speciesID) const {
  const auto *rule = model->getRateRule(speciesID);
  if (rule != nullptr) {
    return inlineExpr(rule->getFormula());
  }
  return {};
}

Func SbmlDocWrapper::getFunctionDefinition(const QString &functionID) const {
  Func f;
  const auto *func = model->getFunctionDefinition(functionID.toStdString());
  if (func == nullptr) {
    SPDLOG_WARN("function {} does not exist", functionID.toStdString());
    return {};
  }
  f.id = func->getId();
  f.name = func->getName();
  f.expression = ASTtoString(func->getBody());
  f.arguments.reserve(func->getNumArguments());
  for (unsigned i = 0; i < func->getNumArguments(); ++i) {
    f.arguments.push_back(ASTtoString(func->getArgument(i)));
  }
  return f;
}

}  // namespace sbml

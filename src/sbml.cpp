#include "sbml.hpp"

#include <sstream>
#include <unordered_set>

#include "logger.hpp"

namespace sbml {

void SbmlDocWrapper::clearAllModelData() {
  compartments.clear();
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
  compartmentImage = QImage();
}

void SbmlDocWrapper::importSBMLFile(const std::string &filename) {
  clearAllModelData();
  doc.reset(libsbml::readSBMLFromFile(filename.c_str()));

  if (doc->getErrorLog()->getNumFailsWithSeverity(libsbml::LIBSBML_SEV_ERROR) >
      0) {
    isValid = false;
    std::stringstream ss;
    doc->printErrors(ss);
    spdlog::warn(ss.str());
    spdlog::warn(
        "SbmlDocWrapper::importSBMLFile :: Warning - errors while reading SBML "
        "file (continuing anyway...)");
    isValid = true;
  } else {
    spdlog::info(
        "SbmlDocWrapper::importSBMLFile :: Successfully imported file {}",
        filename);
    isValid = true;
  }
  model = doc->getModel();

  // get list of compartments
  for (unsigned int i = 0; i < model->getNumCompartments(); ++i) {
    const auto *comp = model->getCompartment(i);
    QString id = comp->getId().c_str();
    compartments << id;
    species[id] = QStringList();
  }

  // get all species, make a list for each compartment
  for (unsigned int i = 0; i < model->getNumSpecies(); ++i) {
    const auto *spec = model->getSpecies(i);
    if (spec->isSetHasOnlySubstanceUnits() &&
        spec->getHasOnlySubstanceUnits()) {
      // equations expect amount, not concentration for this species
      // for now this is not supported:
      std::string errorMessage(
          "SbmlDocWrapper::importSBMLFile :: Error: HasOnlySubstanceUnits=true "
          "is not yet supported.");
      spdlog::critical(errorMessage);
      qFatal("%s", errorMessage.c_str());
    }
    const auto id = spec->getId().c_str();
    species[spec->getCompartment().c_str()] << QString(id);
    // assign a default colour for displaying the species
    mapSpeciesIdToColour[id] = defaultSpeciesColours()[i];
  }

  // get list of functions
  for (unsigned int i = 0; i < model->getNumFunctionDefinitions(); ++i) {
    const auto *func = model->getFunctionDefinition(i);
    functions << QString(func->getId().c_str());
  }
}

void SbmlDocWrapper::exportSBMLFile(const std::string &filename) const {
  if (isValid) {
    spdlog::info("SbmlDocWrapper::exportSBMLFile : exporting SBML model to %s",
                 filename);
    if (!libsbml::SBMLWriter().writeSBML(doc.get(), filename)) {
      spdlog::error("SbmlDocWrapper::exportSBMLFile : failed to write to %s",
                    filename);
    }
  }
}

void SbmlDocWrapper::importGeometryFromImage(const QString &filename) {
  clearAllGeometryData();
  compartmentImage.load(filename);
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
      spdlog::debug(
          "SbmlDocWrapper::importGeometryFromImage : ColourPair iA,iB: {},{}: "
          "{:x},{:x}",
          iA, iB, std::min(colA, colB), std::max(colA, colB));
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
  hasGeometry = true;
}

const QImage &SbmlDocWrapper::getMembraneImage(const QString &membraneID) {
  if (!hasGeometry) {
    return compartmentImage;
  }
  return mapMembraneToImage.at(membraneID);
}

void SbmlDocWrapper::updateMembraneList() {
  // construct membrane list & images
  membranes.clear();
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
          QString name = compartments[i] + "-" + compartments[j];
          if (colA > colB) {
            name = compartments[j] + "-" + compartments[i];
          }
          membranes.push_back(name);
          // map name to index of membrane location pairs
          mapMembraneToIndex[name] = index;
          // generate image
          QImage img = compartmentImage.convertToFormat(QImage::Format_ARGB32);
          for (const auto &pair : membranePairs[index]) {
            img.setPixel(pair.first, QColor(0, 155, 40, 255).rgba());
            img.setPixel(pair.second, QColor(0, 199, 40, 255).rgba());
          }
          mapMembraneToImage[name] = img;
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
          membraneVec.emplace_back(name.toStdString(), compA, compB,
                                   membranePairs[index]);
        }
      }
    }
  }
}

void SbmlDocWrapper::updateReactionList() {
  reactions.clear();
  for (unsigned int i = 0; i < model->getNumReactions(); ++i) {
    const auto *reac = model->getReaction(i);
    QString reacID = reac->getId().c_str();
    // construct the set of compartments where reaction takes place
    std::unordered_set<std::string> comps;
    for (unsigned int k = 0; k < reac->getNumProducts(); ++k) {
      const std::string &specID = reac->getProduct(k)->getSpecies();
      comps.insert(model->getSpecies(specID)->getCompartment());
    }
    // TODO: also include modifiers here?? (and when compiling reactions??)
    for (unsigned int k = 0; k < reac->getNumReactants(); ++k) {
      const std::string &specID = reac->getReactant(k)->getSpecies();
      comps.insert(model->getSpecies(specID)->getCompartment());
    }
    // single compartment
    if (comps.size() == 1) {
      QString comp = comps.begin()->c_str();
      reactions[comp] << reacID;
    } else if (comps.size() == 2) {
      auto iter = comps.cbegin();
      QString compA = iter->c_str();
      ++iter;
      QString compB = iter->c_str();
      // two compartments: want the membrane between them
      // membrane name is compA-compB, ordered by colour
      QRgb colA = mapCompartmentToColour[compA];
      QRgb colB = mapCompartmentToColour[compB];
      QString membraneID = compA + "-" + compB;
      if (colA > colB) {
        membraneID = compB + "-" + compA;
      }
      // check that compartments map to colours - if not do nothing
      if (colA != 0 && colB != 0) {
        reactions[membraneID] << QString(reac->getId().c_str());
      }
    } else {
      // invalid reaction: number of compartments for reaction must be 1 or 2
      spdlog::error(
          "SbmlDocWrapper::updateReactionList :: reaction involves {} "
          "compartments - not supported",
          comps.size());
      exit(1);
    }
  }
}

const QImage &SbmlDocWrapper::getCompartmentImage() { return compartmentImage; }

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

void SbmlDocWrapper::setCompartmentColour(const QString &compartmentID,
                                          QRgb colour) {
  QRgb oldColour = getCompartmentColour(compartmentID);
  if (oldColour != 0) {
    // if there was already a colour mapped to this compartment, map it to a
    // null compartment
    mapColourToCompartment[oldColour] = "";
  }
  auto oldCompartmentID = getCompartmentID(colour);
  if (oldCompartmentID != "") {
    // if the new colour was already mapped to another compartment, set the
    // colour of that compartment to null
    mapCompartmentToColour[oldCompartmentID] = 0;
  }
  mapColourToCompartment[colour] = compartmentID;
  mapCompartmentToColour[compartmentID] = colour;
  // create compartment geometry for this colour
  mapCompIdToGeometry[compartmentID] = geometry::Compartment(
      compartmentID.toStdString(), getCompartmentImage(), colour);
  geometry::Compartment &comp = mapCompIdToGeometry.at(compartmentID);
  // create a field for each species in this compartment
  for (const auto &s : species[compartmentID]) {
    mapSpeciesIdToField[s] = geometry::Field(&comp, s.toStdString(), 1.0,
                                             mapSpeciesIdToColour.at(s));
    // set all species concentrations to their initial values
    mapSpeciesIdToField.at(s).setUniformConcentration(
        model->getSpecies(s.toStdString())->getInitialConcentration());
  }
  // update list of possible inter-compartment membranes
  updateMembraneList();
  // update list of reactions for each compartment/membrane
  updateReactionList();
}

void SbmlDocWrapper::importConcentrationFromImage(const QString &speciesID,
                                                  const QString &filename) {
  QImage img;
  img.load(filename);
  mapSpeciesIdToField.at(speciesID).importConcentration(img);
}

const QImage &SbmlDocWrapper::getConcentrationImage(const QString &speciesID) {
  if (!hasGeometry) {
    return compartmentImage;
  }
  return mapSpeciesIdToField.at(speciesID).getConcentrationImage();
}

void SbmlDocWrapper::setDiffusionConstant(const QString &speciesID,
                                          double diffusionConstant) {
  mapSpeciesIdToField.at(speciesID).diffusionConstant = diffusionConstant;
}

double SbmlDocWrapper::getDiffusionConstant(const QString &speciesID) const {
  return mapSpeciesIdToField.at(speciesID).diffusionConstant;
}

void SbmlDocWrapper::setSpeciesColour(const QString &speciesID,
                                      const QColor &colour) {
  mapSpeciesIdToField.at(speciesID).colour = colour;
}

const QColor &SbmlDocWrapper::getSpeciesColour(const QString &speciesID) const {
  return mapSpeciesIdToField.at(speciesID).colour;
}

const QString &SbmlDocWrapper::getXml() {
  std::unique_ptr<char, decltype(&std::free)> xmlChar(
      libsbml::writeSBMLToString(doc.get()), &std::free);
  xmlString = QString(xmlChar.get());
  return xmlString;
}

bool SbmlDocWrapper::isSpeciesConstant(const std::string &speciesID) const {
  auto *spec = model->getSpecies(speciesID);
  if (spec->isSetConstant() && spec->getConstant()) {
    // `Constant` species is a constant:
    //  - cannot be altered by Reactions or by RateRules
    return true;
  }
  if ((spec->isSetBoundaryCondition() && spec->getBoundaryCondition()) &&
      model->getRateRule(speciesID) == nullptr) {
    // `BoundaryCondition` species is usually constant:
    //   - unless it is altered by a RateRule
    return true;
  }
  return false;
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

std::string SbmlDocWrapper::inlineFunctions(
    const std::string &mathExpression) const {
  std::string expr = mathExpression;
  spdlog::debug("SbmlDocWrapper::inlineFunctions :: inlining {}", expr);
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
      std::unique_ptr<char, decltype(&std::free)> funcBodyChar(
          libsbml::SBML_formulaToL3String(funcBody.get()), &std::free);
      std::string funcBodyString(funcBodyChar.get());
      // wrap function body in parentheses
      std::string pre_expr = expr.substr(0, fn_loc);
      std::string post_expr = expr.substr(loc);
      expr = pre_expr + "(" + funcBodyString + ")" + post_expr;
      // go to end of inlined function body in expr
      loc = fn_loc + funcBodyString.size() + 2;
      spdlog::debug("SbmlDocWrapper::inlineFunctions ::   - new expr = {}",
                    expr);
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
  spdlog::debug("SbmlDocWrapper::inlineAssignments :: inlining {}", expr);
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
        spdlog::debug("SbmlDocWrapper::inlineAssignments ::   - new expr = {}",
                      expr);
        // go to end of inlined assignment body in expr
        end = start + assignmentBody.size() + 2;
      }
      start = expr.find_first_not_of(delimeters, end);
    }
  }
  return expr;
}

}  // namespace sbml

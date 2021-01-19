// SBML xml annotation read/write

#pragma once

#include "model_display_options.hpp"
#include <QRgb>
#include <optional>
#include <string>
#include <vector>

namespace libsbml {
class ParametricGeometry;
class Species;
class Model;
} // namespace libsbml

namespace sme {

namespace mesh {
class Mesh;
}

namespace model {

struct MeshParamsAnnotationData {
  std::vector<std::size_t> maxPoints;
  std::vector<std::size_t> maxAreas;
};

void removeMeshParamsAnnotation(libsbml::ParametricGeometry *pg);
void addMeshParamsAnnotation(libsbml::ParametricGeometry *pg,
                             const mesh::Mesh *mesh);
std::optional<MeshParamsAnnotationData>
getMeshParamsAnnotationData(const libsbml::ParametricGeometry *pg);

void removeSpeciesColourAnnotation(libsbml::Species *species);
void addSpeciesColourAnnotation(libsbml::Species *species, QRgb colour);
std::optional<QRgb> getSpeciesColourAnnotation(const libsbml::Species *species);

void removeDisplayOptionsAnnotation(libsbml::Model *model);
void addDisplayOptionsAnnotation(libsbml::Model *model,
                                 const model::DisplayOptions &displayOptions);
std::optional<model::DisplayOptions>
getDisplayOptionsAnnotation(const libsbml::Model *model);

} // namespace model

} // namespace sme

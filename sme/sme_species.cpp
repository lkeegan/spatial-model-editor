#include "sme_species.hpp"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "model.hpp"
#include "sme_common.hpp"

namespace sme {

void pybindSpecies(const pybind11::module &m) {
  pybind11::class_<sme::Species>(m, "Species")
      .def_property("name", &sme::Species::getName, &sme::Species::setName,
                    "The name of this species")
      .def_property("diffusion_constant", &sme::Species::getDiffusionConstant,
                    &sme::Species::setDiffusionConstant,
                    "The diffusion constant of this species")
      .def("__repr__",
           [](const sme::Species &a) {
             return fmt::format("<sme.Species named '{}'>", a.getName());
           })
      .def("__str__", &sme::Species::getStr);
}

Species::Species(model::Model *sbmlDocWrapper, const std::string &sId)
    : s(sbmlDocWrapper), id(sId) {}

void Species::setName(const std::string &name) {
  s->getSpecies().setName(id.c_str(), name.c_str());
}

std::string Species::getName() const {
  return s->getSpecies().getName(id.c_str()).toStdString();
}

void Species::setDiffusionConstant(double diffusionConstant) {
  s->getSpecies().setDiffusionConstant(id.c_str(), diffusionConstant);
}

double Species::getDiffusionConstant() const {
  return s->getSpecies().getDiffusionConstant(id.c_str());
}

std::string Species::getStr() const {
  std::string str("<sme.Species>\n");
  str.append(fmt::format("  - name: '{}'\n", getName()));
  str.append(
      fmt::format("  - diffusion_constant: {}\n", getDiffusionConstant()));
  return str;
}

} // namespace sme

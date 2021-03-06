#pragma once

#include <pybind11/pybind11.h>
#include <string>

namespace model {
class Model;
}

namespace sme {

void pybindSpecies(pybind11::module &m);

class Species {
private:
  model::Model *s;
  std::string id;

public:
  Species() = default;
  explicit Species(model::Model *sbmlDocWrapper, const std::string &sId);
  std::string getName() const;
  void setName(const std::string &name);
  double getDiffusionConstant() const;
  void setDiffusionConstant(double diffusionConstant);
  std::string getStr() const;
};

} // namespace sme

PYBIND11_MAKE_OPAQUE(std::vector<sme::Species>)

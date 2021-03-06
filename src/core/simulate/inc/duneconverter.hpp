// DUNE-copasi ini file generation
//  - DuneConverter: construct DUNE ini file from model

#pragma once

#include "simulate_options.hpp"
#include <QString>
#include <vector>
#include <map>

namespace sme {

namespace model {
class Model;
}

namespace mesh {
class Mesh;
}

namespace simulate {

class DuneConverter {
public:
  explicit DuneConverter(
      const model::Model &model,
      const std::map<std::string, double, std::less<>> &substitutions = {},
      bool forExternalUse = false, const QString &outputIniFile = {},
      int doublePrecision = 18);
  QString getIniFile(std::size_t compartmentIndex = 0) const;
  const std::vector<QString> &getIniFiles() const;
  bool hasIndependentCompartments() const;

  const mesh::Mesh *getMesh() const;
  const std::vector<std::vector<std::vector<double>>> &
  getConcentrations() const;
  double getXOrigin() const;
  double getYOrigin() const;
  double getPixelWidth() const;
  int getImageWidth() const;

private:
  std::vector<QString> iniFiles;
  bool independentCompartments{true};
  const mesh::Mesh *mesh;
  std::vector<std::vector<std::vector<double>>> concentrations;
  double x0;
  double y0;
  double a;
  int w;
};

} // namespace simulate

} // namespace sme

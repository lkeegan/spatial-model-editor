// Partial Differential Equation (PDE) class
//  - construct PDE for given compartment or membrane
//  - PDE equation + Jacobian for each species as infix strings
// Reaction class
//  - construct matrix of stoich coefficients and reaction terms as strings
//  - along with a map of constants

#pragma once

#include <map>
#include <string>
#include <vector>

namespace sbml {
class SbmlDocWrapper;
class Reac;
}  // namespace sbml

namespace pde {

class PDE {
 private:
  std::vector<std::string> species;
  std::vector<std::string> rhs;
  std::vector<std::vector<std::string>> jacobian;

 public:
  explicit PDE(const sbml::SbmlDocWrapper *doc_ptr,
               const std::vector<std::string> &speciesIDs,
               const std::vector<std::string> &reactionIDs,
               const std::vector<std::string> &relabelledSpeciesIDs = {},
               const std::vector<std::string> &reactionScaleFactors = {});
  const std::vector<std::string> &getRHS() const;
  const std::vector<std::vector<std::string>> &getJacobian() const;
};

class Reaction {
 private:
  // vector of speciesIDs
  std::vector<std::string> speciesIDs;
  // vector of reaction expressions as strings
  std::vector<std::string> expressions;
  // matrix M_ij of stoichiometric coefficients
  // i is the species index
  // j is the reaction index
  std::vector<std::vector<double>> M;
  // vector of maps of constants
  std::vector<std::map<std::string, double>> constants;
  std::vector<double> getStoichMatrixRow(const sbml::SbmlDocWrapper *doc,
                                         const sbml::Reac &reac) const;

 public:
  std::size_t size() const;
  const std::vector<std::string> &getSpeciesIDs() const;
  const std::string &getExpression(std::size_t reactionIndex) const;
  double getMatrixElement(std::size_t speciesIndex,
                          std::size_t reactionIndex) const;
  const std::map<std::string, double> &getConstants(
      std::size_t reactionIndex) const;
  Reaction(const sbml::SbmlDocWrapper *doc,
           const std::vector<std::string> &species,
           const std::vector<std::string> &reactionIDs);
};

}  // namespace pde

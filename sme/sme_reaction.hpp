#pragma once

#include <map>
#include <string>
#include <vector>

#include "sme_reactionparameter.hpp"

namespace model {
class Model;
}

namespace pybind11 {
class module;
}

namespace sme {

void pybindReaction(const pybind11::module& m);

class Reaction {
 private:
  model::Model* s;
  std::string id;
  std::vector<ReactionParameter> parameters;

 public:
  explicit Reaction(model::Model* sbmlDocWrapper,
                    const std::string& sId);
  const std::string& getId() const;
  void setName(const std::string& name);
  std::string getName() const;
  std::map<std::string, ReactionParameter*> getParameters();
  std::string getStr() const;
};

}  // namespace sme

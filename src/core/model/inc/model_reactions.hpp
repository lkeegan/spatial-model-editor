// SBML reactions

#pragma once

#include "geometry.hpp"
#include <QColor>
#include <QStringList>
#include <map>
#include <optional>
#include <string>

namespace libsbml {
class Model;
}

namespace sme::model {

class ModelMembranes;

class ModelReactions {
private:
  QStringList ids;
  QStringList names;
  QVector<QStringList> parameterIds;
  libsbml::Model *sbmlModel{};
  const ModelMembranes *modelMembranes{};
  bool hasUnsavedChanges{false};

public:
  ModelReactions();
  explicit ModelReactions(libsbml::Model *model,
                          const ModelMembranes *membranes);
  void makeReactionsSpatial(bool haveValidGeometry);
  QStringList getIds(const QString &locationId) const;
  QString add(const QString &name, const QString &locationId,
              const QString &rateExpression = "1");
  void remove(const QString &id);
  void removeAllInvolvingSpecies(const QString &speciesId);
  QString setName(const QString &id, const QString &name);
  QString getName(const QString &id) const;
  QString getScheme(const QString &id) const;
  void setLocation(const QString &id, const QString &locationId);
  QString getLocation(const QString &id) const;
  double getSpeciesStoichiometry(const QString &id,
                                 const QString &speciesId) const;
  void setSpeciesStoichiometry(const QString &id, const QString &speciesId,
                               double stoichiometry);
  QString getRateExpression(const QString &id) const;
  void setRateExpression(const QString &id, const QString &expression);
  QStringList getParameterIds(const QString &id) const;
  QString setParameterName(const QString &reactionId,
                           const QString &parameterId, const QString &name);
  QString getParameterName(const QString &reactionId,
                           const QString &parameterId) const;
  void setParameterValue(const QString &reactionId, const QString &parameterId,
                         double value);
  double getParameterValue(const QString &reactionId,
                           const QString &parameterId) const;
  QString addParameter(const QString &reactionId, const QString &name,
                       double value);
  void removeParameter(const QString &reactionId, const QString &id);
  bool dependOnVariable(const QString &variableId) const;
  bool getHasUnsavedChanges() const;
  void setHasUnsavedChanges(bool unsavedChanges);
};

} // namespace sme::model

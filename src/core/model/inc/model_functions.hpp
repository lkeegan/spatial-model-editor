// SBML functions

#pragma once

#include <QColor>
#include <QStringList>
#include <map>
#include <optional>
#include <string>

namespace libsbml {
class Model;
}

namespace model {

class ModelFunctions {
private:
  QStringList ids;
  QStringList names;
  libsbml::Model *sbmlModel = nullptr;

public:
  ModelFunctions();
  explicit ModelFunctions(libsbml::Model *model);
  const QStringList &getIds() const;
  const QStringList &getNames() const;
  QString setName(const QString &id, const QString &name);
  QString getName(const QString &id) const;
  void setExpression(const QString &id, const QString &expression);
  QString getExpression(const QString &id) const;
  QStringList getArguments(const QString &id) const;
  QString addArgument(const QString &functionId, const QString &argumentId);
  void removeArgument(const QString &functionId, const QString &argumentId);
  void add(const QString &name);
  void remove(const QString &id);
};

} // namespace model

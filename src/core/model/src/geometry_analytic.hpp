// SBML AnalyticGeometry
//   - import analytic geometry from spatial SBML model
//   - convert to a sampled field geometry

#pragma once

#include <QImage>

namespace libsbml {
class Model;
}

namespace model {

struct GeometrySampledField;

GeometrySampledField importGeometryFromAnalyticGeometry(libsbml::Model *model,
                                                        const QPointF &origin,
                                                        const QSizeF &size);

} // namespace model

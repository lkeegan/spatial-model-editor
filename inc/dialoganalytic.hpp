#pragma once

#include <QDialog>
#include <memory>

#include "symbolic.hpp"
#include "utils.hpp"

namespace Ui {
class DialogAnalytic;
}

namespace units {
class ModelUnits;
}

class DialogAnalytic : public QDialog {
  Q_OBJECT

 public:
  explicit DialogAnalytic(const QString& analyticExpression,
                          const QSize& imageSize,
                          const std::vector<QPoint>& imagePoints,
                          const QPointF& physicalOrigin, double pixelWidth,
                          const units::ModelUnits& modelUnits,
                          QWidget* parent = nullptr);
  const std::string& getExpression() const;
  bool isExpressionValid() const;

 private:
  std::shared_ptr<Ui::DialogAnalytic> ui;

  // user supplied data
  const std::vector<QPoint>& points;
  double width;
  QPointF origin;
  QString lengthUnit;
  QString concentrationUnit;

  QImage img;
  utils::QPointIndexer qpi;
  std::vector<double> concentration;
  std::string expression;
  bool expressionIsValid = false;

  QPointF physicalPoint(const QPoint& pixelPoint) const;
  void txtExpression_cursorPositionChanged();
  void txtExpression_textChanged();
  void lblImage_mouseOver(QPoint point);
};

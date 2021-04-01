#include "tabgeometry.hpp"
#include "guiutils.hpp"
#include "logger.hpp"
#include "mesh.hpp"
#include "model.hpp"
#include "qlabelmousetracker.hpp"
#include "ui_tabgeometry.h"
#include <QInputDialog>
#include <QMessageBox>
#include <stdexcept>

TabGeometry::TabGeometry(sme::model::Model &m, QLabelMouseTracker *mouseTracker,
                         QLabel *statusBarMsg, QWidget *parent)
    : QWidget(parent), ui{std::make_unique<Ui::TabGeometry>()}, model(m),
      lblGeometry(mouseTracker), statusBarPermanentMessage(statusBarMsg) {
  ui->setupUi(this);
  ui->tabCompartmentGeometry->setCurrentIndex(0);

  connect(lblGeometry, &QLabelMouseTracker::mouseClicked, this,
          &TabGeometry::lblGeometry_mouseClicked);
  connect(ui->btnAddCompartment, &QPushButton::clicked, this,
          &TabGeometry::btnAddCompartment_clicked);
  connect(ui->btnRemoveCompartment, &QPushButton::clicked, this,
          &TabGeometry::btnRemoveCompartment_clicked);
  connect(ui->btnChangeCompartment, &QPushButton::clicked, this,
          &TabGeometry::btnChangeCompartment_clicked);
  connect(ui->txtCompartmentName, &QLineEdit::editingFinished, this,
          &TabGeometry::txtCompartmentName_editingFinished);
  connect(ui->tabCompartmentGeometry, &QTabWidget::currentChanged, this,
          &TabGeometry::tabCompartmentGeometry_currentChanged);
  connect(ui->lblCompBoundary, &QLabelMouseTracker::mouseClicked, this,
          &TabGeometry::lblCompBoundary_mouseClicked);
  connect(ui->lblCompBoundary, &QLabelMouseTracker::mouseWheelEvent, this,
          [this](QWheelEvent *ev) {
            QApplication::sendEvent(ui->spinMaxBoundaryPoints, ev);
          });
  connect(ui->spinBoundaryIndex, qOverload<int>(&QSpinBox::valueChanged), this,
          &TabGeometry::spinBoundaryIndex_valueChanged);
  connect(ui->spinMaxBoundaryPoints, qOverload<int>(&QSpinBox::valueChanged),
          this, &TabGeometry::spinMaxBoundaryPoints_valueChanged);
  connect(ui->spinBoundaryZoom, qOverload<int>(&QSpinBox::valueChanged),
          this, &TabGeometry::spinBoundaryZoom_valueChanged);
  connect(ui->lblCompMesh, &QLabelMouseTracker::mouseClicked, this,
          &TabGeometry::lblCompMesh_mouseClicked);
  connect(ui->lblCompMesh, &QLabelMouseTracker::mouseWheelEvent, this,
          [this](QWheelEvent *ev) {
            QApplication::sendEvent(ui->spinMaxTriangleArea, ev);
          });
  connect(ui->spinMaxTriangleArea, qOverload<int>(&QSpinBox::valueChanged),
          this, &TabGeometry::spinMaxTriangleArea_valueChanged);
  connect(ui->spinMeshZoom, qOverload<int>(&QSpinBox::valueChanged),
          this, &TabGeometry::spinMeshZoom_valueChanged);
  connect(ui->listCompartments, &QListWidget::itemSelectionChanged, this,
          &TabGeometry::listCompartments_itemSelectionChanged);
  connect(ui->listCompartments, &QListWidget::itemDoubleClicked, this,
          &TabGeometry::listCompartments_itemDoubleClicked);
  connect(ui->listMembranes, &QListWidget::itemSelectionChanged, this,
          &TabGeometry::listMembranes_itemSelectionChanged);
}

TabGeometry::~TabGeometry() = default;

void TabGeometry::loadModelData(const QString &selection) {
  ui->tabCompartmentGeometry->setCurrentIndex(0);
  ui->listMembranes->clear();
  ui->listMembranes->addItems(model.getMembranes().getNames());
  ui->listCompartments->clear();
  ui->lblCompShape->clear();
  ui->lblCompBoundary->clear();
  ui->lblCompMesh->clear();
  ui->lblCompartmentColour->clear();
  ui->txtCompartmentName->clear();
  ui->listCompartments->addItems(model.getCompartments().getNames());
  ui->btnChangeCompartment->setEnabled(false);
  ui->txtCompartmentName->setEnabled(false);
  if (ui->listCompartments->count() > 0) {
    ui->txtCompartmentName->setEnabled(true);
    ui->listCompartments->setCurrentRow(0);
    ui->btnChangeCompartment->setEnabled(true);
  }
  lblGeometry->setImage(model.getGeometry().getImage());
  lblGeometry->getImage().save("TT.png");
  enableTabs();
  selectMatchingOrFirstItem(ui->listCompartments, selection);
}

void TabGeometry::enableTabs() {
  bool enableBoundaries = model.getGeometry().getIsValid();
  bool enableMesh = model.getGeometry().getMesh() != nullptr &&
                    model.getGeometry().getMesh()->isValid();
  auto *tab = ui->tabCompartmentGeometry;
  tab->setTabEnabled(1, enableBoundaries);
  tab->setTabEnabled(2, enableBoundaries);
  tab->setTabEnabled(3, enableMesh);
  ui->listMembranes->setEnabled(enableBoundaries);
}

void TabGeometry::lblGeometry_mouseClicked(QRgb col, QPoint point) {
  if (waitingForCompartmentChoice) {
    SPDLOG_INFO("colour {:x}", col);
    SPDLOG_INFO("point ({},{})", point.x(), point.y());
    // update compartment geometry (i.e. colour) of selected compartment to
    // the one the user just clicked on
    try {
      const auto &compartmentID = model.getCompartments().getIds().at(
          ui->listCompartments->currentRow());
      model.getCompartments().setColour(compartmentID, col);
      ui->tabCompartmentGeometry->setCurrentIndex(0);
      ui->listMembranes->clear();
      ui->listMembranes->addItems(model.getMembranes().getNames());
    } catch (const std::runtime_error &e) {
      QMessageBox::warning(this, "Mesh generation failed", e.what());
    }
    // update display by simulating user click on listCompartments
    listCompartments_itemSelectionChanged();
    waitingForCompartmentChoice = false;
    statusBarPermanentMessage->clear();
    enableTabs();
    emit modelGeometryChanged();
    return;
  }
  // display compartment the user just clicked on
  auto compID = model.getCompartments().getIdFromColour(col);
  for (int i = 0; i < model.getCompartments().getIds().size(); ++i) {
    if (model.getCompartments().getIds().at(i) == compID) {
      ui->listCompartments->setCurrentRow(i);
    }
  }
}

void TabGeometry::btnAddCompartment_clicked() {
  bool ok;
  auto compartmentName = QInputDialog::getText(
      this, "Add compartment", "New compartment name:", QLineEdit::Normal, {},
      &ok);
  if (ok && !compartmentName.isEmpty()) {
    QString newCompartmentName = model.getCompartments().add(compartmentName);
    ui->tabCompartmentGeometry->setCurrentIndex(0);
    enableTabs();
    loadModelData(newCompartmentName);
    emit modelGeometryChanged();
  }
}

void TabGeometry::btnRemoveCompartment_clicked() {
  int index = ui->listCompartments->currentRow();
  if (index < 0 || index >= model.getCompartments().getIds().size()) {
    return;
  }
  const auto &compartmentName = ui->listCompartments->item(index)->text();
  const auto &compartmentId = model.getCompartments().getIds()[index];
  auto msgbox = newYesNoMessageBox(
      "Remove compartment?",
      QString("Remove compartment '%1' from the model?").arg(compartmentName),
      this);
  connect(msgbox, &QMessageBox::finished, this,
          [compartmentId, this](int result) {
            if (result == QMessageBox::Yes) {
              ui->listCompartments->clearSelection();
              model.getCompartments().remove(compartmentId);
              ui->tabCompartmentGeometry->setCurrentIndex(0);
              enableTabs();
              loadModelData();
              emit modelGeometryChanged();
            }
          });
  msgbox->open();
}

void TabGeometry::btnChangeCompartment_clicked() {
  if (!(model.getIsValid() && model.getGeometry().getHasImage())) {
    emit invalidModelOrNoGeometryImage();
    SPDLOG_DEBUG("invalid geometry and/or model: ignoring");
    return;
  }
  SPDLOG_DEBUG("waiting for user to click on geometry image..");
  waitingForCompartmentChoice = true;
  statusBarPermanentMessage->setText(
      "Please click on the desired location on the compartment geometry "
      "image...");
}

void TabGeometry::txtCompartmentName_editingFinished() {
  if (membraneSelected) {
    int membraneIndex{ui->listMembranes->currentRow()};
    if (membraneIndex < 0 ||
        membraneIndex >= model.getMembranes().getIds().size()) {
      // invalid index
      return;
    }
    if (ui->txtCompartmentName->text() ==
        model.getMembranes().getNames()[membraneIndex]) {
      // name is unchanged
      return;
    }
    const auto &membraneId{model.getMembranes().getIds().at(membraneIndex)};
    QString name{model.getMembranes().setName(membraneId,
                                              ui->txtCompartmentName->text())};
    ui->txtCompartmentName->setText(name);
    ui->listMembranes->item(membraneIndex)->setText(name);
    return;
  }
  // compartment
  int compIndex{ui->listCompartments->currentRow()};
  if (compIndex < 0 || compIndex >= model.getCompartments().getIds().size()) {
    return;
  }
  if (ui->txtCompartmentName->text() ==
      model.getCompartments().getNames()[compIndex]) {
    return;
  }
  const auto &compartmentId{model.getCompartments().getIds().at(compIndex)};
  QString name{model.getCompartments().setName(compartmentId,
                                               ui->txtCompartmentName->text())};
  ui->txtCompartmentName->setText(name);
  ui->listCompartments->item(compIndex)->setText(name);
  // changing a compartment name may change membrane names
  ui->listMembranes->clear();
  ui->listMembranes->addItems(model.getMembranes().getNames());
}

void TabGeometry::tabCompartmentGeometry_currentChanged(int index) {
  enum TabIndex { IMAGE = 0, BOUNDARIES = 1, MESH = 2 };
  SPDLOG_DEBUG("Tab changed to {} [{}]", index,
               ui->tabCompartmentGeometry->tabText(index).toStdString());
  if (index == TabIndex::IMAGE) {
    return;
  }
  if (index == TabIndex::BOUNDARIES) {
    auto size = model.getGeometry().getMesh()->getNumBoundaries();
    if (size == 0) {
      ui->spinBoundaryIndex->setEnabled(false);
      ui->spinMaxBoundaryPoints->setEnabled(false);
      ui->spinBoundaryZoom->setEnabled(false);
      return;
    }
    ui->spinBoundaryIndex->setMaximum(static_cast<int>(size) - 1);
    ui->spinBoundaryIndex->setEnabled(true);
    ui->spinMaxBoundaryPoints->setEnabled(true);
    ui->spinBoundaryZoom->setEnabled(true);
    spinBoundaryIndex_valueChanged(ui->spinBoundaryIndex->value());
    return;
  }
  if (index == TabIndex::MESH) {
    if(model.getGeometry().getMesh() == nullptr){
      ui->spinMaxTriangleArea->setEnabled(false);
      ui->spinMeshZoom->setEnabled(false);
    }
    ui->spinMaxTriangleArea->setEnabled(true);
    ui->spinMeshZoom->setEnabled(true);
    auto compIndex =
        static_cast<std::size_t>(ui->listCompartments->currentRow());
    ui->spinMaxTriangleArea->setValue(static_cast<int>(
        model.getGeometry().getMesh()->getCompartmentMaxTriangleArea(
            compIndex)));
    spinMaxTriangleArea_valueChanged(ui->spinMaxTriangleArea->value());
  }
}

void TabGeometry::lblCompBoundary_mouseClicked(QRgb col, QPoint point) {
  Q_UNUSED(col);
  Q_UNUSED(point);
  auto index = ui->lblCompBoundary->getMaskIndex();
  if (index <= ui->spinBoundaryIndex->maximum() &&
      index != ui->spinBoundaryIndex->value()) {
    ui->spinBoundaryIndex->setValue(index);
  }
}

void TabGeometry::spinBoundaryIndex_valueChanged(int value) {
  const auto &size = ui->lblCompBoundary->size();
  auto boundaryIndex = static_cast<size_t>(value);
  ui->spinMaxBoundaryPoints->setValue(static_cast<int>(
      model.getGeometry().getMesh()->getBoundaryMaxPoints(boundaryIndex)));
  ui->lblCompBoundary->setImages(
      model.getGeometry().getMesh()->getBoundariesImages(size, boundaryIndex));
}

void TabGeometry::spinMaxBoundaryPoints_valueChanged(int value) {
  const auto &size = ui->lblCompBoundary->size();
  auto boundaryIndex = static_cast<std::size_t>(ui->spinBoundaryIndex->value());
  model.getGeometry().getMesh()->setBoundaryMaxPoints(
      boundaryIndex, static_cast<size_t>(value));
  ui->lblCompBoundary->setImages(
      model.getGeometry().getMesh()->getBoundariesImages(size, boundaryIndex));
}

static QSize zoomedSize(const QSize originalSize, int zoomFactor){
  QSize newSize{originalSize};
  for (int i = 0; i < zoomFactor; ++i) {
    // 20% increase in size versus scroll region for each step
    newSize *= 6;
    newSize /= 5;
  }
  return newSize;
}

void TabGeometry::spinBoundaryZoom_valueChanged(int value) {
  if(value == 0){
    // rescale to fit entire scroll region
    ui->scrollBoundaryLines->setWidgetResizable(true);
  } else {
    ui->scrollBoundaryLines->setWidgetResizable(false);
    auto sz{zoomedSize(ui->scrollBoundaryLines->size(), value)};
    ui->scrollBoundaryLinesContents->resize(sz);
  }
  spinBoundaryIndex_valueChanged(ui->spinBoundaryIndex->value());
}

void TabGeometry::lblCompMesh_mouseClicked(QRgb col, QPoint point) {
  Q_UNUSED(col);
  Q_UNUSED(point);
  auto index = ui->lblCompMesh->getMaskIndex();
  SPDLOG_TRACE("Point ({},{}), Mask index {}", point.x(), point.y(), index);
  auto membraneIndex = static_cast<int>(index) - ui->listCompartments->count();
  if (index >= 0 && index < ui->listCompartments->count()) {
    ui->listCompartments->setCurrentRow(index);
    ui->spinMaxTriangleArea->setFocus();
    ui->spinMaxTriangleArea->selectAll();
    return;
  }
  if (membraneIndex >= 0 && membraneIndex < ui->listMembranes->count()) {
    ui->listMembranes->setCurrentRow(membraneIndex);
    return;
  }
}

void TabGeometry::spinMaxTriangleArea_valueChanged(int value) {
  const auto &size = ui->lblCompMesh->size();
  auto compIndex = static_cast<std::size_t>(ui->listCompartments->currentRow());
  model.getGeometry().getMesh()->setCompartmentMaxTriangleArea(
      compIndex, static_cast<std::size_t>(value));
  ui->lblCompMesh->setImages(
      model.getGeometry().getMesh()->getMeshImages(size, compIndex));
}

void TabGeometry::spinMeshZoom_valueChanged(int value){
  if(value == 0){
    // rescale to fit entire scroll region
    ui->scrollMesh->setWidgetResizable(true);
  } else {
    ui->scrollMesh->setWidgetResizable(false);
    auto sz{zoomedSize(ui->scrollMesh->size(), value)};
    ui->scrollMeshContents->resize(sz);
  }
  spinMaxTriangleArea_valueChanged(ui->spinMaxTriangleArea->value());
}

void TabGeometry::listCompartments_itemSelectionChanged() {
  auto items{ui->listCompartments->selectedItems()};
  if (items.isEmpty()) {
    ui->btnRemoveCompartment->setEnabled(false);
    ui->btnChangeCompartment->setEnabled(false);
    return;
  }
  ui->listMembranes->clearSelection();
  int currentRow{ui->listCompartments->row(items[0])};
  ui->txtCompartmentName->clear();
  ui->lblCompSize->clear();
  membraneSelected = false;
  const QString &compId{model.getCompartments().getIds()[currentRow]};
  ui->btnChangeCompartment->setEnabled(true);
  ui->btnRemoveCompartment->setEnabled(true);
  SPDLOG_DEBUG("row {} selected", currentRow);
  SPDLOG_DEBUG("  - Compartment Name: {}",
               ui->listCompartments->currentItem()->text().toStdString());
  SPDLOG_DEBUG("  - Compartment Id: {}", compId.toStdString());
  ui->txtCompartmentName->setEnabled(true);
  ui->txtCompartmentName->setText(model.getCompartments().getName(compId));
  QRgb col{model.getCompartments().getColour(compId)};
  SPDLOG_DEBUG("  - Compartment colour {:x} ", col);
  if (col == 0) {
    // null (transparent) RGB colour: compartment does not have
    // an assigned colour in the image
    ui->lblCompShape->setImage(QImage());
    ui->lblCompartmentColour->setText("none");
    ui->lblCompShape->setImage(QImage());
    ui->lblCompShape->setText(
        "<p>Compartment has no assigned geometry</p> "
        "<ul><li>please click on the 'Select compartment geometry...' "
        "button below</li> "
        "<li> then on the desired location in the geometry "
        "image on the left</li></ul>");
    ui->lblCompMesh->setImage(QImage());
    ui->lblCompMesh->setText("none");
  } else {
    // update colour box
    QImage img(1, 1, QImage::Format_RGB32);
    img.setPixel(0, 0, col);
    ui->lblCompartmentColour->setPixmap(QPixmap::fromImage(img));
    ui->lblCompartmentColour->setText("");
    // update image of compartment
    const auto *comp{model.getCompartments().getCompartment(compId)};
    ui->lblCompShape->setImage(comp->getCompartmentImage());
    ui->lblCompShape->setText("");
    // update mesh or boundary image if tab is currently visible
    tabCompartmentGeometry_currentChanged(
        ui->tabCompartmentGeometry->currentIndex());
    // update compartment size
    auto nPixels = comp->nPixels();
    double area = static_cast<double>(nPixels) *
                  std::pow(model.getGeometry().getPixelWidth(), 2);
    ui->lblCompSize->setText(QString("Area: %1 %2^2 (%3 pixels)")
                                 .arg(QString::number(area, 'g', 13))
                                 .arg(model.getUnits().getLength().name)
                                 .arg(nPixels));
  }
}

void TabGeometry::listCompartments_itemDoubleClicked(QListWidgetItem *item) {
  // double-click on compartment list item is equivalent to
  // selecting item, then clicking on btnChangeCompartment
  if (item != nullptr) {
    btnChangeCompartment_clicked();
  }
}

void TabGeometry::listMembranes_itemSelectionChanged() {
  auto items{ui->listMembranes->selectedItems()};
  if (items.isEmpty()) {
    return;
  }
  int currentRow{ui->listMembranes->row(items[0])};
  membraneSelected = true;
  ui->listCompartments->clearSelection();
  ui->txtCompartmentName->clear();
  ui->txtCompartmentName->setEnabled(true);
  const QString &membraneId{model.getMembranes().getIds()[currentRow]};
  SPDLOG_DEBUG("row {} selected", currentRow);
  SPDLOG_DEBUG("  - Membrane Name: {}",
               ui->listMembranes->currentItem()->text().toStdString());
  SPDLOG_DEBUG("  - Membrane Id: {}", membraneId.toStdString());
  ui->txtCompartmentName->setText(ui->listMembranes->currentItem()->text());
  // update image
  const auto *m{model.getMembranes().getMembrane(membraneId)};
  ui->lblCompShape->setImage(m->getImage());
  auto nPixels{m->getIndexPairs().size()};
  // update colour box
  QImage img(1, 2, QImage::Format_RGB32);
  img.setPixel(0, 0, m->getCompartmentA()->getColour());
  img.setPixel(0, 1, m->getCompartmentB()->getColour());
  ui->lblCompartmentColour->setPixmap(QPixmap::fromImage(img));
  ui->lblCompartmentColour->setText("");
  // update membrane length
  double length =
      static_cast<double>(nPixels) * model.getGeometry().getPixelWidth();
  ui->lblCompSize->setText(QString("Length: %1 %2 (%3 pixels)")
                               .arg(QString::number(length, 'g', 13))
                               .arg(model.getUnits().getLength().name)
                               .arg(nPixels));
  ui->lblCompMesh->setImages(model.getGeometry().getMesh()->getMeshImages(
      ui->lblCompMesh->size(),
      static_cast<std::size_t>(currentRow + ui->listCompartments->count())));
}

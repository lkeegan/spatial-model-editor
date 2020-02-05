#include "qtabsimulate.hpp"

#include <qcustomplot.h>

#include <QElapsedTimer>
#include <algorithm>

#include "dialogdisplayoptions.hpp"
#include "dialogimageslice.hpp"
#include "logger.hpp"
#include "qlabelmousetracker.hpp"
#include "sbml.hpp"
#include "ui_qtabsimulate.h"

QTabSimulate::QTabSimulate(sbml::SbmlDocWrapper &doc,
                           QLabelMouseTracker *mouseTracker, QWidget *parent)
    : QWidget(parent),
      ui{std::make_unique<Ui::QTabSimulate>()},
      sbmlDoc(doc),
      lblGeometry(mouseTracker) {
  ui->setupUi(this);
  pltPlot = new QCustomPlot(this);
  pltTitle = new QCPTextElement(
      pltPlot, "Average Concentration",
      QFont(font().family(), font().pointSize() + 4, QFont::Bold));
  pltPlot->setInteraction(QCP::iRangeDrag, true);
  pltPlot->setInteraction(QCP::iRangeZoom, true);
  pltPlot->setInteraction(QCP::iSelectPlottables, false);
  pltPlot->legend->setVisible(true);
  pltTimeLine = new QCPItemStraightLine(pltPlot);
  pltTimeLine->setVisible(false);
  pltPlot->setObjectName(QString::fromUtf8("pltPlot"));
  pltPlot->plotLayout()->insertRow(0);
  pltPlot->plotLayout()->addElement(0, 0, pltTitle);

  ui->gridSimulate->addWidget(pltPlot, 1, 0, 1, 9);

  connect(ui->btnSimulate, &QPushButton::clicked, this,
          &QTabSimulate::btnSimulate_clicked);
  connect(ui->btnResetSimulation, &QPushButton::clicked, this,
          &QTabSimulate::reset);
  connect(pltPlot, &QCustomPlot::mousePress, this, &QTabSimulate::graphClicked);
  connect(ui->hslideTime, &QSlider::valueChanged, this,
          &QTabSimulate::hslideTime_valueChanged);
  connect(ui->btnStopSimulation, &QPushButton::clicked, this,
          &QTabSimulate::stopSimulation);
  connect(ui->btnSliceImage, &QPushButton::clicked, this,
          &QTabSimulate::btnSliceImage_clicked);
  connect(ui->btnDisplayOptions, &QPushButton::clicked, this,
          &QTabSimulate::btnDisplayOptions_clicked);

  ui->hslideTime->setEnabled(false);
}

QTabSimulate::~QTabSimulate() = default;

void QTabSimulate::loadModelData() {
  if (sim == nullptr) {
    reset();
  } else {
    hslideTime_valueChanged(ui->hslideTime->value());
  }
}

void QTabSimulate::stopSimulation() {
  SPDLOG_INFO("Simulation cancelled by user");
  isSimulationRunning = false;
}

void QTabSimulate::useDune(bool enable) {
  if (enable) {
    simType = simulate::SimulatorType::DUNE;
  } else {
    simType = simulate::SimulatorType::Pixel;
  }
  reset();
}

void QTabSimulate::reset() {
  pltPlot->clearGraphs();
  pltPlot->replot();
  ui->hslideTime->setMinimum(0);
  ui->hslideTime->setMaximum(0);
  images.clear();
  time.clear();
  normaliseImageIntensityOverWholeSimulation = false;
  if (!(sbmlDoc.isValid && sbmlDoc.hasValidGeometry)) {
    ui->hslideTime->setEnabled(false);
    return;
  }
  sim = std::make_unique<simulate::Simulation>(sbmlDoc, simType);

  // setup species names
  speciesNames.clear();
  speciesVisible.clear();
  compartmentNames.clear();
  for (std::size_t ic = 0; ic < sim->getCompartmentIds().size(); ++ic) {
    compartmentNames.push_back(sbmlDoc.compartmentNames[static_cast<int>(ic)]);
    auto &names = speciesNames.emplace_back();
    for (std::size_t is = 0; is < sim->getSpeciesIds(ic).size(); ++is) {
      names.push_back(
          sbmlDoc.getSpeciesName(sim->getSpeciesIds(ic)[is].c_str()));
      speciesVisible.push_back(true);
    }
  }
  // setup plot
  pltPlot->clearGraphs();
  pltPlot->xAxis->setLabel(
      QString("time (%1)").arg(sbmlDoc.getModelUnits().getTime().symbol));
  pltPlot->yAxis->setLabel(
      QString("concentration (%1)")
          .arg(sbmlDoc.getModelUnits().getConcentration()));
  pltPlot->xAxis->setRange(0, ui->txtSimLength->text().toDouble());
  // graphs
  for (std::size_t ic = 0; ic < sim->getCompartmentIds().size(); ++ic) {
    for (std::size_t is = 0; is < sim->getSpeciesIds(ic).size(); ++is) {
      QColor col = sim->getSpeciesColors(ic)[is];
      QString name = sbmlDoc.getSpeciesName(sim->getSpeciesIds(ic)[is].c_str());
      // avg
      auto *av = pltPlot->addGraph();
      av->setPen(col);
      av->setName(QString("%1 (average)").arg(name));
      av->setScatterStyle(
          QCPScatterStyle(QCPScatterStyle::ScatterShape::ssDisc));
      // min
      auto *min = pltPlot->addGraph();
      col.setAlpha(30);
      min->setPen(col);
      min->setBrush(QBrush(col));
      min->setName(QString("%1 (min/max range)").arg(name));
      // max
      auto *max = pltPlot->addGraph();
      max->setPen(col);
      min->setChannelFillGraph(max);
      pltPlot->legend->removeItem(pltPlot->legend->itemCount() - 1);
    }
  }

  time.push_back(0);
  // get initial concentrations
  int speciesIndex = 0;
  for (std::size_t ic = 0; ic < sim->getCompartmentIds().size(); ++ic) {
    for (std::size_t is = 0; is < sim->getSpeciesIds(ic).size(); ++is) {
      auto conc = sim->getAvgMinMax(0, ic, is);
      pltPlot->graph(3 * speciesIndex)->setData({0}, {conc.avg}, true);
      pltPlot->graph(3 * speciesIndex + 1)->setData({0}, {conc.min}, true);
      pltPlot->graph(3 * speciesIndex + 2)->setData({0}, {conc.max}, true);
      ++speciesIndex;
    }
  }
  speciesVisible.resize(static_cast<std::size_t>(speciesIndex), true);
  updateSpeciesToDraw();

  images.push_back(sim->getConcImage(0));
  ui->hslideTime->setEnabled(true);
  ui->hslideTime->setValue(0);
}

void QTabSimulate::btnSimulate_clicked() {
  // integration time parameters
  double dt = ui->txtSimDt->text().toDouble();
  double dtImage = ui->txtSimInterval->text().toDouble();
  int n_images =
      static_cast<int>(ui->txtSimLength->text().toDouble() / dtImage);

  QElapsedTimer qElapsedTimer;
  qElapsedTimer.start();
  isSimulationRunning = true;
  this->setCursor(Qt::WaitCursor);
  QApplication::processEvents();
  // integrate Model
  for (int i_image = 0; i_image < n_images; ++i_image) {
    sim->doTimestep(dtImage, dt);
    QApplication::processEvents();
    if (!isSimulationRunning) {
      break;
    }
    images.push_back(sim->getConcImage(sim->getTimePoints().size() - 1,
                                       compartmentSpeciesToDraw));
    time.push_back(time.back() + dtImage);
    int speciesIndex = 0;
    for (std::size_t ic = 0; ic < sim->getCompartmentIds().size(); ++ic) {
      for (std::size_t is = 0; is < sim->getSpeciesIds(ic).size(); ++is) {
        auto conc = sim->getAvgMinMax(static_cast<std::size_t>(time.size() - 1),
                                      ic, is);
        pltPlot->graph(3 * speciesIndex)
            ->addData({time.back()}, {conc.avg}, true);
        pltPlot->graph(3 * speciesIndex + 1)
            ->addData({time.back()}, {conc.min}, true);
        pltPlot->graph(3 * speciesIndex + 2)
            ->addData({time.back()}, {conc.max}, true);
        ++speciesIndex;
      }
    }
    lblGeometry->setImage(images.back());
    // rescale & replot plot
    pltPlot->rescaleAxes(true);
    pltPlot->replot();
  }

  // display vertical at current time point
  pltTimeLine->setVisible(true);
  pltTimeLine->point1->setCoords(time.back(), 0);
  pltTimeLine->point2->setCoords(time.back(), 1);

  // rescale & replot plot
  pltPlot->xAxis->rescale();
  pltPlot->replot();

  // enable slider to choose time to display
  ui->hslideTime->setEnabled(true);
  ui->hslideTime->setMinimum(0);
  ui->hslideTime->setMaximum(time.size() - 1);
  ui->hslideTime->setValue(time.size() - 1);

  // ui->statusBar->showMessage("Simulation complete.");
  SPDLOG_INFO("simulation run-time: {}s",
              static_cast<double>(qElapsedTimer.elapsed()) / 1000.0);
  this->setCursor(Qt::ArrowCursor);
}

void QTabSimulate::btnSliceImage_clicked() {
  DialogImageSlice dialog(images, time);
  if (dialog.exec() == QDialog::Accepted) {
    SPDLOG_DEBUG("do something");
  }
}

void QTabSimulate::updateSpeciesToDraw() {
  compartmentSpeciesToDraw.clear();
  std::size_t speciesIndex = 0;
  for (const auto &compSpecies : speciesNames) {
    auto &speciesToDraw = compartmentSpeciesToDraw.emplace_back();
    for (std::size_t i = 0; i < static_cast<std::size_t>(compSpecies.size());
         ++i) {
      if (speciesVisible[speciesIndex]) {
        speciesToDraw.push_back(i);
      }
      ++speciesIndex;
    }
  }
}

void QTabSimulate::btnDisplayOptions_clicked() {
  int norm = normaliseImageIntensityOverWholeSimulation ? 1 : 0;
  DialogDisplayOptions dialog(compartmentNames, speciesNames, speciesVisible,
                              plotShowMinMax, norm);
  if (dialog.exec() == QDialog::Accepted) {
    plotShowMinMax = dialog.getShowMinMax();
    speciesVisible = dialog.getShowSpecies();
    normaliseImageIntensityOverWholeSimulation =
        dialog.getNormalisationType() == 1;
    // update plot
    int iSpecies = 0;
    pltPlot->legend->clearItems();
    for (bool visible : speciesVisible) {
      bool minMaxVisible = visible && plotShowMinMax;
      pltPlot->graph(3 * iSpecies)->setVisible(visible);
      if (visible) {
        pltPlot->graph(3 * iSpecies)->addToLegend();
      }
      pltPlot->graph(3 * iSpecies + 1)->setVisible(minMaxVisible);
      pltPlot->graph(3 * iSpecies + 2)->setVisible(minMaxVisible);
      if (minMaxVisible) {
        pltPlot->graph(3 * iSpecies + 1)->addToLegend();
      }
      ++iSpecies;
    }
    pltPlot->rescaleAxes(true);
    pltPlot->replot();
    updateSpeciesToDraw();
    // update images
    for (int iTime = 0; iTime < time.size(); ++iTime) {
      images[iTime] = sim->getConcImage(
          static_cast<std::size_t>(iTime), compartmentSpeciesToDraw,
          normaliseImageIntensityOverWholeSimulation);
    }
    hslideTime_valueChanged(ui->hslideTime->value());
  }
}

void QTabSimulate::graphClicked(const QMouseEvent *event) {
  if (pltPlot->graphCount() == 0) {
    return;
  }
  double key;
  double val;
  pltPlot->graph(0)->pixelsToCoords(static_cast<double>(event->x()),
                                    static_cast<double>(event->y()), key, val);
  int max = ui->hslideTime->maximum();
  int nearest = static_cast<int>(0.5 + max * key / time.back());
  ui->hslideTime->setValue(std::clamp(nearest, 0, max));
}

void QTabSimulate::hslideTime_valueChanged(int value) {
  if (images.size() > value) {
    lblGeometry->setImage(images[value]);
    pltTimeLine->point1->setCoords(time[value], 0);
    pltTimeLine->point2->setCoords(time[value], 1);
    if (!pltPlot->xAxis->range().contains(time[value])) {
      pltPlot->rescaleAxes(true);
    }
    pltPlot->replot();
    ui->lblCurrentTime->setText(
        QString("%1%2")
            .arg(time[value])
            .arg(sbmlDoc.getModelUnits().getTime().symbol));
  }
}

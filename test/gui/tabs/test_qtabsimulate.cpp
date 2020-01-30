#include <qcustomplot.h>

#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>

#include "catch_wrapper.hpp"
#include "qlabelmousetracker.hpp"
#include "qt_test_utils.hpp"
#include "qtabsimulate.hpp"
#include "sbml.hpp"

SCENARIO("Simulate Tab", "[gui][tabs][simulate]") {
  sbml::SbmlDocWrapper sbmlDoc;
  QLabelMouseTracker mouseTracker;
  auto tab = QTabSimulate(sbmlDoc, &mouseTracker);
  tab.show();
  waitFor(&tab);
  ModalWidgetTimer mwt;
  // get pointers to widgets within tab
  auto *txtSimLength = tab.findChild<QLineEdit *>("txtSimLength");
  auto *txtSimInterval = tab.findChild<QLineEdit *>("txtSimInterval");
  auto *txtSimDt = tab.findChild<QLineEdit *>("txtSimDt");
  auto *btnSimulate = tab.findChild<QPushButton *>("btnSimulate");
  auto *btnResetSimulation = tab.findChild<QPushButton *>("btnResetSimulation");
  auto *hslideTime = tab.findChild<QSlider *>("hslideTime");
  auto *pltPlot = tab.findChild<QCustomPlot *>("pltPlot");
  REQUIRE(pltPlot != nullptr);

  if (QFile f(":/models/ABtoC.xml"); f.open(QIODevice::ReadOnly)) {
    sbmlDoc.importSBMLString(f.readAll().toStdString());
  }
  tab.loadModelData();
  txtSimLength->setFocus();

  // do DUNE simulation with two timesteps of 0.1
  REQUIRE(hslideTime->isEnabled() == false);
  REQUIRE(pltPlot->graphCount() == 0);
  sendKeyEvents(txtSimLength,
                {"End", "Backspace", "Backspace", "Backspace", "0", ".", "2"});
  sendKeyEvents(txtSimInterval,
                {"End", "Backspace", "Backspace", "Backspace", "0", ".", "1"});
  sendKeyEvents(txtSimDt, {"End", "Backspace", "Backspace", "Backspace",
                           "Backspace", "0", ".", "1"});
  sendMouseClick(btnSimulate);
  REQUIRE(hslideTime->isEnabled() == true);
  REQUIRE(hslideTime->maximum() == 2);
  REQUIRE(hslideTime->minimum() == 0);
  REQUIRE(pltPlot->graphCount() == 9);

  sendMouseClick(btnResetSimulation);
  REQUIRE(hslideTime->isEnabled() == false);
  REQUIRE(pltPlot->graphCount() == 0);

  // repeat simulation using Pixel simulator
  tab.useDune(false);
  sendMouseClick(btnSimulate);
  REQUIRE(hslideTime->isEnabled() == true);
  REQUIRE(hslideTime->maximum() == 2);
  REQUIRE(hslideTime->minimum() == 0);
  REQUIRE(pltPlot->graphCount() == 9);

  sendMouseClick(btnResetSimulation);
  REQUIRE(hslideTime->isEnabled() == false);
  REQUIRE(pltPlot->graphCount() == 0);
}

#include <QDoubleSpinBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>

#include "catch_wrapper.hpp"
#include "qlabelmousetracker.hpp"
#include "qt_test_utils.hpp"
#include "qtabgeometry.hpp"
#include "sbml.hpp"

SCENARIO("Geometry Tab", "[gui][tabs][geometry]") {
  sbml::SbmlDocWrapper sbmlDoc;
  QLabelMouseTracker mouseTracker;
  mouseTracker.show();
  waitFor(&mouseTracker);
  QLabel statusBarMsg;
  auto tab = QTabGeometry(sbmlDoc, &mouseTracker, &statusBarMsg);
  tab.show();
  waitFor(&tab);
  ModalWidgetTimer mwt;
  // get pointers to widgets within tab
  auto *listCompartments = tab.findChild<QListWidget *>("listCompartments");
  auto *txtCompartmentSize = tab.findChild<QLineEdit *>("txtCompartmentSize");
  auto *btnSetCompartmentSizeFromImage =
      tab.findChild<QPushButton *>("btnSetCompartmentSizeFromImage");
  auto *tabCompartmentGeometry =
      tab.findChild<QTabWidget *>("tabCompartmentGeometry");
  auto *lblCompShape = tab.findChild<QLabelMouseTracker *>("lblCompShape");
  // auto *btnChangeCompartment = tab.findChild<QPushButton
  // *>("btnChangeCompartment");
  // auto *lblCompBoundary =
  //     tab.findChild<QLabelMouseTracker *>("lblCompBoundary");
  auto *spinBoundaryIndex = tab.findChild<QSpinBox *>("spinBoundaryIndex");
  auto *spinMaxBoundaryPoints =
      tab.findChild<QSpinBox *>("spinMaxBoundaryPoints");
  auto *spinBoundaryWidth =
      tab.findChild<QDoubleSpinBox *>("spinBoundaryWidth");
  // auto *lblCompMesh = tab.findChild<QLabelMouseTracker *>("lblCompMesh");
  auto *spinMaxTriangleArea = tab.findChild<QSpinBox *>("spinMaxTriangleArea");
  WHEN("very-simple-model loaded") {
    if (QFile f(":/models/very-simple-model.xml");
        f.open(QIODevice::ReadOnly)) {
      sbmlDoc.importSBMLString(f.readAll().toStdString());
    }
    tab.loadModelData();

    // select compartments
    REQUIRE(listCompartments->count() == 3);
    REQUIRE(listCompartments->currentItem()->text() == "Outside");
    REQUIRE(txtCompartmentSize->text() == "10");
    REQUIRE(tabCompartmentGeometry->currentIndex() == 0);
    REQUIRE(lblCompShape->getImage().pixel(1, 1) == 0xff000200);
    sendMouseClick(btnSetCompartmentSizeFromImage);
    REQUIRE(txtCompartmentSize->text() != "10");
    listCompartments->setFocus();
    sendKeyEvents(listCompartments, {"Down"});
    REQUIRE(listCompartments->currentItem()->text() == "Cell");
    REQUIRE(lblCompShape->getImage().pixel(20, 20) == 0xff9061c1);
    REQUIRE(txtCompartmentSize->text() == "1");
    sendKeyEvents(listCompartments, {"Down"});
    REQUIRE(listCompartments->currentItem()->text() == "Nucleus");
    REQUIRE(txtCompartmentSize->text() == "0.2");
    REQUIRE(lblCompShape->getImage().pixel(50, 50) == 0xffc58560);

    // boundary tab
    tabCompartmentGeometry->setFocus();
    sendKeyEvents(tabCompartmentGeometry, {"Ctrl+Tab"});
    REQUIRE(tabCompartmentGeometry->currentIndex() == 1);
    spinBoundaryIndex->setFocus();
    sendKeyEvents(spinBoundaryIndex, {"Up"});
    sendKeyEvents(spinMaxBoundaryPoints, {"Up"});
    sendKeyEvents(spinMaxBoundaryPoints, {"Up"});
    sendKeyEvents(spinMaxBoundaryPoints, {"Up"});
    sendKeyEvents(spinBoundaryIndex, {"Up"});
    sendKeyEvents(spinMaxBoundaryPoints, {"Down"});
    sendKeyEvents(spinBoundaryWidth, {"Down"});
    sendKeyEvents(spinBoundaryWidth, {"Down"});
    sendKeyEvents(spinBoundaryIndex, {"Up"});

    // mesh tab
    tabCompartmentGeometry->setFocus();
    sendKeyEvents(tabCompartmentGeometry, {"Ctrl+Tab"});
    REQUIRE(tabCompartmentGeometry->currentIndex() == 2);
    sendKeyEvents(spinMaxTriangleArea,
                  {"End", "Backspace", "Backspace", "Backspace", "3"});

    // image tab
    tabCompartmentGeometry->setFocus();
    sendKeyEvents(tabCompartmentGeometry, {"Ctrl+Tab"});
    REQUIRE(tabCompartmentGeometry->currentIndex() == 0);
  }
}

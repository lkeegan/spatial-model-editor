#include <sbml/SBMLTypes.h>
#include <sbml/extension/SBMLDocumentPlugin.h>
#include <sbml/packages/spatial/common/SpatialExtensionTypes.h>
#include <sbml/packages/spatial/extension/SpatialExtension.h>

#include <QFile>
#include <QMenu>

#include "catch_wrapper.hpp"
#include "mainwindow.hpp"
#include "qt_test_utils.hpp"

static void openBuiltInModel(MainWindow &w, const QString &shortcutKey = "V") {
  auto *menuFile = w.findChild<QMenu *>("menuFile");
  auto *menuOpen_example_SBML_file =
      w.findChild<QMenu *>("menuOpen_example_SBML_file");
  sendKeyEvents(&w, {"Alt+F"});
  sendKeyEvents(menuFile, {"E"});
  sendKeyEvents(menuOpen_example_SBML_file, {shortcutKey});
}

SCENARIO("Mainwindow: shortcut keys", "[gui][mainwindow][shortcuts]") {
  MainWindow w;
  auto *menuImport = w.findChild<QMenu *>("menuImport");
  auto *menuExample_geometry_image =
      w.findChild<QMenu *>("menuExample_geometry_image");
  auto *menu_Tools = w.findChild<QMenu *>("menu_Tools");
  auto *menuSimulation_type = w.findChild<QMenu *>("menuSimulation_type");
  auto *actionSimTypeDUNE = w.findChild<QAction *>("actionSimTypeDUNE");
  auto *actionSimTypePixel = w.findChild<QAction *>("actionSimTypePixel");
  auto *tabMain = w.findChild<QTabWidget *>("tabMain");
  w.show();
  waitFor(&w);
  ModalWidgetTimer mwt;
  WHEN("user presses F8") {
    THEN("open About dialog box") {
      mwt.start();
      sendKeyEvents(&w, {"F8"});
    }
  }
  WHEN("user presses F9") {
    THEN("open About Qt dialog box") {
      mwt.start();
      sendKeyEvents(&w, {"F9"});
      QString correctText = "<h3>About Qt</h3>";
      CAPTURE(mwt.getResult());
      REQUIRE(mwt.getResult().left(correctText.size()) == correctText);
    }
  }
  WHEN("user presses ctrl+n") {
    THEN("ask for name of new model") {
      mwt.addUserAction({"Esc"});
      mwt.start();
      sendKeyEvents(&w, {"Ctrl+N"});
      REQUIRE(w.windowTitle().right(6) == "Editor");
      mwt.addUserAction({"n", "e", "w"});
      mwt.start();
      sendKeyEvents(&w, {"Ctrl+N"});
      REQUIRE(w.windowTitle().right(9) == "[new.xml]");
    }
  }
  WHEN("user presses ctrl+o") {
    THEN("open AcceptOpen FileDialog") {
      mwt.addUserAction({"Esc"});
      mwt.start();
      sendKeyEvents(&w, {"Ctrl+O"});
      REQUIRE(mwt.getResult() == "QFileDialog::AcceptOpen");
    }
  }
  WHEN("user presses ctrl+s (no SBML model loaded)") {
    THEN("offer to import an SBML model") {
      sendKeyEvents(&w, {"Ctrl+S"});
      // press no when asked to import model
      auto title = sendKeyEventsToNextQDialog({"Esc"});
      REQUIRE(title == "No SBML model");
    }
  }
  WHEN("user presses ctrl+s (with valid SBML model)") {
    THEN("open AcceptSave FileDialog") {
      openBuiltInModel(w, "V");
      mwt.addUserAction(QStringList{"Escape"});
      mwt.start();
      sendKeyEvents(&w, {"Ctrl+S"});
      REQUIRE(mwt.getResult() == "QFileDialog::AcceptSave");
    }
  }
  WHEN("user presses ctrl+d (no SBML model loaded)") {
    THEN("offer to import an SBML model") {
      sendKeyEvents(&w, {"Ctrl+D"});
      // press no when asked to import model
      auto title = sendKeyEventsToNextQDialog({"Esc"});
      REQUIRE(title == "No SBML model");
    }
  }
  WHEN("user presses ctrl+d (with valid SBML model)") {
    THEN("open AcceptSave FileDialog") {
      openBuiltInModel(w, "A");
      mwt.addUserAction(QStringList{"Escape"});
      mwt.start();
      sendKeyEvents(&w, {"Ctrl+D"});
      REQUIRE(mwt.getResult() == "QFileDialog::AcceptSave");
    }
  }
  WHEN("user presses ctrl+I to import image (no SBML model)") {
    THEN("offer to import SBML model") {
      sendKeyEvents(&w, {"Ctrl+I"});
      // press no when asked to import model
      sendKeyEventsToNextQDialog({"Esc"});

      // repeat, this time say yes, but then escape on SBML file open dialog
      sendKeyEvents(&w, {"Ctrl+I"});
      mwt.addUserAction({"Esc"}, false);
      mwt.start();
      // press yes when asked to import model
      // NB: don't send release event, as dialog will be deleted by the
      // time it is sent, since the modal QFileDialog opens in between the press
      // & release events
      sendKeyEventsToNextQDialog({"Enter"}, false);
      REQUIRE(mwt.getResult() == "QFileDialog::AcceptOpen");
    }
  }
  WHEN("menu: Import->Example geometry image->Empty donut (no SBML model)") {
    THEN("offer to import SBML model") {
      sendKeyEvents(&w, {"Alt+I"});
      sendKeyEvents(menuImport, {"E"});
      sendKeyEvents(menuExample_geometry_image, {"E"});
      // press no when asked to import model
      sendKeyEventsToNextQDialog({"Esc"});
    }
  }
  WHEN("user presses ctrl+tab (no SBML & compartment image loaded)") {
    THEN("remain on Geometry tab: all others disabled") {
      REQUIRE(tabMain->currentIndex() == 0);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 0);
    }
  }
  WHEN("user presses ctrl+shift+tab (no SBML & compartment image loaded)") {
    THEN("remain on Geometry tab: all others disabled") {
      REQUIRE(tabMain->currentIndex() == 0);
      sendKeyEvents(tabMain, {"Ctrl+Shift+Tab"});
      REQUIRE(tabMain->currentIndex() == 0);
    }
  }
  WHEN("user presses ctrl+tab with valid model loaded") {
    THEN("cycle through tabs") {
      openBuiltInModel(w, "V");
      REQUIRE(tabMain->currentIndex() == 0);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 1);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 2);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 3);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 4);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 5);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 6);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 7);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 8);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 0);
      sendKeyEvents(tabMain, {"Ctrl+Tab"});
      REQUIRE(tabMain->currentIndex() == 1);
      sendKeyEvents(tabMain, {"Ctrl+Shift+Tab"});
      REQUIRE(tabMain->currentIndex() == 0);
      sendKeyEvents(tabMain, {"Ctrl+Shift+Tab"});
      REQUIRE(tabMain->currentIndex() == 8);
    }
  }
  WHEN("menu: Tools->Set model units (no SBML model)") {
    THEN("offer to import SBML model") {
      sendKeyEvents(&w, {"Alt+T"});
      sendKeyEvents(menu_Tools, {"U"});
      // press no when asked to import model
      sendKeyEventsToNextQDialog({"Esc"});
    }
  }
  WHEN("menu: Tools->Set image size (no SBML model)") {
    THEN("offer to import SBML model") {
      sendKeyEvents(&w, {"Alt+T"});
      sendKeyEvents(menu_Tools, {"I"});
      // press no when asked to import model
      sendKeyEventsToNextQDialog({"Esc"});
    }
  }
  WHEN("menu: Tools->Set simulation type") {
    REQUIRE(actionSimTypeDUNE->isChecked() == true);
    REQUIRE(actionSimTypePixel->isChecked() == false);
    sendKeyEvents(&w, {"Alt+T"});
    sendKeyEvents(menu_Tools, {"S"});
    sendKeyEvents(menuSimulation_type, {"P"});
    REQUIRE(actionSimTypeDUNE->isChecked() == false);
    REQUIRE(actionSimTypePixel->isChecked() == true);
    sendKeyEvents(&w, {"Alt+T"});
    sendKeyEvents(menu_Tools, {"S"});
    sendKeyEvents(menuSimulation_type, {"D"});
    REQUIRE(actionSimTypeDUNE->isChecked() == true);
    REQUIRE(actionSimTypePixel->isChecked() == false);
  }
}

SCENARIO("Mainwindow: load built-in SBML model, change units",
         "[gui][mainwindow][units]") {
  MainWindow w;
  w.show();
  waitFor(&w);
  openBuiltInModel(w);
  auto *menu_Tools = w.findChild<QMenu *>("menu_Tools");
  // change units
  ModalWidgetTimer mwt;
  sendKeyEvents(&w, {"Alt+T"});
  mwt.addUserAction({"Down", "Tab", "Down", "Tab", "Down", "Tab", "Down"});
  mwt.start();
  sendKeyEvents(menu_Tools, {"U"}, false);
  // save SBML file
  QFile::remove("units.xml");
  mwt.addUserAction({"u", "n", "i", "t", "s", ".", "x", "m", "l"});
  mwt.start();
  sendKeyEvents(&w, {"Ctrl+S"});
  // check units of SBML model
  std::unique_ptr<libsbml::SBMLDocument> doc(
      libsbml::readSBMLFromFile("units.xml"));
  const auto *model = doc->getModel();
  // millisecond
  const auto *timeunit =
      model->getUnitDefinition(model->getTimeUnits())->getUnit(0);
  REQUIRE(timeunit->isSecond() == true);
  REQUIRE(timeunit->getScale() == dbl_approx(-3));
  // decimetre
  const auto *lengthunit =
      model->getUnitDefinition(model->getLengthUnits())->getUnit(0);
  REQUIRE(lengthunit->isMetre() == true);
  REQUIRE(lengthunit->getScale() == dbl_approx(-1));
  // decilitre
  const auto *volunit =
      model->getUnitDefinition(model->getVolumeUnits())->getUnit(0);
  REQUIRE(volunit->isLitre() == true);
  REQUIRE(volunit->getScale() == dbl_approx(-1));
  // millimole
  const auto *amountunit =
      model->getUnitDefinition(model->getSubstanceUnits())->getUnit(0);
  REQUIRE(amountunit->isMole() == true);
  REQUIRE(amountunit->getScale() == dbl_approx(-3));
}

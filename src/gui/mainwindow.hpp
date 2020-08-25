#pragma once

#include <QMainWindow>
#include <memory>

#include "model.hpp"

class QLabel;
class TabFunctions;
class TabGeometry;
class TabParameters;
class TabReactions;
class TabSimulate;
class TabSpecies;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(const QString &filename = {}, QWidget *parent = nullptr);
  ~MainWindow();

private:
  std::unique_ptr<Ui::MainWindow> ui;
  QLabel *statusBarPermanentMessage;
  model::Model sbmlDoc;

  void setupTabs();
  void setupConnections();

  // check if SBML model and geometry image are both valid
  // offer user to load a valid one if not
  bool isValidModel();
  bool isValidModelAndGeometryImage();

  void importGeometryImage(const QImage &image);
  void openSBMLFile(const QString &filename);

  void tabMain_currentChanged(int index);
  TabGeometry *tabGeometry;
  TabSpecies *tabSpecies;
  TabReactions *tabReactions;
  TabFunctions *tabFunctions;
  TabParameters *tabParameters;
  TabSimulate *tabSimulate;

  void validateSBMLDoc(const QString &filename = {});
  // if SBML model and geometry are both valid, enable all tabs
  void enableTabs();

  // File menu actions
  void action_New_triggered();
  void action_Open_SBML_file_triggered();
  void menuOpen_example_SBML_file_triggered(const QAction *action);
  void action_Save_SBML_file_triggered();
  void actionExport_Dune_ini_file_triggered();

  // Import menu actions
  void actionGeometry_from_model_triggered();
  void actionGeometry_from_image_triggered();
  void menuExample_geometry_image_triggered(const QAction *action);

  // Tools menu actions
  void actionSet_model_units_triggered();
  void actionSet_image_size_triggered();
  void actionSet_spatial_coordinates_triggered();

  // Advanced menu actions
  void actionSimulation_options_triggered();

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;
};

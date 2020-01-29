#include <fmt/core.h>

#include <QApplication>
#include <QtGui>
#include <locale>

#include "logger.hpp"
#include "mainwindow.hpp"
#include "version.hpp"

int main(int argc, char *argv[]) {
  if (argc > 1) {
    if (std::string arg = argv[1]; (arg == "-v") || (arg == "--version")) {
      fmt::print("{}\n", SPATIAL_MODEL_EDITOR_VERSION);
      return 0;
    } else if ((arg == "-h") || (arg == "--help")) {
      fmt::print("Spatial Model Editor {}\n", SPATIAL_MODEL_EDITOR_VERSION);
      fmt::print("https://www.github.com/lkeegan/spatial-model-editor\n");
      return 0;
    }
  }

  spdlog::set_pattern("%^[%L%$][%14s:%4#] %! :: %v%$");
  // set to lowest level here to show everything
  // then disable lower levels at compile time
  spdlog::set_level(spdlog::level::trace);

  QApplication a(argc, argv);

  // Qt sets the locale according to the system one
  // This can cause problems with symengine
  // e.g. when a double is written as "3,142" vs "3.142"
  // it looks for "." to identify a double, but then uses strtod
  // to convert it, so "3.142" gets interpreted as "3"
  // Here we reset to the default "C" locale to avoid these issues
  std::locale::global(std::locale::classic());

  MainWindow w;
  w.show();

  return a.exec();
}

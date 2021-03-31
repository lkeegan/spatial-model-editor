#include <QDir>
#include <QImage>
#include <QRgb>
#include <list>
#include <set>
#include <vector>

#include "catch_wrapper.hpp"
#include "tiff.hpp"
#include "utils.hpp"

using namespace sme;

SCENARIO("TiffReader", "[core/common/tiff][core/common][core][tiff]") {
  GIVEN("1 16bit grayscale tiff") {
    QFile::remove("tmp.tif");
    QFile::copy(":/test/16bit_gray.tif", "tmp.tif");
    utils::TiffReader tiffReader(
        QDir::current().filePath("tmp.tif").toStdString());
    REQUIRE(tiffReader.size() == 1);
    REQUIRE(tiffReader.getErrorMessage().isEmpty());
    auto img = tiffReader.getImage();
    REQUIRE(img.size() == QSize(100, 100));
    REQUIRE(img.pixel(0, 0) == 0xff000000);
  }
  GIVEN("3x 8bit grayscale tiff") {
    QFile::remove("tmp2.tif");
    QFile::copy(":/test/8bit_gray_3x.tif", "tmp2.tif");
    utils::TiffReader tiffReader(
        QDir::current().filePath("tmp2.tif").toStdString());
    REQUIRE(tiffReader.size() == 3);
    REQUIRE(tiffReader.getErrorMessage().isEmpty());
    auto img = tiffReader.getImage(0);
    REQUIRE(img.size() == QSize(5, 5));
    REQUIRE(img.pixel(0, 0) == 0xff000000);
  }
}

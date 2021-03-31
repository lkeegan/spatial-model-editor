#include <QDir>
#include <QImage>
#include <QRgb>
#include <list>
#include <set>
#include <vector>

#include "catch_wrapper.hpp"
#include "svg.hpp"
#include "utils.hpp"

using namespace sme;

SCENARIO("SvgReader", "[core/common/svg][core/common][core][svg]") {
GIVEN("nonexistent file"){
    utils::SvgReader svgReader("idontexist.blah");
    REQUIRE(svgReader.valid() == false);
}
GIVEN("invalid svg file"){
    QFile::remove("tmp.tif");
    QFile::copy(":/test/16bit_gray.tif", "tmp.tif");
    utils::SvgReader svgReader("tmp.tif");
    REQUIRE(svgReader.valid() == false);
}
  GIVEN("valid svg file") {
    QFile::remove("tmp.svg");
    QFile::copy(":/test/geometry/icon.svg", "tmp.svg");
    utils::SvgReader svgReader(
        QDir::current().filePath("tmp.svg").toStdString());
    REQUIRE(svgReader.valid() == true);
    QSize s0{320, 320};
    auto img = svgReader.getImage(s0);
    REQUIRE(img.size() == s0);
    //REQUIRE(img.pixel(0, 0) == 0xff000000);
  }
}

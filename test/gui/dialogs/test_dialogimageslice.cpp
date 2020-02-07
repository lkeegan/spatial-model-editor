#include <QFile>

#include "catch_wrapper.hpp"
#include "dialogimageslice.hpp"
#include "qt_test_utils.hpp"

SCENARIO("DialogImageSlice", "[gui][dialogs][imageslice]") {
  GIVEN("5 100x50 images") {
    QVector<QImage> imgs(5,
                         QImage(100, 50, QImage::Format_ARGB32_Premultiplied));
    QRgb col1 = 0xffa34f6c;
    QRgb col2 = 0xff123456;
    for (auto& img : imgs) {
      img.fill(col1);
    }
    imgs[3].fill(col2);
    QVector<double> time{0, 1, 2, 3, 4};
    DialogImageSlice dia(imgs, time);
    ModalWidgetTimer mwt;
    WHEN("user does nothing") {
      QImage slice = dia.getSlicedImage();
      // default: y vs t
      REQUIRE(slice.width() == imgs.size());
      REQUIRE(slice.height() == imgs[0].height());
      REQUIRE(slice.pixel(1, 35) == col1);
      REQUIRE(slice.pixel(2, 49) == col1);
      REQUIRE(slice.pixel(3, 12) == col2);
      REQUIRE(slice.pixel(4, 0) == col1);
    }
    WHEN("user sets image vertical axis to x") {
      mwt.addUserAction({"Up"});
      mwt.start();
      dia.exec();
      QImage slice = dia.getSlicedImage();
      // x vs t
      REQUIRE(slice.width() == imgs.size());
      REQUIRE(slice.height() == imgs[0].width());
      REQUIRE(slice.pixel(1, 75) == col1);
      REQUIRE(slice.pixel(2, 28) == col1);
      REQUIRE(slice.pixel(3, 99) == col2);
      REQUIRE(slice.pixel(4, 0) == col1);
    }
  }
}

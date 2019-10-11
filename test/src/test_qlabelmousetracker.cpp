#include <QtTest>

#include "catch.hpp"
#include "logger.hpp"
#include "qlabelmousetracker.hpp"
#include "qt_test_utils.hpp"

SCENARIO("qlabelmousetracker", "[qlabelmousetracker][gui]") {
  GIVEN("single pixel, single colour image") {
    QLabelMouseTracker mouseTracker;
    QImage img(1, 1, QImage::Format_RGB32);
    QRgb col = QColor(12, 243, 154).rgba();
    img.setPixel(0, 0, col);
    mouseTracker.setImage(img);
    mouseTracker.show();
    mouseTracker.resize(100, 100);
    mouseTracker.setFixedSize(100, 100);
    QTest::qWait(mouseDelay);

    REQUIRE(mouseTracker.getImage().size() == img.size());
    REQUIRE(mouseTracker.getImage().pixel(0, 0) == img.pixel(0, 0));
    REQUIRE(mouseTracker.getColour() == col);

    std::vector<QRgb> clicks;
    QObject::connect(&mouseTracker, &QLabelMouseTracker::mouseClicked,
                     [&clicks](QRgb c) { clicks.push_back(c); });
    REQUIRE(clicks.size() == 0);
    WHEN("mouse clicked on image") {
      // move mouse over image
      QTest::mouseMove(mouseTracker.windowHandle(), QPoint(2, 3), mouseDelay);
      // click on image
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(2, 3), mouseDelay);
      THEN("register click, get colour of pixel") {
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col);
        REQUIRE(mouseTracker.getColour() == col);
      }
    }
    WHEN("mouse clicked twice on image") {
      // click on image
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(66, 81), mouseDelay);
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(55, 11), mouseDelay);
      THEN("register clicks, get colour of last pixel clicked") {
        REQUIRE(clicks.size() == 2);
        REQUIRE(clicks.back() == col);
        REQUIRE(mouseTracker.getColour() == col);
      }
    }
  }
#ifndef Q_OS_WIN
  // CI on windows has issues resizing widgets correctly,
  // so skip these tests on windows
  GIVEN("3x3 pixel, 4 colour image") {
    // pixel colours:
    // 2 1 1
    // 3 4 1
    // 3 1 1
    QLabelMouseTracker mouseTracker;
    QImage img(3, 3, QImage::Format_RGB32);
    QRgb col1 = QColor(12, 243, 154).rgba();
    QRgb col2 = QColor(34, 92, 14).rgba();
    QRgb col3 = QColor(88, 43, 91).rgba();
    QRgb col4 = QColor(108, 13, 55).rgba();
    img.fill(col1);
    img.setPixel(0, 0, col2);
    img.setPixel(0, 1, col3);
    img.setPixel(0, 2, col3);
    img.setPixel(1, 1, col4);
    mouseTracker.setImage(img);
    mouseTracker.show();
    mouseTracker.resize(100, 100);
    QTest::qWait(mouseDelay);

    CAPTURE(col1);
    CAPTURE(col2);
    CAPTURE(col3);
    CAPTURE(col4);
    REQUIRE(mouseTracker.getImage().size() == img.size());
    REQUIRE(mouseTracker.getImage().pixel(0, 0) == img.pixel(0, 0));
    REQUIRE(mouseTracker.getImage().pixel(2, 2) == img.pixel(2, 2));
    REQUIRE(mouseTracker.getColour() == col2);
    int imgDisplaySize = std::min(mouseTracker.width(), mouseTracker.height());
    CAPTURE(imgDisplaySize);

    std::vector<QRgb> clicks;
    QObject::connect(&mouseTracker, &QLabelMouseTracker::mouseClicked,
                     [&clicks](QRgb c) { clicks.push_back(c); });
    REQUIRE(clicks.size() == 0);
    WHEN("mouse click (0,1)") {
      QTest::mouseMove(mouseTracker.windowHandle(), QPoint(99, 99), mouseDelay);
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(0, 1), mouseDelay);
      THEN("pix (0,0) <-> col2") {
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col2);
        REQUIRE(mouseTracker.getColour() == col2);
      }
    }
    WHEN("mouse click (3,7)") {
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(3, 7), mouseDelay);
      THEN("pix (0,0) <-> col2") {
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col2);
        REQUIRE(mouseTracker.getColour() == col2);
      }
    }
    WHEN("mouse click (5,44)") {
      THEN("pix (0,1) <-> col3") {
        QTest::mouseClick(&mouseTracker, Qt::LeftButton,
                          Qt::KeyboardModifiers(),
                          QPoint(static_cast<int>(0.05 * imgDisplaySize),
                                 static_cast<int>(0.44 * imgDisplaySize)),
                          mouseDelay);
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col3);
        REQUIRE(mouseTracker.getColour() == col3);
      }
    }
    WHEN("mouse click (5,84)") {
      THEN("pix (0,2) <-> col3") {
        QTest::mouseClick(&mouseTracker, Qt::LeftButton,
                          Qt::KeyboardModifiers(),
                          QPoint(static_cast<int>(0.05 * imgDisplaySize),
                                 static_cast<int>(0.84 * imgDisplaySize)),
                          mouseDelay);
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col3);
        REQUIRE(mouseTracker.getColour() == col3);
      }
    }
    WHEN("mouse clicks (99,99), (5,84), (50,50)") {
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(99, 99), mouseDelay);
      QTest::mouseMove(mouseTracker.windowHandle(), QPoint(79, 19), mouseDelay);
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(5, 84), mouseDelay);
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(50, 50), mouseDelay);
      THEN("pix (2,2),(0,2),(1,1) <-> col1,col3,col4") {
        REQUIRE(clicks.size() == 3);
        REQUIRE(clicks[0] == col1);
        REQUIRE(clicks[1] == col3);
        REQUIRE(clicks[2] == col4);
        REQUIRE(mouseTracker.getColour() == col4);
      }
    }
    WHEN("resized to 600x600, mouseclick (300,300)") {
      mouseTracker.resize(600, 600);
      QTest::qWait(mouseDelay);
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(300, 300), mouseDelay);
      THEN("pix (1,1) <-> col4") {
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col4);
        REQUIRE(mouseTracker.getColour() == col4);
      }
    }
    WHEN("resized to 6x6, mouseclick (1,3)") {
      mouseTracker.resize(6, 6);
      QTest::qWait(mouseDelay);
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(1, 3), mouseDelay);
      THEN("pix (0,1) <-> col3") {
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col3);
        REQUIRE(mouseTracker.getColour() == col3);
      }
    }
    WHEN("resized to 900x3, mouseclick (2,2)") {
      mouseTracker.resize(900, 3);
      QTest::qWait(mouseDelay);
      QTest::mouseClick(&mouseTracker, Qt::LeftButton, Qt::KeyboardModifiers(),
                        QPoint(2, 2), mouseDelay);
      THEN("pix (2,0) <-> col1") {
        REQUIRE(clicks.size() == 1);
        REQUIRE(clicks.back() == col1);
        REQUIRE(mouseTracker.getColour() == col1);
      }
    }
  }
#endif
}

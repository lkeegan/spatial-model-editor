#include "svg.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <QImage>
#include <QPainter>
#include <QPainterPath>

// includes required by nanosvg
#include <math.h>
#include <stdio.h>
#include <string.h>

// defines required before including nanosvg
#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION // Expands implementation
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvg.h>
#include <nanosvgrast.h>

namespace sme::utils {

SvgReader::SvgReader(const std::string &filename) {
  nsvgImage.reset(nsvgParseFromFile(filename.c_str(), "px", 96));
}

SvgReader::~SvgReader() = default;

bool SvgReader::valid() const {
  return nsvgImage != nullptr && nsvgImage->shapes != nullptr;
}

QImage SvgReader::getImage(const QSize &size) const {
  QImage img(size, QImage::Format_ARGB32_Premultiplied);
  img.fill(qRgba(0, 0, 0, 0));
  if (valid()) {
    auto *nsvgRasterizer{nsvgCreateRasterizer()};
    std::vector<unsigned char> buf(
        static_cast<std::size_t>(size.width() * size.height() * 4), 0);
    nsvgRasterize(nsvgRasterizer, nsvgImage.get(), 0.0, 0.0, 1.0,
                  buf.data(), size.width(), size.height(), size.width() * 4);
    for (int y = 0; y < size.height(); ++y) {
      for (int x = 0; x < size.width(); ++x) {
        int i = 4 * (x + y * size.width());
        img.setPixel(x, y, qRgba(buf[i], buf[i+1], buf[i+2], buf[i+3]));
        SPDLOG_INFO("{:x}", img.pixel(x, y));
      }
    }
    nsvgDeleteRasterizer(nsvgRasterizer);
  }
  img.save("QQQ.png");
  return img;
}

const QString &SvgReader::getErrorMessage() const { return errorMessage; }

} // namespace sme::utils

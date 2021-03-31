// Code to rasterize SVG images

#pragma once

#include <QImage>
#include <QString>
#include <QSvgRenderer>
#include <string>

namespace sme::utils {

class SvgReader {
private:
  std::string errorMessage;

public:
  explicit SvgReader(const std::string &filename);
  QImage getImage(const QSize &size) const;
  const QString &getErrorMessage() const;
};

} // namespace sme::utils

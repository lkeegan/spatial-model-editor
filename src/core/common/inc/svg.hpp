// Code to rasterize SVG images using Nano SVG

#pragma once

#include <QImage>
#include <QString>
#include <string>
#include <memory>

struct NSVGimage;

namespace sme::utils {

class SvgReader {
private:
  std::unique_ptr<NSVGimage> nsvgImage;
  QString errorMessage;

public:
  explicit SvgReader(const std::string &filename);
  ~SvgReader();
  bool valid() const;
  QImage getImage(const QSize &size) const;
  const QString &getErrorMessage() const;
};

} // namespace sme::utils

#include "svg.hpp"
#include "logger.hpp"
#include "utils.hpp"

namespace sme::utils {

SvgReader::SvgReader(const std::string &filename){}

QImage SvgReader::getImage(const QSize& size) const {
  return {};
}

const QString &SvgReader::getErrorMessage() const { return errorMessage; }

} // namespace utils

} // namespace sme

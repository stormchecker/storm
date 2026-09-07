#include "LocatedInformation.h"

namespace storm {
namespace prism {
LocatedInformation::LocatedInformation(std::string const& filename, uint_fast64_t lineNumber) : filename(filename), lineNumber(lineNumber) {
    // Intentionally left empty.
}

std::string const& LocatedInformation::getFilename() const {
    return this->filename;
}

void LocatedInformation::setFilename(std::string const& filename) {
    this->filename = filename;
}

uint_fast64_t LocatedInformation::getLineNumber() const {
    return this->lineNumber;
}

void LocatedInformation::setLineNumber(uint_fast64_t lineNumber) {
    this->lineNumber = lineNumber;
}
}  // namespace prism
}  // namespace storm

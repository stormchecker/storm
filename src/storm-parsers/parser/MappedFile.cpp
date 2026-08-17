#include "storm-parsers/parser/MappedFile.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <boost/integer/integer_mask.hpp>
#include <cstring>
#include <fstream>

#include "storm/exceptions/FileIoException.h"
#include "storm/io/file.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

MappedFile::MappedFile(const char* filename) {
    STORM_LOG_THROW(storm::io::fileExistsAndIsReadable(filename), storm::exceptions::FileIoException,
                    "Error while reading " << filename << ": The file does not exist or is not readable.");

    // Do file mapping for reasonable systems.
    // stat64(), open(), mmap()

#if defined MACOS || !defined __GLIBC__
    if (stat(filename, &(this->st)) != 0) {
#else
    if (stat64(filename, &(this->st)) != 0) {
#endif
        STORM_LOG_THROW(false, storm::exceptions::FileIoException, "Error in stat(" << filename << "): Probably, this file does not exist.");
    }
    this->file = open(filename, O_RDONLY);

    STORM_LOG_THROW(this->file >= 0, storm::exceptions::FileIoException, "Error in open(" << filename << "): Probably, we may not read this file.");

    this->data = static_cast<char*>(mmap(nullptr, this->st.st_size, PROT_READ, MAP_PRIVATE, this->file, 0));
    if (this->data == MAP_FAILED) {
        close(this->file);
        STORM_LOG_ERROR("Error in mmap(" << filename << "): " << std::strerror(errno));
        STORM_LOG_THROW(false, storm::exceptions::FileIoException, "MappedFile Error in mmap(): " << std::strerror(errno) << ".");
    }
    this->dataEnd = this->data + this->st.st_size;
}

MappedFile::~MappedFile() {
    munmap(this->data, this->st.st_size);
    close(this->file);
}

char const* MappedFile::getData() const {
    return data;
}

char const* MappedFile::getDataEnd() const {
    return dataEnd;
}

std::size_t MappedFile::getDataSize() const {
    return this->getDataEnd() - this->getData();
}

}  // namespace parser
}  // namespace storm

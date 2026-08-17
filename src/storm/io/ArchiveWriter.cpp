#include "storm/io/ArchiveWriter.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/macros.h"

namespace storm::io {

#ifdef STORM_HAVE_LIBARCHIVE
ArchiveWriter::ArchiveWriter(std::filesystem::path const& filename, CompressionMode const compression) : _archive(archive_write_new(), ArchiveDeleter{}) {
    STORM_LOG_THROW(_archive, storm::exceptions::FileIoException, "Failed to create archive reader.");

    // Set format to gzipped TAR with restricted pax extensions
    switch (compression) {
        case CompressionMode::None:
            break;
        case CompressionMode::Gzip:
            archive_write_add_filter_gzip(_archive.get());
            break;
        case CompressionMode::Xz:
        case CompressionMode::Default:  // Following suggestions from UMB, we use xz as default compression
            archive_write_add_filter_xz(_archive.get());
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unsupported compression mode.");
    }
    checkResult(archive_write_set_format_pax_restricted(_archive.get()));

    // Open file for writing
    checkResult(archive_write_open_filename(_archive.get(), filename.c_str()));
}
#else
ArchiveWriter::ArchiveWriter(std::filesystem::path const&, CompressionMode const) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Writing archives is not supported. Storm is compiled without LibArchive.");
}
#endif

void ArchiveWriter::addDirectory(std::filesystem::path const& archivePath) {
#ifdef STORM_HAVE_LIBARCHIVE

    archive_entry* entry = archive_entry_new();
    STORM_LOG_THROW(entry, storm::exceptions::FileIoException, "Failed to create archive entry.");

    // Fill in metadata: path, file type, permissions, etc.
    archive_entry_set_pathname(entry, archivePath.c_str());
    archive_entry_set_filetype(entry, AE_IFDIR);
    archive_entry_set_perm(entry, 0777);

    // Write the header (metadata) to the archive
    checkResult(archive_write_header(_archive.get(), entry), entry);

    // Free the entry metadata after we finish writing
    archive_entry_free(entry);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Writing archives is not supported. Storm is compiled without LibArchive.");
#endif
}

void ArchiveWriter::addFile(std::filesystem::path const& archivePath, char const* data, std::size_t const size) {
    std::size_t startOfChunk = 0;
    auto getNextChunk = [&]() {
        auto const chunkSize = std::min<std::size_t>(size - startOfChunk, BufferSize);
        auto const chunk = std::span<char const>(data + startOfChunk, chunkSize);
        startOfChunk += chunkSize;
        return chunk;
    };
    addFileFromChunks(archivePath, getNextChunk, size);
}

void ArchiveWriter::addBinaryFile(std::filesystem::path const& archivePath, storm::storage::BitVector const& data) {
    using BucketType = decltype(std::declval<storm::storage::BitVector&>().getBucket({}));
    static_assert(BufferSize % sizeof(BucketType) == 0, "Buffer size must be a multiple of sizeof(BucketType).");
    std::array<BucketType, BufferSize / sizeof(BucketType)> buffer;

    // need to reverse bits and potentially swap bytes
    uint64_t startOfChunk = 0;
    auto getNextChunk = [&data, &buffer, &startOfChunk]() -> std::span<const char> {
        auto const endOfChunk = std::min<uint64_t>(startOfChunk + buffer.size(), data.bucketCount());
        auto bufferIt = buffer.begin();
        for (uint64_t i = startOfChunk; i < endOfChunk; ++i) {
            // reverse bits so that the first bit of the first byte is data.get(0).
            bool constexpr NativeLittleEndian = std::endian::native == std::endian::little;
            *bufferIt = storm::utility::reverseBits<BucketType, NativeLittleEndian>(data.getBucket(i));
            ++bufferIt;
        }
        std::span<const char> chunk(reinterpret_cast<const char*>(buffer.data()), (endOfChunk - startOfChunk) * sizeof(BucketType));
        startOfChunk = endOfChunk;
        return chunk;
    };
    uint64_t const numBytes = data.bucketCount() * sizeof(BucketType);
    addFileFromChunks(archivePath, getNextChunk, numBytes);
}

void ArchiveWriter::addTextFile(std::filesystem::path const& archivePath, std::string const& data) {
    addFile(archivePath, data.c_str(), data.size());
}

#ifdef STORM_HAVE_LIBARCHIVE
void ArchiveWriter::checkResult(auto resultCode, archive_entry* entry) const {
    STORM_LOG_THROW(_archive, storm::exceptions::FileIoException, "Unexpected result: Archive not loaded.");

    STORM_LOG_WARN_COND(std::cmp_greater_equal(resultCode, ARCHIVE_OK), "Unexpected result from archive: " << archive_error_string(_archive.get()) << ".");
    if (std::cmp_less(resultCode, ARCHIVE_WARN)) {
        if (entry) {
            archive_entry_free(entry);
        }
        STORM_LOG_THROW(false, storm::exceptions::FileIoException, "Unexpected result from archive: " << archive_error_string(_archive.get()) << ".");
    }
}

void ArchiveWriter::ArchiveDeleter::operator()(archive* arch) const noexcept {
    if (arch) {
        archive_free(arch);
    }
}
#endif

}  // namespace storm::io
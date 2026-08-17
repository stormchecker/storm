#pragma once

#include <bit>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "storm-config.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/io/CompressionMode.h"
#include "storm/utility/bitoperations.h"
#include "storm/utility/macros.h"

#ifdef STORM_HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace storm {

namespace storage {
class BitVector;
}

namespace io {

namespace detail {
template<typename R>
concept ArchiveWritableRange = std::ranges::input_range<R> && std::is_arithmetic_v<std::ranges::range_value_t<R>>;

template<typename R, std::endian Endianness>
concept ArchiveWritableWithoutBuffer =
    ArchiveWritableRange<R> && std::same_as<std::remove_cvref_t<R>, std::vector<std::ranges::range_value_t<R>>> &&
    !std::is_same_v<std::ranges::range_value_t<R>, bool> && (Endianness == std::endian::native || sizeof(std::ranges::range_value_t<R>) == 1);

}  // namespace detail

static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big, "This code is not supported for mixed endian systems.");

class ArchiveWriter {
   public:
    /*!
    Create a new archive and open it as a file on disk.
    */
    ArchiveWriter(std::filesystem::path const& filename, CompressionMode const compression);

    /*!
      Adds a (sub-) directory to the archive
     */
    void addDirectory(std::filesystem::path const& archivePath);

    /*!
     * Add a file to the archive.
     * @param archivePath The file’s path inside the archive
     * @param data The file contents
     */
    void addFile(std::filesystem::path const& archivePath, char const* data, std::size_t const size);

    /*!
     * Add a file to the archive using a binary encoding of the provided data.
     * @param archivePath The file’s path inside the archive
     * @param data The file contents
     */
    template<detail::ArchiveWritableRange Range, std::endian Endianness = std::endian::little>
        requires(detail::ArchiveWritableWithoutBuffer<Range, Endianness>)
    void addBinaryFile(std::filesystem::path const& archivePath, Range&& data) {
        auto const numBytes = data.size() * sizeof(std::ranges::range_value_t<Range>);
        addFile(archivePath, reinterpret_cast<char const*>(data.data()), numBytes);
    }

    /*!
     * Add a file to the archive using a binary encoding of the provided data.
     * @param archivePath The file’s path inside the archive
     * @param data The file contents
     */
    template<detail::ArchiveWritableRange Range, std::endian Endianness = std::endian::little>
        requires(!detail::ArchiveWritableWithoutBuffer<Range, Endianness> && !std::is_same_v<std::ranges::range_value_t<Range>, bool>)
    void addBinaryFile(std::filesystem::path const& archivePath, Range const& data) {
        using T = std::ranges::range_value_t<Range>;
        auto const numBytes = data.size() * sizeof(T);
        static_assert(BufferSize % sizeof(T) == 0, "Buffer size must be a multiple of sizeof(T).");
        std::vector<T> buffer(BufferSize / sizeof(T));  // todo check array

        uint64_t startOfChunk = 0;
        auto getNextChunk = [&data, &buffer, &startOfChunk]() {
            auto const endOfChunk = std::min<uint64_t>(startOfChunk + buffer.size(), data.size());
            auto bufferIt = buffer.begin();
            for (uint64_t i = startOfChunk; i < endOfChunk; ++i) {
                if constexpr (std::endian::native != Endianness && sizeof(T) > 1) {
                    // copy into buffer with byteswap
                    *bufferIt = storm::utility::byteSwap(data[i]);
                } else {
                    // copy into buffer
                    *bufferIt = data[i];
                }
                ++bufferIt;
            }
            std::span<const char> chunk(reinterpret_cast<const char*>(buffer.data()), (endOfChunk - startOfChunk) * sizeof(T));
            startOfChunk = endOfChunk;
            return chunk;
        };
        addFileFromChunks(archivePath, getNextChunk, numBytes);
    }

    /*!
     * Add a file to the archive using a binary encoding of the provided data.
     * @param archivePath The file’s path inside the archive
     * @param data The file contents
     */
    void addBinaryFile(std::filesystem::path const& archivePath, storm::storage::BitVector const& data);

    /*!
     * Add a text file to the archive
     * @param archivePath The file’s path inside the archive
     * @param data The file contents
     */
    void addTextFile(std::filesystem::path const& archivePath, std::string const& data);

   private:
    /*!
     * Add a file to the archive.
     * @param archivePath the path inside the archive
     * @param getNextChunk returns the file contents in bytes, divided into multiple chunks
     * @param size the size of the file in bytes
     */
    template<typename F>
    void addFileFromChunks(std::string const& archivePath, F getNextChunk, size_t const size) {
#ifdef STORM_HAVE_LIBARCHIVE
        archive_entry* entry = archive_entry_new();
        STORM_LOG_THROW(entry, storm::exceptions::FileIoException, "Failed to create archive entry.");

        // Fill in metadata: path, file size, file type, permissions, etc.
        archive_entry_set_pathname(entry, archivePath.c_str());
        archive_entry_set_size(entry, size);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0777);

        // Write the header (metadata) to the archive
        checkResult(archive_write_header(_archive.get(), entry), entry);

        // Write the file contents
        uint64_t bytesWritten = 0;
        for (auto chunk = getNextChunk(); chunk.size() > 0 && bytesWritten < size; chunk = getNextChunk()) {
            auto res = archive_write_data(_archive.get(), chunk.data(), chunk.size());
            checkResult(res, entry);
            bytesWritten += res;
        }
        STORM_LOG_WARN_COND(bytesWritten == size, "When writing file '" << archivePath << "' to archive, " << bytesWritten << " bytes were written but " << size
                                                                        << " bytes were expected.");

        // Free the entry metadata after we finish writing
        archive_entry_free(entry);
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Writing archives is not supported. Storm is compiled without LibArchive.");
#endif
    }

#ifdef STORM_HAVE_LIBARCHIVE
    /*!
     * Checks the result of an archive operation and throws an exception if the result is not ok.
     */
    void checkResult(auto resultCode, archive_entry* entry = nullptr) const;

    struct ArchiveDeleter {
        void operator()(archive* arch) const noexcept;
    };

    std::unique_ptr<archive, ArchiveDeleter> _archive;
#endif

    static constexpr size_t BufferSize = 8192;
};
}  // namespace io
}  // namespace storm
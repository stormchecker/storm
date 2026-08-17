#pragma once

#include <bit>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "storm-config.h"

#ifdef STORM_HAVE_LIBARCHIVE
#include <archive.h>
#include <archive_entry.h>
#endif

namespace storm {

namespace storage {
class BitVector;
}

namespace io {
static_assert(std::endian::native == std::endian::little || std::endian::native == std::endian::big, "This code is not supported for mixed endian systems.");

class ArchiveReader {
   private:
#ifdef STORM_HAVE_LIBARCHIVE
    /*!
     * Auxiliary struct to delete archive objects (not the archive from disk!)
     */
    struct ArchiveDeleter {
        void operator()(archive* arch) const noexcept;
    };
#endif
   public:
    /*!
     * Object that reads the archive entry.
     */
    class ArchiveReadEntry {
       public:
#ifdef STORM_HAVE_LIBARCHIVE
        ArchiveReadEntry(archive_entry* currentEntry, archive* archive);
#endif

        /*!
         * Get the current entry’s path (filename) inside the archive.
         * Returns an empty path if the entry does not exist.
         */
        std::filesystem::path name() const;

        /*!
         * @return Whether the current entry is a directory.
         */
        bool isDir() const;

        template<typename T>
        using VectorType = std::conditional_t<std::is_same_v<T, bool>, storm::storage::BitVector, std::vector<T>>;

        /*!
         * extracts the current entry’s data as a vector of the given type.
         * @note this consumes the archive entry contents, i.e., this can only be called once
         */
        template<typename T, std::endian Endianness = std::endian::little>
            requires(std::is_arithmetic_v<T>)
        VectorType<T> toVector();

        /*!
         * extracts the current entry’s data as a string
         * @note this consumes the archive entry contents, i.e., this can only be called once
         */
        std::string toString();

       private:
#ifdef STORM_HAVE_LIBARCHIVE
        static constexpr size_t BufferSize = 8192;
        archive_entry* const _currentEntry;
        archive* _archive;
#endif
    };
    class Iterator {
       public:
        Iterator() = default;
#ifdef STORM_HAVE_LIBARCHIVE
        Iterator(std::filesystem::path const& filename);
#endif

        bool operator==(Iterator const& other) const;
        bool operator!=(Iterator const& other) const;

        /*!
         * Move to the next entry in the archive.
         */
        Iterator& operator++();

        ArchiveReadEntry operator*() const;

       private:
#ifdef STORM_HAVE_LIBARCHIVE
        std::unique_ptr<archive, ArchiveDeleter> _archive;
        archive_entry* _currentEntry{nullptr};
#endif
    };

    ArchiveReader(std::filesystem::path const& file);

    /*!
     * @return true if the provided file is a readable archive
     * @note if libarchive is not installed, this check looks at the magic bytes and might be inaccurate
     */
    bool isReadableArchive() const;

    /*!
     * @return an iterator to the beginning of the archive entries
     * @throws MissingLibraryException if libarchive is not installed.
     */
    Iterator begin() const;

    /*!
     * @return an iterator to the end of the archive entries
     * @throws MissingLibraryException if libarchive is not installed
     */
    Iterator end() const;

   private:
    std::filesystem::path const file;
};

/*!
 * Reads an archive file
 * @param file path to an archive file
 * @return A range-like object to iterate over the entries in the archive.
 */
ArchiveReader openArchive(std::filesystem::path const& file);
}  // namespace io
}  // namespace storm
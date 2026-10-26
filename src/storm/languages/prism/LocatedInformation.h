#pragma once

#include <cstdint>
#include <string>

namespace storm {
namespace prism {
class LocatedInformation {
   public:
    /*!
     * Constructs a located information with the given filename and line number.
     *
     * @param filename The file in which the information was found.
     * @param lineNumber The line number in which the information was found.
     */
    LocatedInformation(std::string const& filename, uint_fast64_t lineNumber);

    // Create default implementations of constructors/assignment.
    LocatedInformation() = default;
    LocatedInformation(LocatedInformation const& other) = default;
    LocatedInformation& operator=(LocatedInformation const& other) = default;
    LocatedInformation(LocatedInformation&& other) = default;
    LocatedInformation& operator=(LocatedInformation&& other) = default;

    /*!
     * Retrieves the name of the file in which the information was found.
     *
     * @return The name of the file in which the information was found.
     */
    std::string const& getFilename() const;

    /*!
     * Sets the filename of this information.
     *
     * @param filename The new filename of this information.
     */
    void setFilename(std::string const& filename);

    /*!
     * Retrieves the line number in which the information was found.
     *
     * @return The line number in which the information was found.
     */
    uint_fast64_t getLineNumber() const;

    /*!
     * Sets the line number of this information.
     *
     * @param lineNumber The new line number for this information.
     */
    void setLineNumber(uint_fast64_t lineNumber);

   private:
    // The file in which the piece of information was found.
    std::string filename;

    // The line in the file in which the piece of information was found.
    uint_fast64_t lineNumber;
};
}  // namespace prism
}  // namespace storm

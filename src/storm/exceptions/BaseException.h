#pragma once

#include <exception>
#include <sstream>

namespace storm {
namespace exceptions {

/*!
 * This class represents the base class of all exception classes.
 */
class BaseException : public std::exception {
   public:
    /*!
     * Creates a base exception without a message.
     */
    BaseException();

    /*!
     * Creates a base expression from the given exception.
     *
     * @param other The expression from which to copy-construct.
     */
    BaseException(BaseException const& other);

    /*!
     * Adds the given string to the message of this exception.
     */
    BaseException(char const* cstr);

    /*!
     * Declare a destructor to counter the "looser throw specificator" error
     */
    virtual ~BaseException();

    /*!
     * Retrieves the message associated with this exception.
     *
     * @return The message associated with this exception.
     */
    virtual const char* what() const noexcept override;

    /*!
     * Returns the type of the exception.
     */
    virtual std::string type() const;

    /*!
     * Returns additional information about the exception.
     */
    virtual std::string additionalInfo() const;

   protected:
    // This stream stores the message of this exception.
    std::stringstream stream;

   private:
    // storage for the string backing the C string returned by what()
    mutable std::string errorString;
};

}  // namespace exceptions
}  // namespace storm

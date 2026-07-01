#pragma once

#include <boost/optional.hpp>
#include <chrono>
#include <cstdint>
#include <ostream>

namespace storm {
namespace utility {

/*!
 * A class that provides convenience operations to display run times.
 */
class ProgressMeasurement {
   public:
    typedef decltype(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::seconds::zero()).count()) SecondType;
    typedef decltype(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::milliseconds::zero()).count()) MilisecondType;
    typedef decltype(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::nanoseconds::zero()).count()) NanosecondType;

    /*!
     * Initializes progress measurement.
     * @param itemName the name of what we are counting (iterations, states, ...).
     */
    ProgressMeasurement(std::string const& itemName = "items");

    /*!
     * Starts a new measurement, dropping all progress information collected so far.
     * @param startCount the initial count.
     */
    void startNewMeasurement(uint64_t startCount);

    /*!
     * Updates the progress to the current count and prints it if the delay passed.
     * The progress is only updated and printed if the ShowProgress setting is enabled.
     *
     * @param count The currently achieved count.
     * @return True iff the progress was printed (i.e., the delay passed and showProgress setting enabled).
     */
    bool updateProgress(uint64_t count);

    /*!
     * Updates the progress to the current count.
     * The update and printing is done independently of the showProgress setting.
     *
     * @param count The currently achieved count.
     * @param outstream The stream to which the progress is printed (if the delay passed)
     * @return True iff the progress was printed (i.e., the delay passed)
     */
    bool updateProgress(uint64_t count, std::ostream& outstream);

    /*!
     * Returns whether a maximal count (which is required to achieve 100% progress) has been specified
     */
    bool isMaxCountSet() const;

    /*!
     * Returns the maximal possible count (if specified).
     */
    uint64_t getMaxCount() const;

    /*!
     * Sets the maximal possible count.
     */
    void setMaxCount(uint64_t maxCount);

    /*!
     * Erases a previously specified maximal count.
     */
    void unsetMaxCount();

    /*!
     * Returns the currently specified minimal delay (in seconds) between two progress messages.
     */
    uint64_t getShowProgressDelay() const;

    /*!
     * Customizes the minimal delay between two progress messages.
     * @param delay the delay (in seconds).
     */
    void setShowProgressDelay(uint64_t delay);

    /*!
     * Returns the current name of what we are counting (e.g. iterations, states, ...)
     */
    std::string const& getItemName() const;

    /*!
     * Customizes the name of what we are counting (e.g. iterations, states, ...)
     * @param name the name of what we are counting.
     */
    void setItemName(std::string const& name);

   private:
    // Whether progress should be printed to standard output.
    bool showProgress;

    // The delay (in seconds) between progress emission.
    uint64_t delay;
    // A name for what this is measuring (iterations, states, ...)
    std::string itemName;

    // The maximal count that can be achieved. numeric_limits<uint64_t>::max() means unspecified.
    uint64_t maxCount;

    // The last displayed count
    uint64_t lastDisplayedCount;

    std::chrono::high_resolution_clock::time_point timeOfStart;
    std::chrono::high_resolution_clock::time_point timeOfLastMessage;
};

}  // namespace utility
}  // namespace storm

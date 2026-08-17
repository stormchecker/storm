#pragma once

#include <cstdint>

#include "storm-gspn/storage/gspn/Transition.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace gspn {
template<typename RateType>
class TimedTransition : public storm::gspn::Transition {
   public:
    TimedTransition() {
        setSingleServerSemantics();
    }

    /*!
     * Sets the rate of this transition to the given value.
     *
     * @param rate The new rate for this transition.
     */
    void setRate(RateType const& rate) {
        this->rate = rate;
    }

    /*!
     * Sets the semantics of this transition
     */
    void setKServerSemantics(uint64_t k) {
        STORM_LOG_THROW(k > 0, storm::exceptions::InvalidArgumentException, "Invalid Parameter for server semantics: 0.");
        nServers = k;
    }

    void setSingleServerSemantics() {
        nServers = 1;
    }

    void setInfiniteServerSemantics() {
        nServers = 0;
    }

    /*!
     * Retrieves the semantics of this transition
     */
    bool hasKServerSemantics() const {
        return nServers > 0;
    }

    bool hasSingleServerSemantics() const {
        return nServers == 1;
    }

    bool hasInfiniteServerSemantics() const {
        return nServers == 0;
    }

    uint64_t getNumberOfServers() const {
        STORM_LOG_ASSERT(hasKServerSemantics(), "Tried to get the number of servers of a timed transition although it does not have K-Server-Semantics.");
        return nServers;
    }

    /*!
     * Retrieves the rate of this transition.
     *
     * @return The rate of this transition.
     */
    RateType getRate() const {
        return this->rate;
    }

   private:
    // the rate of the transition
    RateType rate;

    // the number of servers of this transition. 0 means infinite server semantics.
    uint64_t nServers;
};
}  // namespace gspn
}  // namespace storm
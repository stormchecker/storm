#pragma once

#include <boost/optional.hpp>
#include <vector>

#include "storm/modelchecker/hints/ModelCheckerHint.h"
#include "storm/storage/Scheduler.h"

namespace storm {
namespace modelchecker {

/*!
 * This class contains information that might accelerate the model checking process.
 * @note The model checker has to make sure whether a given hint is actually applicable and thus a hint might be ignored.
 */
template<typename ValueType>
class ExplicitModelCheckerHint : public ModelCheckerHint {
   public:
    ExplicitModelCheckerHint() = default;
    ExplicitModelCheckerHint(ExplicitModelCheckerHint<ValueType> const& other) = default;
    ExplicitModelCheckerHint(ExplicitModelCheckerHint<ValueType>&& other) = default;

    // Returns true iff this hint does not contain any information
    virtual bool isEmpty() const override;

    // Returns true iff this is an explicit model checker hint
    virtual bool isExplicitModelCheckerHint() const override;

    bool hasResultHint() const;
    std::vector<ValueType> const& getResultHint() const;
    std::vector<ValueType>& getResultHint();
    void setResultHint(boost::optional<std::vector<ValueType>> const& resultHint);
    void setResultHint(boost::optional<std::vector<ValueType>>&& resultHint);

    // Set whether only the maybestates need to be computed, i.e., skips the qualitative check.
    // The result for non-maybe states is taken from the result hint.
    // Hence, this option may only be enabled iff a resultHint and a set of maybestates are given.
    bool getComputeOnlyMaybeStates() const;
    void setComputeOnlyMaybeStates(bool value);
    bool hasMaybeStates() const;
    storm::storage::BitVector const& getMaybeStates() const;
    storm::storage::BitVector& getMaybeStates();
    void setMaybeStates(storm::storage::BitVector const& maybeStates);
    void setMaybeStates(storm::storage::BitVector&& maybeStates);

    bool hasSchedulerHint() const;
    storm::storage::Scheduler<ValueType> const& getSchedulerHint() const;
    storm::storage::Scheduler<ValueType>& getSchedulerHint();
    void setSchedulerHint(boost::optional<storage::Scheduler<ValueType>> const& schedulerHint);
    void setSchedulerHint(boost::optional<storage::Scheduler<ValueType>>&& schedulerHint);

    // If set, it is assumed that there are no end components that consist only of maybestates.
    // May only be enabled iff maybestates are given.
    bool getNoEndComponentsInMaybeStates() const;
    void setNoEndComponentsInMaybeStates(bool value);

   private:
    boost::optional<std::vector<ValueType>> resultHint;
    boost::optional<storm::storage::Scheduler<ValueType>> schedulerHint;

    bool computeOnlyMaybeStates;
    boost::optional<storm::storage::BitVector> maybeStates;
    bool noEndComponentsInMaybeStates;
};

}  // namespace modelchecker
}  // namespace storm

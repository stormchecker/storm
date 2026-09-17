#pragma once

#include <optional>

#include "storm/adapters/JsonForward.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/sparse/StateType.h"
#include "storm/storage/valuations/Valuations.h"

namespace storm {

namespace modelchecker {

/*!
 * A qualitative check result over the states of a sparse model.
 *
 * The result either covers every state of the model or just a subset of them, e.g. after filtering it to the
 * initial states. In the latter case the states in question are recorded in a bit vector and the truth values are
 * stored compressed, i.e. the i-th truth value belongs to the i-th state selected by that bit vector.
 */
template<typename ValueType>
class ExplicitQualitativeCheckResult : public QualitativeCheckResult {
   public:
    typedef storm::storage::BitVector vector_type;

    /*!
     * Creates a result that holds the given truth value for the given state only.
     */
    ExplicitQualitativeCheckResult(storm::storage::sparse::state_type state, bool value);

    /*!
     * Creates a result for all states of a model with as many states as the given bit vector has bits.
     */
    ExplicitQualitativeCheckResult(vector_type const& truthValues, std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});
    ExplicitQualitativeCheckResult(vector_type&& truthValues, std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});

    /*!
     * Creates a result for the given states only.
     * @param states The states the result is for.
     * @param truthValues One truth value per state selected by @p states, in the order of the selected states.
     */
    ExplicitQualitativeCheckResult(storm::storage::BitVector states, vector_type&& truthValues,
                                   std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler = {});

    virtual ~ExplicitQualitativeCheckResult() = default;
    ExplicitQualitativeCheckResult(ExplicitQualitativeCheckResult const& other) = default;
    ExplicitQualitativeCheckResult& operator=(ExplicitQualitativeCheckResult const& other) = default;
    ExplicitQualitativeCheckResult(ExplicitQualitativeCheckResult&& other) = default;
    ExplicitQualitativeCheckResult& operator=(ExplicitQualitativeCheckResult&& other) = default;

    virtual std::unique_ptr<CheckResult> clone() const override;

    /*!
     * Retrieves the truth value of the given state.
     * @pre The result holds a truth value for that state.
     */
    bool operator[](storm::storage::sparse::state_type state) const;

    virtual bool isExplicit() const override;
    virtual bool isResultForAllStates() const override;

    virtual bool isExplicitQualitativeCheckResult() const override;

    virtual QualitativeCheckResult& operator&=(QualitativeCheckResult const& other) override;
    virtual QualitativeCheckResult& operator|=(QualitativeCheckResult const& other) override;
    virtual void complement() override;

    /*!
     * Retrieves whether the result holds a truth value for the given state.
     */
    bool hasValueForState(storm::storage::sparse::state_type state) const;

    /*!
     * Retrieves the states this result holds truth values for.
     * @pre This is not a result for all states.
     */
    storm::storage::BitVector const& getStates() const;

    /*!
     * Retrieves the truth values, one per state this result is for. If this is not a result for all states, the
     * i-th truth value belongs to the i-th state selected by getStates().
     */
    vector_type const& getTruthValuesVector() const;

    virtual bool existsTrue() const override;
    virtual bool forallTrue() const override;
    virtual uint64_t count() const override;

    virtual std::ostream& writeToStream(std::ostream& out) const override;

    virtual void filter(QualitativeCheckResult const& filter) override;

    virtual bool hasScheduler() const override;
    void setScheduler(std::unique_ptr<storm::storage::Scheduler<ValueType>>&& scheduler);
    storm::storage::Scheduler<ValueType> const& getScheduler() const;
    storm::storage::Scheduler<ValueType>& getScheduler();

    template<typename JsonRationalType>
    storm::json<JsonRationalType> toJson(std::optional<storm::storage::sparse::Valuations> const& stateValuations = std::nullopt,
                                         std::optional<storm::models::sparse::StateLabeling> const& stateLabels = std::nullopt) const;

   private:
    bool hasValueType(std::type_info const& t) const override {
        return t == typeid(ValueType);
    }

    static void performLogicalOperation(ExplicitQualitativeCheckResult& first, QualitativeCheckResult const& second, bool logicalAnd);

    /*!
     * Retrieves the index at which the truth value of the given state is stored.
     * @pre The result holds a truth value for that state.
     */
    uint64_t getOffset(storm::storage::sparse::state_type state) const;

    /*!
     * Invokes the given function with the state and the offset of its truth value, for every state this result is
     * for.
     */
    template<typename Function>
    void forEachState(Function const& f) const {
        if (states) {
            uint64_t offset = 0;
            for (auto const& state : *states) {
                f(state, offset);
                ++offset;
            }
        } else {
            for (uint64_t state = 0; state < truthValues.size(); ++state) {
                f(state, state);
            }
        }
    }

    // The states this result holds truth values for, or nothing at all if it is a result for all states.
    std::optional<storm::storage::BitVector> states;

    // The truth values of the qualitative check result, one per state this result is for.
    vector_type truthValues;

    // An optional scheduler that accompanies the values.
    std::optional<std::shared_ptr<storm::storage::Scheduler<ValueType>>> scheduler;
};
}  // namespace modelchecker
}  // namespace storm

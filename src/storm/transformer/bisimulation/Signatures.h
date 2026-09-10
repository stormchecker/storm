#pragma once

#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <utility>
#include <vector>

#include "storm/models/sparse/ModelForward.h"
#include "storm/transformer/bisimulation/Partition.h"
#include "storm/transformer/bisimulation/QuotientData.h"

namespace storm::bisimulation {

enum class SignatureMode { Exact, Approximative };

/*!
 * Computes and caches the state signatures and provides state-based order / condition for splitting in signature refinement.
 * @tparam ValueType the type of the transition values in the model
 * @tparam Mode the mode of the signature computation (exact or approximate)
 */
template<typename ValueType, SignatureMode Mode>
class Signatures {
   public:
    Signatures(storm::models::sparse::Model<ValueType> const& model, std::optional<std::vector<uint64_t>> const& choiceClasses,
               storm::bisimulation::Partition const& partition)
        requires(Mode == SignatureMode::Exact);

    Signatures(storm::models::sparse::Model<ValueType> const& model, std::optional<std::vector<uint64_t>> const& choiceClasses,
               storm::bisimulation::Partition const& partition, ValueType const& tolerance)
        requires(Mode == SignatureMode::Approximative);

    /*!
     * Updates the state signature of the given state index.
     * @param stateIndex
     */
    void updateStateSignature(uint64_t const stateIndex);

   private:
    struct StateSignature;  // Forward-declared; defined privately further down.

   public:
    /*!
     * Represents a strict weak order over state indices based on their signature.
     * @note assumes that only states are compared for which updateStateSignature has been called since the last change of the partition.
     */
    class SplitOrder {
       public:
        explicit SplitOrder(std::vector<StateSignature> const& signatures) : signatures(signatures) {}
        bool operator()(uint64_t const state1, uint64_t const state2) const;

       private:
        std::vector<StateSignature> const& signatures;
    };

    /*!
     * Instantiates a SplitOrder whose ties exactly coincide with true signature equality (operator==)
     */
    SplitOrder getEquivalenceSplitOrder() const
        requires(Mode == SignatureMode::Exact);

    /*!
     * Instantiates a SplitOrder that only compares with respect to structural equality of the state signature (ChoiceSignature::compareStructure)
     */
    SplitOrder getStructuralSplitOrder() const
        requires(Mode == SignatureMode::Approximative);

    /*!
     * Represents a condition for splitting two states based on their signature.
     * @note assumes that only states are compared for which updateStateSignature has been called since the last change of the partition.
     * @note assumes that the two states are equivalent with respect to getStructuralSplitOrder()
     * operator() returns true iff the two states are not approximately equal with respect to the given tolerance.
     */
    struct SplitCondition {
        SplitCondition(std::vector<StateSignature> const& signatures, ValueType const& tolerance) : signatures(signatures), tolerance(tolerance) {}
        bool operator()(uint64_t const state1, uint64_t const state2) const
            requires(Mode == SignatureMode::Approximative);

       private:
        std::vector<StateSignature> const& signatures;
        ValueType const tolerance;
    };

    /*!
     * Instantiates a SplitCondition.
     */
    SplitCondition getApproximateSplitCondition() const
        requires(Mode == SignatureMode::Approximative);

    /*!
     * Fills in the choice mappings of the given (already state-mapped) quotient data: for every quotient state, the choices of its representative state
     * are deduplicated (exactly or up to tolerance, depending on Mode) into the quotient choices.
     * @param createQuotientChoiceMapping if set, every original model choice is additionally mapped to the quotient choice that represents it
     * (QuotientData::SignatureData::toQuotientChoice). If not set, that (comparatively expensive) mapping is left empty.
     */
    void extendQuotientData(QuotientData<ValueType>& quotientData, bool const createQuotientChoiceMapping) const;

   private:
    // For all choices, we store the current distributions over successor blocks in choiceDistributionStorage and use a view into that for ChoiceSignatures.
    using BlockDistributionView = std::span<std::pair<Partition::Block, ValueType>>;

    /*!
     * The signature of a single choice, consisting of its choice class and the distribution over successor blocks (Partition::Block -> ValueType).
     */
    struct ChoiceSignature {
        uint64_t choiceClass;  // The class of this choice according to the provided choice classes.
        // The accumulated transition values for each block, sorted by Partition::BlockCompare. This is a (non-owning) view into the choice's permanent
        // slot of Signatures::choiceDistributionStorage, see there.
        BlockDistributionView distr;

        // Orders by choiceClass and the set of blocks (identity and order) of distr; independent of the actual transition values.
        std::strong_ordering compareStructure(ChoiceSignature const& other) const;
        // strong_ordering in exact mode (equal signatures are substitutable); only weak_ordering in approximative mode, since ties don't imply approxEqual.
        using ComparisonResult = std::conditional_t<Mode == SignatureMode::Exact, std::strong_ordering, std::weak_ordering>;
        // Strict weak order for sorting/lower_bound. In approximative mode, ties don't imply approximate equality - see StateSignature::find.
        ComparisonResult compare(ChoiceSignature const& other) const;
    };

    using ChoiceSignatureIterator = typename std::vector<ChoiceSignature>::const_iterator;

    /*!
     * The (static) signature of a state w.r.t. a partition.
     *  A state signature is seen as an ordered set of deduplicated choice signatures, where the order is defined by ChoiceSignature::compare and the
     *  deduplication is defined by the equality used for the mode (exact equality for Exact, approximate equality for Approximative, see ChoiceSignature).
     */
    struct StateSignature {
        /*!
         * Find the given choice signature in the list.
         * Returns an (iterator,bool)-pair. The bool is true iff the given choice signature is contained.
         * The iterator is either (exact/approximately) equal to the given signature, or the position where it would be inserted.
         * Two choice signatures are approximately equal (w.r.t. a tolerance) iff they have equal structure (compareStructure) and their distr values pairwise
         * differ by at most the tolerance.
         */
        std::pair<ChoiceSignatureIterator, bool> find(ChoiceSignature const& signature, ValueType const tolerance) const;

        /*!
         * Like find(), but starts scanning at `hint` instead of locating the compare()-tie window via lower_bound.
         * @param hint any iterator in [l, r], where [l, r) is the window of entries compareStructure-equal to signature and within tolerance of its first
         * entry (r itself may lie outside that window, e.g. as an insertion point).
         * @note requires !signature.distr.empty().
         * @note returns {hint, false} if signature is not found.
         */
        std::pair<ChoiceSignatureIterator, bool> findWithHint(ChoiceSignatureIterator const hint, ChoiceSignature const& signature,
                                                              ValueType const tolerance) const
            requires(Mode == SignatureMode::Approximative);

        void insert(ChoiceSignature const& signature, ValueType const tolerance);

        std::vector<ChoiceSignature> choices;  // Always ordered and deduplicated as described above.
    };

    /*!
     * Builds the signature of the given choice index.
     * The underlying distribution maps blocks to aggregated probability.
     * @note the returned ChoiceSignature::distr is a view into choiceDistributionStorage; calling buildChoiceSignature(choiceIndex) again later overwrites it.
     */
    ChoiceSignature buildChoiceSignature(uint64_t const choiceIndex);

    /*!
     * Gets the signature of the given choice index. The signature must have been built before.
     * @note the returned ChoiceSignature::distr is a view into choiceDistributionStorage; calling buildChoiceSignature(choiceIndex) later overwrites it.
     */
    ChoiceSignature getChoiceSignature(uint64_t const choiceIndex) const;

    /*!
     * A reusable, sparse accumulator for building the distribution (Partition::Block -> ValueType) of a single choice, without allocating a fresh map for
     * every choice: values is a dense array indexed by a block's front() element (unique per block, since blocks are disjoint), and support records which
     * blocks were touched so that extract() only has to look at (and reset) those, not the whole array.
     * @note the cache must not be used across a change of the partition, since a block's front() element is only a stable identifier for that block as long
     * as the partition does not change.
     */
    class ChoiceSignatureCache {
       public:
        explicit ChoiceSignatureCache(uint64_t const numStates);

        /*!
         * Adds value to the accumulated value of the block b. Registers b as touched the first time this happens.
         */
        void addValue(Partition::Block const& b, ValueType const& value);

        /*!
         * Writes the accumulated (block, value) pairs, sorted by Partition::BlockCompare, into the given range (which must be large enough to hold them)
         * @return a sub-view over the entries actually written to dest.
         * @note resets the cache (i.e. all accumulated values) so that it can be reused for the next choice.
         */
        BlockDistributionView extract(BlockDistributionView const dest);

       private:
        std::vector<ValueType> values;
        std::vector<Partition::Block> support;
    } choiceSignatureCache;

    storm::models::sparse::Model<ValueType> const& model;
    Partition const& partition;
    std::optional<std::vector<uint64_t>> const& choiceClasses;

    /*!
     * Backing storage for all ChoiceSignature::distr views. Choice i is permanently assigned the sub-range starting at the same offset as row i within
     * model's transition matrix (i.e. distance(model.getTransitionMatrix().begin(), model.getTransitionMatrix().begin(i))), which is guaranteed to fit
     * distr's entries since distr can have at most as many entries as row i of the transition matrix (deduplicating by block only ever shrinks it). This
     * avoids allocating a fresh container for every buildChoiceSignature call. Sized to fit every row's entries at once, since (row-)offsets are permanent.
     */
    mutable std::vector<std::pair<Partition::Block, ValueType>> choiceDistributionStorage;

    /*!
     * Half of the tolerance passed to the constructor; only meaningful for approximate signatures.
     *
     * This is used to distribute the tolerance from the constructor across two approximate-equality (≈) checks.
     * To see why taking half of the tolerance is necessary, consider the following example:
     * Let s_1 and s_2 be two states of a block. Let c_1 be a choice of s_1 and let c_2 and c'_2 be two choices of s_2 such that c_1 ≈ c_2 ≈ c'_2.
     * Then, c'_2 does not need to appear in s_2's signature as it is represented by c_2.
     * The quotient might only have s_1 and c_1 as representatives for the block but c_1 ≈ c'_2 does not need to hold. c_1 ≈ c_2 ≈ c'_2 only imples that
     * c_1 and c'_2 are within twice of the tolerance considered for ≈.
     */
    ValueType const halfTolerance;

    std::vector<StateSignature> stateSignatureCache;
};

}  // namespace storm::bisimulation

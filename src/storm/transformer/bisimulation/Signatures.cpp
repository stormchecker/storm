#include "storm/transformer/bisimulation/Signatures.h"

#include <algorithm>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::bisimulation {

template<typename ValueType, SignatureMode Mode>
Signatures<ValueType, Mode>::ChoiceSignatureCache::ChoiceSignatureCache(uint64_t const numStates) : values(numStates, storm::utility::zero<ValueType>()) {}

template<typename ValueType, SignatureMode Mode>
void Signatures<ValueType, Mode>::ChoiceSignatureCache::addValue(Partition::Block const& b, ValueType const& value) {
    auto& current = values[b.front()];
    if (storm::utility::isZero(current)) {
        support.push_back(b);
    }
    current += value;
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::ChoiceSignatureCache::extract(BlockDistributionView const dest) -> BlockDistributionView {
    std::sort(support.begin(), support.end(), Partition::BlockCompare());
    STORM_LOG_ASSERT(support.size() <= dest.size(), "Destination span is too small to hold all entries.");
    auto result = dest.first(support.size());

    auto writeIt = result.begin();
    for (auto const& b : support) {
        auto& value = values[b.front()];
        *writeIt = std::make_pair(b, std::move(value));
        ++writeIt;
        value = storm::utility::zero<ValueType>();
    }
    if (result.size() != dest.size()) {
        // Mark the end of the written entries with an empty block.
        // This is used to easily get the choice signature later without rebuilding it. See getChoiceSignature.
        dest[result.size()] = std::pair<Partition::Block, ValueType>{};
    }
    support.clear();
    return result;
}

template<typename ValueType, SignatureMode Mode>
std::strong_ordering Signatures<ValueType, Mode>::ChoiceSignature::compareStructure(ChoiceSignature const& other) const {
    if (auto const cmp = choiceClass <=> other.choiceClass; cmp != 0) {
        return cmp;
    }
    if (auto const cmp = distr.size() <=> other.distr.size(); cmp != 0) {
        return cmp;
    }
    auto it2 = other.distr.begin();
    for (auto const& entry : distr) {
        if (auto const cmp = entry.first.size() <=> it2->first.size(); cmp != 0) {
            return cmp;
        }
        if (auto const cmp = entry.first.data() <=> it2->first.data(); cmp != 0) {
            return cmp;
        }
        ++it2;
    }
    return std::strong_ordering::equal;
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::ChoiceSignature::compare(ChoiceSignature const& other) const -> ComparisonResult {
    if (auto const cmp = compareStructure(other); cmp != 0) {
        return cmp;
    }
    STORM_LOG_ASSERT(distr.size() == other.distr.size(), "The distributions of two signatures with the same structure must have the same size.");
    // Only the distribution values can still differ. ValueType (e.g. RationalFunction, Interval) may lack <=>, so we fall back to </!= here.
    if constexpr (Mode == SignatureMode::Exact) {
        // Strong order: full lexicographic comparison.
        auto it2 = other.distr.begin();
        for (auto const& entry : distr) {
            if (entry.second != it2->second) {
                return entry.second < it2->second ? ComparisonResult::less : ComparisonResult::greater;
            }
            ++it2;
        }
    } else {
        // Weak order: compare only the first entry
        if (distr.empty()) {
            return ComparisonResult::equivalent;
        }
        auto const& value = distr.front().second;
        auto const& otherValue = other.distr.front().second;
        if (value != otherValue) {
            return value < otherValue ? ComparisonResult::less : ComparisonResult::greater;
        }
    }
    return ComparisonResult::equivalent;
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::StateSignature::find(ChoiceSignature const& signature, ValueType const tolerance) const
    -> std::pair<ChoiceSignatureIterator, bool> {
    if (choices.empty()) {
        return std::make_pair(choices.end(), false);
    }
    auto it = std::lower_bound(choices.begin(), choices.end(), signature, [](ChoiceSignature const& choice1, ChoiceSignature const& choice2) {
        return choice1.compare(choice2) == ChoiceSignature::ComparisonResult::less;
    });
    if constexpr (Mode == SignatureMode::Exact) {
        return std::make_pair(it, it != choices.end() && it->compare(signature) == std::strong_ordering::equal);
    } else {
        if (signature.distr.empty()) {
            // For empty choice signatures, it suffices to compare the structure.
            return std::make_pair(it, it != choices.end() && it->compareStructure(signature) == std::strong_ordering::equal);
        }
        return findWithHint(it, signature, tolerance);
    }
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::StateSignature::findWithHint(ChoiceSignatureIterator hint, ChoiceSignature const& signature, ValueType const tolerance) const
    -> std::pair<ChoiceSignatureIterator, bool>
    requires(Mode == SignatureMode::Approximative)
{
    // The hint may be the insertion point choices.end(), cf. the contract in the header: the forward scan below then does not run and the backward scan
    // starts at the last entry.
    STORM_LOG_ASSERT(hint >= choices.begin() && hint <= choices.end(), "Hint iterator must be within the range of choices.");
    STORM_LOG_ASSERT(!signature.distr.empty(), "Distributions must not be empty.");  // Don't deal with this special case here.

    auto const near = [&tolerance](ValueType const& value1, ValueType const& value2) { return storm::utility::abs<ValueType>(value1 - value2) <= tolerance; };
    auto const inWindow = [&near, &signature](ChoiceSignature const& choice) {
        return choice.compareStructure(signature) == std::strong_ordering::equal && near(choice.distr.front().second, signature.distr.front().second);
    };
    auto const found = [&signature, &near](ChoiceSignature const& choice) {
        // The first entry is already assumed to be near, so we check the others.
        auto it1 = choice.distr.begin() + 1;
        auto it2 = signature.distr.begin() + 1;
        for (; it1 != choice.distr.end() && it2 != signature.distr.end(); ++it1, ++it2) {
            if (!near(it1->second, it2->second)) {
                return false;
            }
        }
        return true;
    };

    // Scan the window in both directions to find an approxEqual entry, if any.
    for (ChoiceSignatureIterator scan = hint; scan != choices.end() && inWindow(*scan); ++scan) {
        if (found(*scan)) {
            return std::make_pair(scan, true);
        }
    }
    for (ChoiceSignatureIterator scan = hint; scan != choices.begin();) {
        --scan;
        if (!inWindow(*scan)) {
            break;
        }
        if (found(*scan)) {
            return std::make_pair(scan, true);
        }
    }
    return std::make_pair(hint, false);
}

template<typename ValueType, SignatureMode Mode>
void Signatures<ValueType, Mode>::StateSignature::insert(ChoiceSignature const& signature, ValueType const tolerance) {
    auto [it, found] = find(signature, tolerance);
    if (!found) {
        choices.insert(it, signature);
    }
}

template<typename ValueType, SignatureMode Mode>
Signatures<ValueType, Mode>::Signatures(storm::models::sparse::Model<ValueType> const& model, std::optional<std::vector<uint64_t>> const& choiceClasses,
                                        storm::bisimulation::Partition const& partition)
    requires(Mode == SignatureMode::Exact)
    : model(model),
      partition(partition),
      choiceClasses(choiceClasses),
      choiceSignatureCache(partition.getNumberOfElements()),
      choiceDistributionStorage(model.getTransitionMatrix().getEntryCount()),
      halfTolerance(storm::utility::zero<ValueType>()),
      stateSignatureCache(model.getNumberOfStates()) {}

template<typename ValueType, SignatureMode Mode>
Signatures<ValueType, Mode>::Signatures(storm::models::sparse::Model<ValueType> const& model, std::optional<std::vector<uint64_t>> const& choiceClasses,
                                        storm::bisimulation::Partition const& partition, ValueType const& tolerance)
    requires(Mode == SignatureMode::Approximative)
    : model(model),
      partition(partition),
      choiceClasses(choiceClasses),
      choiceSignatureCache(partition.getNumberOfElements()),
      choiceDistributionStorage(model.getTransitionMatrix().getEntryCount()),
      halfTolerance(tolerance / storm::utility::convertNumber<ValueType, uint64_t>(2)),
      stateSignatureCache(model.getNumberOfStates()) {}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::buildChoiceSignature(uint64_t const choiceIndex) -> ChoiceSignature {
    auto const& matrix = model.getTransitionMatrix();
    // Use the choiceSignatureCache to compute the distribution over successor blocks
    for (auto const& entry : matrix.getRow(choiceIndex)) {
        if (!storm::utility::isZero(entry.getValue())) {
            choiceSignatureCache.addValue(partition.getBlockOfElement(entry.getColumn()), entry.getValue());
        }
    }
    // Identify the choice distribution storage.
    auto const row = matrix.getRow(choiceIndex);
    uint64_t const offset = std::distance(matrix.begin(), row.begin());
    BlockDistributionView distrStorage(choiceDistributionStorage.data() + offset, row.getNumberOfEntries());

    // Extract the resulting distribution from the cache.
    return ChoiceSignature{.choiceClass = choiceClasses ? (*choiceClasses)[choiceIndex] : 0, .distr = choiceSignatureCache.extract(distrStorage)};
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::getChoiceSignature(uint64_t const choiceIndex) const -> ChoiceSignature {
    // Identify the choice distribution storage
    auto const& matrix = model.getTransitionMatrix();
    auto const row = matrix.getRow(choiceIndex);
    uint64_t const offset = std::distance(matrix.begin(), row.begin());
    BlockDistributionView distrStorage(choiceDistributionStorage.data() + offset, row.getNumberOfEntries());

    // Determine the right size by finding the first empty block (if any)
    uint64_t size = 0;
    for (auto const& [block, _] : distrStorage) {
        if (block.empty()) {
            break;
        }
        ++size;
    }

    return ChoiceSignature{.choiceClass = choiceClasses ? (*choiceClasses)[choiceIndex] : 0, .distr = distrStorage.first(size)};
}

template<typename ValueType, SignatureMode Mode>
void Signatures<ValueType, Mode>::updateStateSignature(uint64_t const stateIndex) {
    auto& sig = stateSignatureCache[stateIndex];
    sig.choices.clear();
    for (uint64_t const choiceIndex : model.getTransitionMatrix().getRowGroupIndices(stateIndex)) {
        sig.insert(buildChoiceSignature(choiceIndex), halfTolerance);
    }
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::getEquivalenceSplitOrder() const -> SplitOrder
    requires(Mode == SignatureMode::Exact)
{
    return SplitOrder(stateSignatureCache);
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::getStructuralSplitOrder() const -> SplitOrder
    requires(Mode == SignatureMode::Approximative)
{
    return SplitOrder(stateSignatureCache);
}

template<typename ValueType, SignatureMode Mode>
bool Signatures<ValueType, Mode>::SplitOrder::operator()(uint64_t const state1, uint64_t const state2) const {
    auto const& sig1 = signatures[state1];
    auto const& sig2 = signatures[state2];
    if (auto const cmp = sig1.choices.size() <=> sig2.choices.size(); cmp != 0) {
        return cmp < 0;
    }
    auto it2 = sig2.choices.begin();
    for (auto const& choice : sig1.choices) {
        if constexpr (Mode == SignatureMode::Exact) {
            // In exact mode, do a full comparison of the choice signatures
            if (auto const cmp = choice.compare(*it2); cmp != 0) {
                return cmp < 0;
            }
        } else {
            // In approximative mode, only sort based on the structure of the choice signatures
            if (auto const cmp = choice.compareStructure(*it2); cmp != 0) {
                return cmp < 0;
            }
        }
        ++it2;
    }
    return false;
}

template<typename ValueType, SignatureMode Mode>
auto Signatures<ValueType, Mode>::getApproximateSplitCondition() const -> SplitCondition
    requires(Mode == SignatureMode::Approximative)
{
    return SplitCondition(stateSignatureCache, halfTolerance);
}

template<typename ValueType, SignatureMode Mode>
bool Signatures<ValueType, Mode>::SplitCondition::operator()(uint64_t const state1, uint64_t const state2) const
    requires(Mode == SignatureMode::Approximative)
{
    auto const& sig1 = signatures[state1];
    auto const& sig2 = signatures[state2];
    // We can assume that sig1.choices and sig2.choices have the same pointwise structure (compareStructure-equivalent) as this comparison is only called after
    // splitting with respect to getStructuralSplitOrder.
    STORM_LOG_ASSERT(sig1.choices.size() == sig2.choices.size(), "SplitCondition should only be called for signatures with the same number of choices.");
    auto it1 = sig1.choices.begin();
    auto it2 = sig2.choices.begin();
    for (; it1 != sig1.choices.end() && it2 != sig2.choices.end(); ++it1, ++it2) {
        STORM_LOG_ASSERT(it1->distr.size() == it2->distr.size(), "SplitCondition should only be called for signatures with pointwise same choice structure.");
        if (it1->distr.empty()) {
            continue;
        }
        // Find a choice matching choice2 in sig1
        auto const [choiceInSig1It, foundIn1] = sig1.findWithHint(it1, *it2, tolerance);
        if (!foundIn1) {
            return true;
        }
        // If it1 was not the matching choice, we have to check if it1 also has a matching choice in sig2
        if (it1 != choiceInSig1It && !sig2.findWithHint(it2, *it1, tolerance).second) {
            return true;
        }
    }
    // Reaching this point means that we consider the signatures to be equal, so no split.
    return false;
}

template<typename ValueType, SignatureMode Mode>
void Signatures<ValueType, Mode>::extendQuotientData(QuotientData<ValueType>& quotientData, bool const createQuotientChoiceMapping) const {
    auto& signatureData = quotientData.signatureData.emplace();
    if (createQuotientChoiceMapping) {
        quotientData.toQuotientChoice.emplace(model.getNumberOfChoices(), 0);
    }
    signatureData.quotientChoiceGroupIndices.reserve(quotientData.toRepresentativeState.size() + 1);

    for (uint64_t quotientState = 0; quotientState < quotientData.toRepresentativeState.size(); ++quotientState) {
        auto const representativeState = quotientData.toRepresentativeState[quotientState];
        uint64_t const firstQuotientChoiceIndex = signatureData.toRepresentativeChoice.size();
        signatureData.quotientChoiceGroupIndices.push_back(firstQuotientChoiceIndex);
        // Relies on performSignatureBasedRefinement's postcondition that all cached signatures (including singleton blocks) are up to date.
        auto const& representativeSignature = stateSignatureCache[representativeState];

        std::vector<uint64_t> choiceSignatureToQuotientChoiceIndex(representativeSignature.choices.size(), std::numeric_limits<uint64_t>::max());
        for (uint64_t const choiceIndex : model.getTransitionMatrix().getRowGroupIndices(representativeState)) {
            auto [it, found] = representativeSignature.find(getChoiceSignature(choiceIndex), halfTolerance);
            STORM_LOG_ASSERT(found, "Expected to find the signature of representative state");
            uint64_t const choiceSignatureIndex = std::distance(representativeSignature.choices.begin(), it);
            if (choiceSignatureToQuotientChoiceIndex[choiceSignatureIndex] == std::numeric_limits<uint64_t>::max()) {
                // We see this representative choice for the first time. Establish some mappings
                uint64_t const quotientChoiceIndex = signatureData.toRepresentativeChoice.size();
                choiceSignatureToQuotientChoiceIndex[choiceSignatureIndex] = quotientChoiceIndex;
                signatureData.toRepresentativeChoice.push_back(choiceIndex);
                if (createQuotientChoiceMapping) {
                    (*quotientData.toQuotientChoice)[choiceIndex] = quotientChoiceIndex;
                }
                // Fill distribution from signature
                auto& distr = signatureData.quotientChoiceDistributions.emplace_back();
                for (auto const& [successorBlock, value] : it->distr) {
                    distr.emplace(quotientData.toQuotientState[successorBlock.front()], value);
                }
            } else {
                // We have already seen this representative choice before.
                if (createQuotientChoiceMapping) {
                    (*quotientData.toQuotientChoice)[choiceIndex] = choiceSignatureToQuotientChoiceIndex[choiceSignatureIndex];
                }
            }
        }
        if (createQuotientChoiceMapping) {
            // Handle choice mappings for other blocks. This is comparatively expensive, since (unlike the representative's own choices above) it visits
            // every choice of every non-representative state, so it is skipped entirely if the caller does not need toQuotientChoice.
            for (uint64_t const state : partition.getBlockOfElement(representativeState)) {
                if (state == representativeState) {
                    continue;
                }
                for (uint64_t const choiceIndex : model.getTransitionMatrix().getRowGroupIndices(state)) {
                    // This raw choice can be up to halfTolerance away from state's own canonical choice (established while updating state's signature
                    // during refinement), which in turn can be up to halfTolerance away from the representative's canonical choice (established by
                    // SplitCondition). Comparing the raw choice directly against the representative's canonical choice therefore spans two independent
                    // halfTolerance steps, i.e. exactly the tolerance originally passed to this Signatures instance.
                    auto [it, found] = representativeSignature.find(getChoiceSignature(choiceIndex), halfTolerance + halfTolerance);
                    STORM_LOG_ASSERT(found, "Expected to find the signature of non-representative state");
                    uint64_t const choiceSignatureIndex = std::distance(representativeSignature.choices.begin(), it);
                    STORM_LOG_ASSERT(choiceSignatureToQuotientChoiceIndex[choiceSignatureIndex] != std::numeric_limits<uint64_t>::max(),
                                     "Expected to have already seen this representative choice");
                    (*quotientData.toQuotientChoice)[choiceIndex] = choiceSignatureToQuotientChoiceIndex[choiceSignatureIndex];
                }
            }
        }
    }
    signatureData.quotientChoiceGroupIndices.push_back(signatureData.toRepresentativeChoice.size());
    signatureData.quotientChoiceGroupIndices.shrink_to_fit();
    signatureData.toRepresentativeChoice.shrink_to_fit();
    signatureData.quotientChoiceDistributions.shrink_to_fit();
}

template class Signatures<double, SignatureMode::Exact>;
template class Signatures<double, SignatureMode::Approximative>;
template class Signatures<storm::RationalNumber, SignatureMode::Exact>;
template class Signatures<storm::RationalNumber, SignatureMode::Approximative>;
template class Signatures<storm::RationalFunction, SignatureMode::Exact>;

}  // namespace storm::bisimulation

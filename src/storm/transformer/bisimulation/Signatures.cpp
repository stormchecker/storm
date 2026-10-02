#include "storm/transformer/bisimulation/Signatures.h"

#include <algorithm>
#include <limits>
#include <set>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/matching.h"
#include "storm/utility/vector.h"

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
bool Signatures<ValueType, Mode>::ChoiceSignature::approximatelyEqual(ChoiceSignature const& other, ValueType const& tolerance) const
    requires(Mode == SignatureMode::Approximative)
{
    if (compareStructure(other) != 0) {
        return false;
    }
    auto otherIt = other.distr.begin();
    for (auto const& [block, value] : distr) {
        if (storm::utility::abs<ValueType>(value - otherIt->second) > tolerance) {
            return false;
        }
        ++otherIt;
    }
    return true;
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
    // The input signature defines a (potentially empty) window inside the choices vector. That window is given by the entries that are compareStructure-equal
    // to signature and whose first entry is within tolerance of the first entry of signature.
    // As the choices are sorted by compare(), they can be devided into three consecutive, potentially empty segments:
    // the entries before the window, the window itself, and the entries after the window.

    STORM_LOG_ASSERT(hint >= choices.begin() && hint <= choices.end(), "Hint iterator must be within the range of choices.");
    STORM_LOG_ASSERT(!signature.distr.empty(), "Distributions must not be empty.");  // Don't deal with this special case here.

    auto const near = [&tolerance](ValueType const& value1, ValueType const& value2) { return storm::utility::abs<ValueType>(value1 - value2) <= tolerance; };
    enum class Location { BeforeWindow, InWindow, AfterWindow };
    // Locate in which segment of the choices vector the given choice is.
    auto const locate = [this, &signature, &near](ChoiceSignatureIterator choiceIt) {
        if (choiceIt == choices.end()) {
            return Location::AfterWindow;
        }
        auto const& choice = *choiceIt;
        if (auto const cmp = choice.compareStructure(signature); cmp != 0) {
            return cmp < 0 ? Location::BeforeWindow : Location::AfterWindow;
        }
        auto const& value = choice.distr.front().second;
        auto const& signatureValue = signature.distr.front().second;
        if (near(value, signatureValue)) {
            return Location::InWindow;
        }
        return value < signatureValue ? Location::BeforeWindow : Location::AfterWindow;
    };
    // Returns true iff the choice is considered equivalent to the signature. Assumes that the given choice is in the window.
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

    // Assumes an iterator that points inside the window and scans all window contents to the right (it excluded)
    auto const scanToRightBoundary = [&](ChoiceSignatureIterator it) {
        ++it;
        while (locate(it) == Location::InWindow) {
            if (found(*it)) {
                return std::make_pair(it, true);
            }
            ++it;
        }
        return std::make_pair(it, false);
    };
    // Assumes an iterator that points inside the window and scans all window contents to the left (it excluded)
    auto const scanToLeftBoundary = [&](ChoiceSignatureIterator it) {
        while (it != choices.begin()) {
            --it;
            if (locate(it) != Location::InWindow) {
                break;
            }
            if (found(*it)) {
                return std::make_pair(it, true);
            }
        }
        return std::make_pair(it, false);
    };

    // Callers usually pass a hint that lies in the window or is close to it. There are three cases for the hint:
    // a) inside, b) before, or c) after the window.
    auto const hintLocation = locate(hint);
    if (hintLocation == Location::InWindow) {  // case a), so we might need to scan in both directions
        if (found(*hint)) {
            return std::make_pair(hint, true);
        }
        if (auto resRight = scanToRightBoundary(hint); resRight.second) {
            return resRight;
        }
        if (auto resLeft = scanToLeftBoundary(hint); resLeft.second) {
            return resLeft;
        }
    } else {
        auto start = hint;
        auto startLocation = hintLocation;
        if (hintLocation == Location::BeforeWindow) {  // case b), find right boundary of window of the window
            while (startLocation == Location::BeforeWindow) {
                ++start;
                startLocation = locate(start);
            }
        } else {  // case c), find left boundary of the window
            while (startLocation == Location::AfterWindow && start != choices.begin()) {
                --start;
                startLocation = locate(start);
            }
        }
        if (startLocation == Location::InWindow) {
            if (found(*start)) {
                return std::make_pair(start, true);
            }
            if (auto result = hintLocation == Location::BeforeWindow ? scanToRightBoundary(start) : scanToLeftBoundary(start); result.second) {
                return result;
            }
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
    : choiceSignatureCache(partition.getNumberOfElements()),
      model(model),
      partition(partition),
      choiceClasses(choiceClasses),
      choiceDistributionStorage(model.getTransitionMatrix().getEntryCount()),
      halfTolerance(storm::utility::zero<ValueType>()),
      stateSignatureCache(model.getNumberOfStates()) {}

template<typename ValueType, SignatureMode Mode>
Signatures<ValueType, Mode>::Signatures(storm::models::sparse::Model<ValueType> const& model, std::optional<std::vector<uint64_t>> const& choiceClasses,
                                        storm::bisimulation::Partition const& partition, ValueType const& tolerance)
    requires(Mode == SignatureMode::Approximative)
    : choiceSignatureCache(partition.getNumberOfElements()),
      model(model),
      partition(partition),
      choiceClasses(choiceClasses),
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
        // Find a choice matching choice2 in sig1. As both signatures are sorted the same way, it1 is usually close to the window of choice2.
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
void Signatures<ValueType, Mode>::addQuotientChoiceMapping(uint64_t const state, StateSignature const& representativeSignature,
                                                           std::vector<uint64_t> const& choiceSignatureToQuotientChoiceIndex,
                                                           std::vector<uint64_t>& toQuotientChoice) const {
    constexpr uint64_t Invalid = std::numeric_limits<uint64_t>::max();
    auto const& stateSignature = stateSignatureCache[state];
    auto const choiceIndices = model.getTransitionMatrix().getRowGroupIndices(state);
    STORM_LOG_ASSERT(stateSignature.choices.size() == representativeSignature.choices.size(), "States in a block have different number of choice signatures.");

    // Every choice of the state is represented by one of the state's own choice signatures. That representation is surjective, since each of those signatures
    // was established from a choice of this state. It thus suffices to find a bijective matching between the choice signatures of the state and those of the
    // representative: the composition of the two is surjective on the choice signatures of the representative as well. Such a surjective mapping is needed,
    // as otherwise a scheduler for the quotient model could not be translated back to a choice for this state. Both steps are within halfTolerance, i.e. the
    // values of a choice and those of the quotient choice representing it differ by at most the tolerance this instance was created with.
    std::vector<uint64_t> choiceSignatureMatching;
    if constexpr (Mode == SignatureMode::Exact) {
        // The signatures of two states of the same block are equal entry-wise, so we can match them in order.
        choiceSignatureMatching = storm::utility::vector::buildVectorForRange<uint64_t>(0, stateSignature.choices.size());
        STORM_LOG_ASSERT(std::all_of(choiceSignatureMatching.begin(), choiceSignatureMatching.end(),
                                     [&stateSignature, &representativeSignature](uint64_t const i) {
                                         return stateSignature.choices[i].compare(representativeSignature.choices[i]) == std::strong_ordering::equal;
                                     }),
                         "In exact mode, choice signatures are expected to be equal for states in the same block.");
    } else {
        // In approximate mode, this is in fact a matching problem on a bipartite graph.
        auto optionalChoiceSignatureMatching = storm::utility::findPerfectMatching(
            stateSignature.choices.size(), [&stateSignature, &representativeSignature, this](uint64_t const stateChoice, uint64_t const representativeChoice) {
                return stateSignature.choices[stateChoice].approximatelyEqual(representativeSignature.choices[representativeChoice], halfTolerance);
            });
        // Such a matching does not have to exist: approximate equality is not transitive, so two choice signatures of the state can have the same, single
        // approximately equal partner in the representative's signature, which is not enough of a reason to split the block during refinement.
        STORM_LOG_THROW(optionalChoiceSignatureMatching.has_value(), storm::exceptions::UnexpectedException,
                        "Unable to match the choices of state " << state << " with the choices of the state that represents it in the quotient. "
                                                                << "Try again with a smaller tolerance.");
        choiceSignatureMatching = std::move(*optionalChoiceSignatureMatching);
    }

    for (uint64_t const choiceIndex : choiceIndices) {
        auto [it, found] = stateSignature.find(getChoiceSignature(choiceIndex), halfTolerance);
        STORM_LOG_ASSERT(found, "Expected to find the signature of a choice in the signature of its own state");
        uint64_t const indexInStateSignature = std::distance(stateSignature.choices.begin(), it);
        uint64_t const indexInRepresentativeSignature = choiceSignatureMatching[indexInStateSignature];
        STORM_LOG_ASSERT(choiceSignatureToQuotientChoiceIndex[indexInRepresentativeSignature] != Invalid,
                         "Expected to have already seen this representative choice");
        toQuotientChoice[choiceIndex] = choiceSignatureToQuotientChoiceIndex[indexInRepresentativeSignature];
    }

    // Finally, assert that we indeed have a surjective mapping
    STORM_LOG_ASSERT(([&]() {
                         std::set<uint64_t> seenQuotientChoices;
                         for (uint64_t const choiceIndex : choiceIndices) {
                             seenQuotientChoices.insert(toQuotientChoice[choiceIndex]);
                         }
                         return seenQuotientChoices.size() == representativeSignature.choices.size();
                     }()),
                     "The mapping from state choices to quotient choices is not surjective.");
}

template<typename ValueType, SignatureMode Mode>
void Signatures<ValueType, Mode>::extendQuotientData(QuotientData<ValueType>& quotientData, bool const createQuotientChoiceMapping) const {
    auto& signatureData = quotientData.signatureData.emplace();
    if (createQuotientChoiceMapping) {
        quotientData.toQuotientChoice.emplace(model.getNumberOfChoices(), 0);
    }
    signatureData.quotientChoiceGroupIndices.reserve(quotientData.toRepresentativeState.size() + 1);

    // Scratch space for mappings, reused for every state.
    std::vector<uint64_t> choiceSignatureToQuotientChoiceIndex;  // Maps choiceSignature indices to quotient model choice indices
    constexpr uint64_t Invalid = std::numeric_limits<uint64_t>::max();

    for (uint64_t quotientState = 0; quotientState < quotientData.toRepresentativeState.size(); ++quotientState) {
        auto const representativeState = quotientData.toRepresentativeState[quotientState];
        uint64_t const firstQuotientChoiceIndex = signatureData.toRepresentativeChoice.size();
        signatureData.quotientChoiceGroupIndices.push_back(firstQuotientChoiceIndex);
        // Relies on performSignatureBasedRefinement's postcondition that all cached signatures (including singleton blocks) are up to date.
        auto const& representativeSignature = stateSignatureCache[representativeState];

        // At the quotientState, the choices should appear in the same order as the represented choices at the representativeState. This prevents unexpected
        // effects downstream (e.g. different quality of the initial policy in policy iteration). For Markov automata with hybrid states, this is even necessary
        // to ensure that the Markovian choice remains the first one.
        // The choiceSignatureToQuotientChoiceIndex mapping below permutes the choices as they appear in representativeSignature into the right order.
        choiceSignatureToQuotientChoiceIndex.assign(representativeSignature.choices.size(), Invalid);
        for (uint64_t const choiceIndex : model.getTransitionMatrix().getRowGroupIndices(representativeState)) {
            auto [it, found] = representativeSignature.find(getChoiceSignature(choiceIndex), halfTolerance);
            STORM_LOG_ASSERT(found, "Expected to find the signature of representative state");
            uint64_t const choiceSignatureIndex = std::distance(representativeSignature.choices.begin(), it);
            if (choiceSignatureToQuotientChoiceIndex[choiceSignatureIndex] == Invalid) {
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
            // Handle choice mappings for the other states of the block. This is comparatively expensive, since (unlike the representative's own choices
            // above) it visits every choice of every non-representative state, so it is skipped entirely if the caller does not need toQuotientChoice.
            for (uint64_t const state : partition.getBlockOfElement(representativeState)) {
                if (state != representativeState) {
                    addQuotientChoiceMapping(state, representativeSignature, choiceSignatureToQuotientChoiceIndex, *quotientData.toQuotientChoice);
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

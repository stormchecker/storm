#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/elements/DFTElementType.h"
#include "storm-dft/storage/elements/DFTElements.h"
#include "storm/exceptions/InvalidArgumentException.h"

namespace storm::dft {
namespace storage {

struct GateGroupToHash {
    static constexpr uint_fast64_t fivebitmask = (1 << 6) - 1;
    static constexpr uint_fast64_t eightbitmask = (1 << 9) - 1;

    /**
     * Hash function, which ensures that the colours are sorted according to their rank.
     */
    uint_fast64_t operator()(storm::dft::storage::elements::DFTElementType type, size_t nrChildren, size_t nrParents, size_t nrPDEPs, size_t nrRestrictions,
                             size_t rank) const {
        // Sets first bit to 1
        uint_fast64_t groupHash = static_cast<uint_fast64_t>(1) << 63;
        // Assumes 5 bits for the rank,
        groupHash |= (static_cast<uint_fast64_t>(rank) & fivebitmask) << (62 - 5);
        // 8 bits for the nrChildren
        groupHash |= (static_cast<uint_fast64_t>(nrChildren) & eightbitmask) << (62 - 5 - 8);
        // 5 bits for nrParents
        groupHash |= (static_cast<uint_fast64_t>(nrParents) & fivebitmask) << (62 - 5 - 8 - 5);
        // 5 bits for nrPDEPs
        groupHash |= (static_cast<uint_fast64_t>(nrPDEPs) & fivebitmask) << (62 - 5 - 8 - 5 - 5);
        // 5 bits for nrRestrictions
        groupHash |= (static_cast<uint_fast64_t>(nrPDEPs) & fivebitmask) << (62 - 5 - 8 - 5 - 5 - 5);
        // 5 bits for the type
        groupHash |= (static_cast<uint_fast64_t>(type) & fivebitmask) << (62 - 5 - 8 - 5 - 5 - 5 - 5);
        return groupHash;
    }
};

template<typename ValueType>
struct BEColourClass {
    BEColourClass() = default;

    BEColourClass(storm::dft::storage::elements::BEType type, size_t nrParents, size_t nrOutDep, size_t nrInDep, size_t nrRestrictions)
        : type(type), nrParents(nrParents), nrOutDep(nrOutDep), nrInDep(nrInDep), nrRestrictions(nrRestrictions) {
        STORM_LOG_ASSERT(type == storm::dft::storage::elements::BEType::SAMPLES, "Expected type CONSTANT but got type " << type);
    }

    BEColourClass(storm::dft::storage::elements::BEType type, bool fail, size_t nrParents, size_t nrOutDep, size_t nrInDep, size_t nrRestrictions)
        : type(type), nrParents(nrParents), nrOutDep(nrOutDep), nrInDep(nrInDep), nrRestrictions(nrRestrictions), failed(fail) {
        STORM_LOG_ASSERT(type == storm::dft::storage::elements::BEType::CONSTANT, "Expected type CONSTANT but got type " << type);
    }

    BEColourClass(storm::dft::storage::elements::BEType type, ValueType valA, ValueType valB, size_t nrParents, size_t nrOutDep, size_t nrInDep,
                  size_t nrRestrictions)
        : type(type), nrParents(nrParents), nrOutDep(nrOutDep), nrInDep(nrInDep), nrRestrictions(nrRestrictions), valueA(valA), valueB(valB) {
        STORM_LOG_ASSERT(type == storm::dft::storage::elements::BEType::EXPONENTIAL || type == storm::dft::storage::elements::BEType::PROBABILITY ||
                             type == storm::dft::storage::elements::BEType::LOGNORMAL || type == storm::dft::storage::elements::BEType::WEIBULL,
                         "Unexpected type " << type);
    }

    BEColourClass(storm::dft::storage::elements::BEType type, ValueType valA, ValueType valB, size_t phases, size_t nrParents, size_t nrOutDep, size_t nrInDep,
                  size_t nrRestrictions)
        : type(type), nrParents(nrParents), nrOutDep(nrOutDep), nrInDep(nrInDep), nrRestrictions(nrRestrictions), valueA(valA), valueB(valB), phases(phases) {
        STORM_LOG_ASSERT(type == storm::dft::storage::elements::BEType::ERLANG, "Expected type ERLANG but got type " << type);
    }

    storm::dft::storage::elements::BEType type;
    size_t nrParents;
    size_t nrOutDep;
    size_t nrInDep;
    size_t nrRestrictions;
    ValueType valueA;  // Meaning of value depends on type (active rate/probability, mean, shape)
    ValueType valueB;  // Meaning of value depends on type (passive rate/probability, stddev, rate)
    bool failed;       // For constant BE
    size_t phases;     // For Erlang BE
};

template<typename ValueType>
bool operator==(BEColourClass<ValueType> const& lhs, BEColourClass<ValueType> const& rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }
    if (lhs.nrParents != rhs.nrParents || lhs.nrOutDep != rhs.nrOutDep || lhs.nrInDep != rhs.nrInDep || lhs.nrRestrictions != rhs.nrRestrictions) {
        return false;
    }
    switch (lhs.type) {
        case storm::dft::storage::elements::BEType::CONSTANT:
            return lhs.failed == rhs.failed;
        case storm::dft::storage::elements::BEType::PROBABILITY:
        case storm::dft::storage::elements::BEType::EXPONENTIAL:
        case storm::dft::storage::elements::BEType::LOGNORMAL:
        case storm::dft::storage::elements::BEType::WEIBULL:
            return lhs.valueA == rhs.valueA && lhs.valueB == rhs.valueB;
        case storm::dft::storage::elements::BEType::ERLANG:
            return lhs.valueA == rhs.valueA && lhs.valueA == rhs.valueB && lhs.phases == rhs.phases;
        case storm::dft::storage::elements::BEType::SAMPLES:
            // Samples are not compared and always assumed to be non-symmetric
            return false;
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "BE of type '" << lhs.type << "' is not known.");
            break;
    }
}

template<typename ValueType>
struct BijectionCandidates {
    std::unordered_map<size_t, std::vector<size_t>> gateCandidates;
    std::unordered_map<BEColourClass<ValueType>, std::vector<size_t>> beCandidates;
    std::unordered_map<size_t, std::vector<size_t>> pdepCandidates;
    std::unordered_map<size_t, std::vector<size_t>> restrictionCandidates;

    size_t nrGroups() const {
        return gateCandidates.size() + beCandidates.size() + pdepCandidates.size() + restrictionCandidates.size();
    }

    size_t size() const {
        return nrGates() + nrBEs() + nrDeps() + nrRestrictions();
    }

    size_t nrGates() const {
        size_t res = 0;
        for (auto const& x : gateCandidates) {
            res += x.second.size();
        }
        return res;
    }

    size_t nrBEs() const {
        size_t res = 0;
        for (auto const& x : beCandidates) {
            res += x.second.size();
        }
        return res;
    }

    size_t nrDeps() const {
        size_t res = 0;
        for (auto const& x : pdepCandidates) {
            res += x.second.size();
        }
        return res;
    }

    size_t nrRestrictions() const {
        size_t res = 0;
        for (auto const& x : restrictionCandidates) {
            res += x.second.size();
        }
        return res;
    }

    bool hasGate(size_t index) const {
        for (auto const& x : gateCandidates) {
            for (auto const& ind : x.second) {
                if (index == ind)
                    return true;
            }
        }
        return false;
    }

    bool hasBE(size_t index) const {
        for (auto const& x : beCandidates) {
            for (auto const& ind : x.second) {
                if (index == ind)
                    return true;
            }
        }
        return false;
    }

    bool hasDep(size_t index) const {
        for (auto const& x : pdepCandidates) {
            for (auto const& ind : x.second) {
                if (index == ind)
                    return true;
            }
        }
        return false;
    }

    bool hasRestriction(size_t index) const {
        for (auto const& x : restrictionCandidates) {
            for (auto const& ind : x.second) {
                if (index == ind)
                    return true;
            }
        }
        return false;
    }

    bool has(size_t index) const {
        return hasGate(index) || hasBE(index) || hasDep(index) || hasRestriction(index);
    }

    size_t trivialGateGroups() const {
        size_t res = 0;
        for (auto const& x : gateCandidates) {
            if (x.second.size() == 1)
                ++res;
        }
        return res;
    }

    size_t trivialBEGroups() const {
        size_t res = 0;
        for (auto const& x : beCandidates) {
            if (x.second.size() == 1)
                ++res;
        }
        return res;
    }
};

template<typename ValueType>
class DFTColouring {
    DFT<ValueType> const& dft;
    std::unordered_map<size_t, size_t> gateColour;
    std::unordered_map<size_t, BEColourClass<ValueType>> beColour;
    std::unordered_map<size_t, size_t> depColour;
    std::unordered_map<size_t, size_t> restrictionColour;
    GateGroupToHash gateColourizer;

   public:
    DFTColouring(DFT<ValueType> const& ft) : dft(ft) {
        for (size_t id = 0; id < dft.nrElements(); ++id) {
            if (dft.isBasicElement(id)) {
                colourize(dft.getBasicElement(id));
            } else if (dft.isGate(id)) {
                colourize(dft.getGate(id));
            } else if (dft.isDependency(id)) {
                colourize(dft.getDependency(id));
            } else {
                STORM_LOG_ASSERT(dft.isRestriction(id), "Element is no restriction.");
                colourize(dft.getRestriction(id));
            }
        }
    }

    bool hasSameColour(size_t index1, size_t index2) const {
        return beColour.at(index1) == beColour.at(index2);
    }

    BijectionCandidates<ValueType> colourSubdft(std::vector<size_t> const& subDftIndices) const {
        BijectionCandidates<ValueType> res;
        for (size_t index : subDftIndices) {
            if (dft.isBasicElement(index)) {
                auto it = res.beCandidates.find(beColour.at(index));
                if (it != res.beCandidates.end()) {
                    it->second.push_back(index);
                } else {
                    res.beCandidates[beColour.at(index)] = std::vector<size_t>({index});
                }
            } else if (dft.isGate(index)) {
                auto it = res.gateCandidates.find(gateColour.at(index));
                if (it != res.gateCandidates.end()) {
                    it->second.push_back(index);
                } else {
                    res.gateCandidates[gateColour.at(index)] = std::vector<size_t>({index});
                }
            } else if (dft.isDependency(index)) {
                auto it = res.pdepCandidates.find(depColour.at(index));
                if (it != res.pdepCandidates.end()) {
                    it->second.push_back(index);
                } else {
                    res.pdepCandidates[depColour.at(index)] = std::vector<size_t>({index});
                }
            } else {
                STORM_LOG_ASSERT(dft.isRestriction(index), "Element is no restriction.");
                auto it = res.restrictionCandidates.find(restrictionColour.at(index));
                if (it != res.restrictionCandidates.end()) {
                    it->second.push_back(index);
                } else {
                    res.restrictionCandidates[restrictionColour.at(index)] = std::vector<size_t>({index});
                }
            }
        }
        return res;
    }

   protected:
    void colourize(std::shared_ptr<const storm::dft::storage::elements::DFTBE<ValueType>> const& be) {
        switch (be->beType()) {
            case storm::dft::storage::elements::BEType::CONSTANT: {
                auto beConst = std::static_pointer_cast<storm::dft::storage::elements::BEConst<ValueType> const>(be);
                beColour[beConst->id()] =
                    BEColourClass<ValueType>(beConst->beType(), beConst->failed(), beConst->nrParents(), beConst->nrOutgoingDependencies(),
                                             beConst->nrIngoingDependencies(), beConst->nrRestrictions());
                break;
            }
            case storm::dft::storage::elements::BEType::PROBABILITY: {
                auto beProb = std::static_pointer_cast<storm::dft::storage::elements::BEProbability<ValueType> const>(be);
                beColour[beProb->id()] =
                    BEColourClass<ValueType>(beProb->beType(), beProb->activeFailureProbability(), beProb->passiveFailureProbability(), beProb->nrParents(),
                                             beProb->nrOutgoingDependencies(), beProb->nrIngoingDependencies(), beProb->nrRestrictions());
                break;
            }
            case storm::dft::storage::elements::BEType::EXPONENTIAL: {
                auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<ValueType> const>(be);
                beColour[beExp->id()] = BEColourClass<ValueType>(beExp->beType(), beExp->activeFailureRate(), beExp->passiveFailureRate(), beExp->nrParents(),
                                                                 beExp->nrOutgoingDependencies(), beExp->nrIngoingDependencies(), beExp->nrRestrictions());
                break;
            }
            case storm::dft::storage::elements::BEType::ERLANG: {
                auto beErlang = std::static_pointer_cast<storm::dft::storage::elements::BEErlang<ValueType> const>(be);
                beColour[beErlang->id()] = BEColourClass<ValueType>(beErlang->beType(), beErlang->activeFailureRate(), beErlang->passiveFailureRate(),
                                                                    beErlang->phases(), beErlang->nrParents(), beErlang->nrOutgoingDependencies(),
                                                                    beErlang->nrIngoingDependencies(), beErlang->nrRestrictions());
                break;
            }
            case storm::dft::storage::elements::BEType::LOGNORMAL: {
                auto beLog = std::static_pointer_cast<storm::dft::storage::elements::BELogNormal<ValueType> const>(be);
                beColour[beLog->id()] = BEColourClass<ValueType>(beLog->beType(), beLog->mean(), beLog->standardDeviation(), beLog->nrParents(),
                                                                 beLog->nrOutgoingDependencies(), beLog->nrIngoingDependencies(), beLog->nrRestrictions());
                break;
            }
            case storm::dft::storage::elements::BEType::WEIBULL: {
                auto beWeibull = std::static_pointer_cast<storm::dft::storage::elements::BEWeibull<ValueType> const>(be);
                beColour[beWeibull->id()] =
                    BEColourClass<ValueType>(beWeibull->beType(), beWeibull->shape(), beWeibull->rate(), beWeibull->nrParents(),
                                             beWeibull->nrOutgoingDependencies(), beWeibull->nrIngoingDependencies(), beWeibull->nrRestrictions());
                break;
            }
            case storm::dft::storage::elements::BEType::SAMPLES: {
                auto beSamples = std::static_pointer_cast<storm::dft::storage::elements::BESamples<ValueType> const>(be);
                beColour[beSamples->id()] = BEColourClass<ValueType>(beSamples->beType(), beSamples->nrParents(), beSamples->nrOutgoingDependencies(),
                                                                     beSamples->nrIngoingDependencies(), beSamples->nrRestrictions());
                break;
            }
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "BE of type '" << be->beType() << "' is not known.");
                break;
        }
    }

    void colourize(std::shared_ptr<const storm::dft::storage::elements::DFTGate<ValueType>> const& gate) {
        STORM_LOG_TRACE("Colour " << gate->id() << ": " << gate->type() << " " << gate->nrChildren() << " " << gate->rank() << ".");
        gateColour[gate->id()] =
            gateColourizer(gate->type(), gate->nrChildren(), gate->nrParents(), gate->nrOutgoingDependencies(), gate->nrRestrictions(), gate->rank());
        STORM_LOG_TRACE("Coloured " << gate->id() << " with " << gateColour[gate->id()] << ".");
    }

    void colourize(std::shared_ptr<const storm::dft::storage::elements::DFTDependency<ValueType>> const& dep) {
        STORM_LOG_TRACE("Colour " << dep->id() << ": " << dep->type() << " " << (1 + dep->dependentEvents().size()) << " " << dep->rank() << ".");
        depColour[dep->id()] = gateColourizer(dep->type(), (1 + dep->dependentEvents().size()), 0, 0, 0, dep->rank());
        STORM_LOG_TRACE("Coloured " << dep->id() << " with " << restrictionColour[dep->id()] << ".");
    }

    void colourize(std::shared_ptr<const storm::dft::storage::elements::DFTRestriction<ValueType>> const& restr) {
        STORM_LOG_TRACE("Colour " << restr->id() << ": " << restr->type() << " " << restr->nrChildren() << " " << restr->rank() << ".");
        restrictionColour[restr->id()] = gateColourizer(restr->type(), restr->nrChildren(), 0, 0, 0, restr->rank());
        STORM_LOG_TRACE("Coloured " << restr->id() << " with " << restrictionColour[restr->id()] << ".");
    }
};

/**
 * Saves isomorphism between subtrees
 */
template<typename ValueType>
class DFTIsomorphismCheck {
    /// Coloured nodes as provided by the input: left hand side
    BijectionCandidates<ValueType> const& bleft;
    /// Coloured nodes as provided by the input: right hand side.
    BijectionCandidates<ValueType> const& bright;
    /// Whether the colourings are compatible
    bool candidatesCompatible = true;
    /// Current bijection
    std::map<size_t, size_t> bijection;
    /// Current permutations of right hand side groups which lead to the homomorphism.
    /// Contains only colours with more than one member.
    BijectionCandidates<ValueType> currentPermutations;
    DFT<ValueType> const& dft;

   public:
    DFTIsomorphismCheck(BijectionCandidates<ValueType> const& left, BijectionCandidates<ValueType> const& right, DFT<ValueType> const& dft)
        : bleft(left), bright(right), dft(dft) {
        candidatesCompatible = checkCompatibility();
    }

    /**
     * Checks whether the candidates are compatible, that is, checks the colours and the number of members for each colour.
     * @return True iff compatible, ie if the preliminary check allows for a isomorphism.
     */
    bool compatible() {
        return candidatesCompatible;
    }

    /**
     * Returns the isomorphism
     * Can only be called after the findIsomorphism procedure returned that an isomorphism has found.
     * @see findIsomorphism
     */
    std::map<size_t, size_t> const& getIsomorphism() const {
        return bijection;
    }

    /**
     * Check whether another isomorphism exists.
     *
     * @return true iff another isomorphism exists.
     */
    bool findNextIsomorphism() {
        if (!candidatesCompatible) {
            return false;
        }
        if (bijection.empty()) {
            constructInitialBijection();
        } else {
            if (!findNextBijection()) {
                return false;
            }
        }
        while (!check()) {
            // continue our search
            if (!findNextBijection()) {
                // No further bijections to check, no is
                return false;
            }
        }
        return true;
    }

   protected:
    /**
     * Construct the initial bijection.
     */
    void constructInitialBijection() {
        STORM_LOG_ASSERT(candidatesCompatible, "Candidates are not compatible.");
        // We first construct the currentPermutations, which helps to determine the current state of the check.
        initializePermutationsAndTreatTrivialGroups(bleft.beCandidates, bright.beCandidates, currentPermutations.beCandidates);
        initializePermutationsAndTreatTrivialGroups(bleft.gateCandidates, bright.gateCandidates, currentPermutations.gateCandidates);
        initializePermutationsAndTreatTrivialGroups(bleft.pdepCandidates, bright.pdepCandidates, currentPermutations.pdepCandidates);
        initializePermutationsAndTreatTrivialGroups(bleft.restrictionCandidates, bright.restrictionCandidates, currentPermutations.restrictionCandidates);
        STORM_LOG_TRACE(bijection.size() << " vs. " << bleft.size() << " vs. " << bright.size());
        STORM_LOG_ASSERT(bijection.size() == bleft.size(), "No. of bijection elements do not match.");
    }

    /**
     * Construct the next bijection
     * @return true if a next bijection exists.
     */
    bool findNextBijection() {
        STORM_LOG_ASSERT(candidatesCompatible, "Candidates are not compatible.");
        bool foundNext = false;
        if (!currentPermutations.beCandidates.empty()) {
            auto it = currentPermutations.beCandidates.begin();
            while (!foundNext && it != currentPermutations.beCandidates.end()) {
                foundNext = std::next_permutation(it->second.begin(), it->second.end());
                ++it;
            }
        }
        if (!foundNext && !currentPermutations.gateCandidates.empty()) {
            auto it = currentPermutations.gateCandidates.begin();
            while (!foundNext && it != currentPermutations.gateCandidates.end()) {
                foundNext = std::next_permutation(it->second.begin(), it->second.end());
                ++it;
            }
        }

        if (!foundNext && !currentPermutations.pdepCandidates.empty()) {
            auto it = currentPermutations.pdepCandidates.begin();
            while (!foundNext && it != currentPermutations.pdepCandidates.end()) {
                foundNext = std::next_permutation(it->second.begin(), it->second.end());
                ++it;
            }
        }

        if (!foundNext && !currentPermutations.restrictionCandidates.empty()) {
            auto it = currentPermutations.restrictionCandidates.begin();
            while (!foundNext && it != currentPermutations.restrictionCandidates.end()) {
                foundNext = std::next_permutation(it->second.begin(), it->second.end());
                ++it;
            }
        }

        if (foundNext) {
            for (auto const& colour : bleft.beCandidates) {
                if (colour.second.size() > 1) {
                    STORM_LOG_ASSERT(currentPermutations.beCandidates.find(colour.first) != currentPermutations.beCandidates.end(), "Colour not found.");
                    zipVectorsIntoMap(colour.second, currentPermutations.beCandidates.find(colour.first)->second, bijection);
                }
            }

            for (auto const& colour : bleft.gateCandidates) {
                if (colour.second.size() > 1) {
                    STORM_LOG_ASSERT(currentPermutations.gateCandidates.find(colour.first) != currentPermutations.gateCandidates.end(), "Colour not found.");
                    zipVectorsIntoMap(colour.second, currentPermutations.gateCandidates.find(colour.first)->second, bijection);
                }
            }

            for (auto const& colour : bleft.pdepCandidates) {
                if (colour.second.size() > 1) {
                    STORM_LOG_ASSERT(currentPermutations.pdepCandidates.find(colour.first) != currentPermutations.pdepCandidates.end(), "Colour not found.");
                    zipVectorsIntoMap(colour.second, currentPermutations.pdepCandidates.find(colour.first)->second, bijection);
                }
            }

            for (auto const& colour : bleft.restrictionCandidates) {
                if (colour.second.size() > 1) {
                    STORM_LOG_ASSERT(currentPermutations.restrictionCandidates.find(colour.first) != currentPermutations.restrictionCandidates.end(),
                                     "Colour not found.");
                    zipVectorsIntoMap(colour.second, currentPermutations.restrictionCandidates.find(colour.first)->second, bijection);
                }
            }
        }

        return foundNext;
    }

    bool check() const {
        // Perform additional checks not yet covered by the colouring
        STORM_LOG_ASSERT(bijection.size() == bleft.size(), "No. of bijection elements do not match.");
        for (auto const& indexpair : bijection) {
            if (!dft.getElement(indexpair.first)->isTypeEqualTo(*dft.getElement(indexpair.second))) {
                // In-depth type check failed, e.g. voting thresholds do not match
                return false;
            }
            if (dft.getElement(indexpair.first)->isRelevant() || dft.getElement(indexpair.second)->isRelevant()) {
                // Relevant events are not symmetric
                return false;
            }
            if (dft.isGate(indexpair.first)) {
                STORM_LOG_ASSERT(dft.isGate(indexpair.second), "Element is no gate.");
                auto const& lGate = dft.getGate(indexpair.first);
                auto const& rGate = dft.getGate(indexpair.second);
                // Compare children for gates
                if (!checkChildren(lGate->children(), rGate->children(), !lGate->isStaticElement())) {
                    return false;
                }
            } else if (dft.isDependency(indexpair.first)) {
                STORM_LOG_ASSERT(dft.isDependency(indexpair.second), "Element is no dependency.");
                auto const& lDep = dft.getDependency(indexpair.first);
                auto const& rDep = dft.getDependency(indexpair.second);

                if (bijection.at(lDep->triggerEvent()->id()) != rDep->triggerEvent()->id()) {
                    // Symmetric dependencies must have the same trigger event
                    return false;
                }
                if (!checkChildren(lDep->dependentEvents(), rDep->dependentEvents(), false)) {
                    // Symmetric dependencies must have the same dependent events
                    return false;
                }
            } else if (dft.isRestriction(indexpair.first)) {
                STORM_LOG_ASSERT(dft.isRestriction(indexpair.second), "Element is no restriction.");
                auto const& lRestr = dft.getRestriction(indexpair.first);
                auto const& rRestr = dft.getRestriction(indexpair.second);
                if (!checkChildren(lRestr->children(), rRestr->children(), lRestr->isSeqEnforcer())) {
                    // Symmetric restrictions must have the same children
                    // Order must be preserved for SEQ but not for MUTEX
                    return false;
                }
            } else {
                STORM_LOG_ASSERT(dft.isBasicElement(indexpair.first), "Element is no BE.");
                STORM_LOG_ASSERT(dft.isBasicElement(indexpair.second), "Element is no BE.");
                // No operations required.
            }
        }
        return true;
    }

   private:
    /*!
     * Check compatibility of colour classes for specific type.
     * @param left Left candidates.
     * @param right Right candidates.
     * @return True iff left and right are compatible.
     */
    template<typename ColourType>
    bool checkCompatibility(std::unordered_map<ColourType, std::vector<size_t>> const& left, std::unordered_map<ColourType, std::vector<size_t>> const& right) {
        if (left.size() != right.size()) {
            // Different number of colour classes
            return false;
        }

        for (auto const& gc : left) {
            auto it = right.find(gc.first);
            if (it == right.end()) {
                // No corresponding colour
                return false;
            } else if (it->second.size() != gc.second.size()) {
                // Corresponding colour classes have different number of elements
                return false;
            }
        }
        return true;
    }

    /*!
     * Check if colour classes are compatible in principle.
     * This checks only excludes non-compatible colour classes. In the positive case, further checks are necessary.
     *
     * @return True iff colours are compatible.
     */
    bool checkCompatibility() {
        if (!checkCompatibility(bleft.gateCandidates, bright.gateCandidates)) {
            return false;
        }
        if (!checkCompatibility(bleft.beCandidates, bright.beCandidates)) {
            return false;
        }
        if (!checkCompatibility(bleft.pdepCandidates, bright.pdepCandidates)) {
            return false;
        }
        if (!checkCompatibility(bleft.restrictionCandidates, bright.restrictionCandidates)) {
            return false;
        }
        return true;
    }

    /**
     *
     */
    template<typename ColourType>
    void initializePermutationsAndTreatTrivialGroups(std::unordered_map<ColourType, std::vector<size_t>> const& left,
                                                     std::unordered_map<ColourType, std::vector<size_t>> const& right,
                                                     std::unordered_map<ColourType, std::vector<size_t>>& permutations) {
        for (auto const& colour : right) {
            if (colour.second.size() > 1) {
                auto it = permutations.insert(colour);
                STORM_LOG_ASSERT(it.second, "Element already contained.");
                std::sort(it.first->second.begin(), it.first->second.end());
                zipVectorsIntoMap(left.at(colour.first), it.first->second, bijection);
            } else {
                STORM_LOG_ASSERT(colour.second.size() == 1, "No elements for colour.");
                STORM_LOG_ASSERT(bijection.count(left.at(colour.first).front()) == 0, "Element already contained.");
                bijection[left.at(colour.first).front()] = colour.second.front();
            }
        }
    }

    /**
     * Local helper function for the creation of bijections, should be hidden from api.
     */
    void zipVectorsIntoMap(std::vector<size_t> const& a, std::vector<size_t> const& b, std::map<size_t, size_t>& map) const {
        // Assert should pass due to compatibility check
        STORM_LOG_ASSERT(a.size() == b.size(), "Sizes do not match.");
        auto it = b.cbegin();
        for (size_t lIndex : a) {
            map[lIndex] = *it;
            ++it;
        }
    }

    bool checkChildren(std::vector<std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>>> const& left,
                       std::vector<std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>>> const& right, bool ensureOrder) const {
        if (ensureOrder) {
            // Observe order of entries
            size_t mappedId;
            size_t rightId;
            for (size_t i = 0; i < left.size(); ++i) {
                if (bleft.has(left[i]->id())) {
                    mappedId = bijection.at(left[i]->id());
                } else {
                    mappedId = -1;
                }
                if (bright.has(right[i]->id())) {
                    rightId = right[i]->id();
                } else {
                    rightId = -1;
                }
                if (mappedId != rightId) {
                    return false;
                }
            }
            return true;
        } else {
            // Order is irrelevant
            std::set<size_t> mappedIds;
            std::set<size_t> rightIds;
            for (auto const& l : left) {
                if (bleft.has(l->id())) {
                    mappedIds.insert(l->id());
                }
            }
            for (auto const& r : right) {
                if (bright.has(r->id())) {
                    rightIds.insert(r->id());
                }
            }
            return mappedIds != rightIds;
        }
    }
};

}  // namespace storage
}  // namespace storm::dft

namespace std {
template<typename ValueType>
struct hash<storm::dft::storage::BEColourClass<ValueType>> {
    size_t operator()(storm::dft::storage::BEColourClass<ValueType> const& bcc) const {
        constexpr uint_fast64_t fivebitmask = (1ul << 6) - 1ul;
        constexpr uint_fast64_t eightbitmask = (1ul << 9) - 1ul;
        constexpr uint_fast64_t fortybitmask = (1ul << 41) - 1ul;
        std::hash<ValueType> hasher;
        uint_fast64_t groupHash = static_cast<uint_fast64_t>(1) << 63;
        groupHash |= (static_cast<uint_fast64_t>(bcc.type) & fivebitmask) << (62 - 5);

        switch (bcc.type) {
            case storm::dft::storage::elements::BEType::CONSTANT:
                groupHash |= (static_cast<uint_fast64_t>(bcc.failed) & fortybitmask) << 8;
                break;
            case storm::dft::storage::elements::BEType::PROBABILITY:
            case storm::dft::storage::elements::BEType::EXPONENTIAL:
            case storm::dft::storage::elements::BEType::LOGNORMAL:
            case storm::dft::storage::elements::BEType::WEIBULL:
                groupHash |= ((hasher(bcc.valueA) ^ hasher(bcc.valueB)) & fortybitmask) << 8;
                break;
            case storm::dft::storage::elements::BEType::ERLANG:
                groupHash |= ((hasher(bcc.valueA) ^ hasher(bcc.valueB) ^ static_cast<uint_fast64_t>(bcc.phases)) & fortybitmask) << 8;
                break;
            case storm::dft::storage::elements::BEType::SAMPLES:
                // Samples have no dedicated hashing
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "BE of type '" << bcc.type << "' is not known.");
        }
        groupHash |= static_cast<uint_fast64_t>(bcc.nrParents) & eightbitmask;
        return groupHash;
    }
};

}  // namespace std

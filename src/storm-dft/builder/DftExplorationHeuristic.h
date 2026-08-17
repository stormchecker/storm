#pragma once

#include <memory>

#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace builder {

/*!
 * Enum representing the heuristic used for deciding which states to expand.
 */
enum class ApproximationHeuristic { DEPTH, PROBABILITY, BOUNDDIFFERENCE };

/*!
 * General super class for approximation heuristics.
 */
template<typename ValueType>
class DFTExplorationHeuristic {
   public:
    explicit DFTExplorationHeuristic(size_t id) : id(id), expand(false) {
        // Intentionally left empty
    }

    virtual ~DFTExplorationHeuristic() = default;

    virtual bool updateHeuristicValues(DFTExplorationHeuristic const& predecessor, ValueType rate, ValueType exitRate) = 0;

    virtual void setBounds(ValueType /*lowerBound*/, ValueType /*upperBound*/) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Should be handled by specialized heuristic.");
    }

    void markExpand() {
        expand = true;
    }

    size_t getId() const {
        return id;
    }

    bool isExpand() const {
        return expand;
    }

    virtual size_t getDepth() const {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Should be handled by specialized heuristic.");
    }

    virtual ValueType getProbability() const {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Should be handled by specialized heuristic.");
    }

    virtual ValueType getLowerBound() const {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Should be handled by specialized heuristic.");
    }

    virtual ValueType getUpperBound() const {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Should be handled by specialized heuristic.");
    }

    virtual double getPriority() const = 0;

    virtual bool isSkip(double approximationThreshold) const {
        return !this->isExpand() && this->getPriority() < approximationThreshold;
    }

    virtual bool operator<(DFTExplorationHeuristic<ValueType> const& other) const {
        return this->getPriority() < other.getPriority();
    }

   protected:
    size_t id;
    bool expand;
};

template<typename ValueType>
class DFTExplorationHeuristicDepth : public DFTExplorationHeuristic<ValueType> {
   public:
    DFTExplorationHeuristicDepth(size_t id) : DFTExplorationHeuristic<ValueType>(id), depth(0) {}

    DFTExplorationHeuristicDepth(size_t id, DFTExplorationHeuristic<ValueType> const& predecessor)
        : DFTExplorationHeuristic<ValueType>(id), depth(predecessor.getDepth() + 1) {}

    bool updateHeuristicValues(DFTExplorationHeuristic<ValueType> const& predecessor, ValueType, ValueType) override {
        if (predecessor.getDepth() + 1 < this->depth) {
            this->depth = predecessor.getDepth() + 1;
            return true;
        }
        return false;
    }

    size_t getDepth() const override {
        return depth;
    }

    double getPriority() const override {
        return this->depth;
    }

    bool isSkip(double approximationThreshold) const override {
        return !this->expand && this->getPriority() > approximationThreshold;
    }

    bool operator<(DFTExplorationHeuristic<ValueType> const& other) const override {
        return this->getPriority() > other.getPriority();
    }

   protected:
    size_t depth;
};

template<typename ValueType>
class DFTExplorationHeuristicProbability : public DFTExplorationHeuristic<ValueType> {
   public:
    DFTExplorationHeuristicProbability(size_t id) : DFTExplorationHeuristic<ValueType>(id), probability(storm::utility::one<ValueType>()) {}

    DFTExplorationHeuristicProbability(size_t id, DFTExplorationHeuristic<ValueType> const& predecessor, ValueType rate, ValueType exitRate)
        : DFTExplorationHeuristic<ValueType>(id), probability(storm::utility::zero<ValueType>()) {
        this->updateHeuristicValues(predecessor, rate, exitRate);
    }

    bool updateHeuristicValues(DFTExplorationHeuristic<ValueType> const& predecessor, ValueType rate, ValueType exitRate) override {
        STORM_LOG_ASSERT(!storm::utility::isZero<ValueType>(exitRate), "Exit rate is 0.");
        probability += predecessor.getProbability() * rate / exitRate;
        return true;
    }

    ValueType getProbability() const override {
        return probability;
    }

    double getPriority() const override;

   protected:
    ValueType probability;
};

template<typename ValueType>
class DFTExplorationHeuristicBoundDifference : public DFTExplorationHeuristicProbability<ValueType> {
   public:
    DFTExplorationHeuristicBoundDifference(size_t id)
        : DFTExplorationHeuristicProbability<ValueType>(id), lowerBound(storm::utility::zero<ValueType>()), upperBound(storm::utility::infinity<ValueType>()) {}

    DFTExplorationHeuristicBoundDifference(size_t id, DFTExplorationHeuristic<ValueType> const& predecessor, ValueType rate, ValueType exitRate)
        : DFTExplorationHeuristicProbability<ValueType>(id, predecessor, rate, exitRate),
          lowerBound(storm::utility::zero<ValueType>()),
          upperBound(storm::utility::infinity<ValueType>()) {}

    void setBounds(ValueType lowerBound, ValueType upperBound) override {
        this->lowerBound = lowerBound;
        this->upperBound = upperBound;
    }

    ValueType getLowerBound() const override {
        return lowerBound;
    }

    ValueType getUpperBound() const override {
        return upperBound;
    }

    double getPriority() const override;

   protected:
    ValueType lowerBound;
    ValueType upperBound;
};

}  // namespace builder
}  // namespace storm::dft

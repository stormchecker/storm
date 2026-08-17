#pragma once

#include <iostream>
#include <map>
#include <unordered_map>

#include "storm-dft/storage/DFTLayoutInfo.h"
#include "storm-dft/storage/elements/DFTElements.h"
#include "storm-dft/storage/elements/DFTRestriction.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/macros.h"

namespace storm::storage {
// Forward declaration
template<typename ValueType>
class DFT;
}  // namespace storm::storage

namespace storm::dft {
namespace builder {

template<typename ValueType>
class DFTBuilder {
    using DFTElementPointer = std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>>;
    using DFTElementCPointer = std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const>;
    using DFTElementVector = std::vector<DFTElementPointer>;
    using DFTBEPointer = std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType>>;
    using DFTChildrenCPointer = std::shared_ptr<storm::dft::storage::elements::DFTChildren<ValueType> const>;
    using DFTGatePointer = std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType>>;
    using DFTDependencyPointer = std::shared_ptr<storm::dft::storage::elements::DFTDependency<ValueType>>;
    using DFTRestrictionPointer = std::shared_ptr<storm::dft::storage::elements::DFTRestriction<ValueType>>;

   private:
    std::size_t mNextId;
    std::string mTopLevelName;
    std::unordered_map<std::string, DFTElementPointer> mElements;
    std::unordered_map<DFTElementPointer, std::vector<std::string>> mChildNames;
    std::unordered_map<DFTRestrictionPointer, std::vector<std::string>> mRestrictionChildNames;
    std::unordered_map<DFTDependencyPointer, std::vector<std::string>> mDependencyChildNames;
    std::unordered_map<std::string, storm::dft::storage::DFTLayoutInfo> mLayoutInfo;

   public:
    /*!
     * Constructor.
     */
    DFTBuilder();

    /*!
     * Create BE which is constant failed or constant failsafe and add it to DFT.
     * @param name Name.
     * @param failed Whether the BE is constant failed.
     */
    void addBasicElementConst(std::string const& name, bool failed);

    /*!
     * Create BE with constant (Bernoulli) distribution and add it to DFT.
     * @param name Name.
     * @param probability (Constant) probability that BE is failed.
     * @param dormancyFactor Dormancy factor in passive mode.
     * @param transient Whether the failure is transient.
     */
    void addBasicElementProbability(std::string const& name, ValueType probability, ValueType dormancyFactor);

    /*!
     * Create BE with exponential distribution and add it to DFT.
     * @param name Name.
     * @param rate Failure rate governing failure distribution of BE.
     * @param dormancyFactor Dormancy factor in passive mode.
     * @param transient Whether the failure is transient.
     */
    void addBasicElementExponential(std::string const& name, ValueType rate, ValueType dormancyFactor, bool transient = false);

    /*!
     * Create BE with Erlang distribution and add it to DFT.
     * @param name Name.
     * @param rate Failure rate governing failure distribution in each phase.
     * @param phases Number of consecutive phases.
     * @param dormancyFactor Dormancy factor in passive mode.
     */
    void addBasicElementErlang(std::string const& name, ValueType rate, unsigned phases, ValueType dormancyFactor);

    /*!
     * Create BE with Weibull distribution and add it to DFT.
     * @param name Name.
     * @param shape Shape parameter.
     * @param rate Failure rate governing failure distribution (also called scale).
     */
    void addBasicElementWeibull(std::string const& name, ValueType shape, ValueType rate);

    /*!
     * Create BE with log-normal distribution and add it to DFT.
     * @param name Name.
     *  @param mean Mean value (also called expected value).
     * @param standardDeviation Standard deviation.
     */
    void addBasicElementLogNormal(std::string const& name, ValueType mean, ValueType standardDeviation);

    /*!
     * Create BE with distribution given by sample points and add it to DFT.
     * @param name Name.
     * @param activeSamples Sample points defining the unreliability at certain time points (i.e. the cumulative distribution function F(x)).
     */
    void addBasicElementSamples(std::string const& name, std::map<ValueType, ValueType> const& activeSamples);

    /*!
     * Create AND-gate and add it to DFT.
     * @param name Name.
     * @param children Names of children.
     */
    void addAndGate(std::string const& name, std::vector<std::string> const& children);

    /*!
     * Create OR-gate and add it to DFT.
     * @param name Name.
     * @param children Names of children.
     */
    void addOrGate(std::string const& name, std::vector<std::string> const& children);

    /*!
     * Create VOTing-gate and add it to DFT.
     * @param name Name.
     * @param threshold Threshold specifying how many children need to fail.
     * @param children Names of children.
     */
    void addVotingGate(std::string const& name, unsigned threshold, std::vector<std::string> const& children);

    /*!
     * Create PAND-gate and add it to DFT.
     * @param name Name.
     * @param children Names of children.
     * @param inclusive Whether the failure behaviour is inclusive (simultaneous failures are allowed) or not.
     */
    void addPandGate(std::string const& name, std::vector<std::string> const& children, bool inclusive = true);

    /*!
     * Create POR-gate and add it to DFT.
     * @param name Name.
     * @param children Names of children.
     * @param inclusive Whether the failure behaviour is inclusive (simultaneous failures are allowed) or not.
     */
    void addPorGate(std::string const& name, std::vector<std::string> const& children, bool inclusive = true);

    /*!
     * Create SPARE-gate and add it to DFT.
     * @param name Name.
     * @param children Names of children.
     */
    void addSpareGate(std::string const& name, std::vector<std::string> const& children);

    /*!
     * Create sequence enforcer (SEQ) and add it to DFT.
     * @param name Name.
     * @param children Names of children.
     */
    void addSequenceEnforcer(std::string const& name, std::vector<std::string> const& children);

    /*!
     * Create mutual exclusion-gate (MUTEX) and add it to DFT.
     * @param name Name.
     * @param children Names of children.
     */
    void addMutex(std::string const& name, std::vector<std::string> const& children);

    /*!
     * Create (probabilistic) dependency (PDEP) and add it to DFT.
     * @param name Name.
     * @param children Names of children. First child is the trigger event, all remaining children are the dependent events.
     * @param probability Probability of forwarding the failure. Probability 1 corresponds to an FDEP.
     */
    void addPdep(std::string const& name, std::vector<std::string> const& children, ValueType probability);

    /*!
     * Set top level element.
     * @param tle Name of top level element.
     */
    void setTopLevel(std::string const& tle);

    /*!
     * Add layout information for DFT element.
     * @param name Name of DFT element.
     * @param x X coordinate.
     * @param y Y coordinate.
     */
    void addLayoutInfo(std::string const& name, double x, double y);

    /*!
     * Create DFT.
     * The elements of the DFT must be specified before via the addX() methods and the top level element must be set via setTopLevel().
     * @return DFT.
     */
    storm::dft::storage::DFT<ValueType> build();

    /**
     * Clone element and add it via the builder.
     *
     * @param element Element to clone.
     */
    void cloneElement(DFTElementCPointer element);

    /**
     * Clone element, replace its children with the given children and add it via the builder.
     *
     * @param element Element to clone.
     * @param children New children of cloned gate.
     */
    void cloneElementWithNewChildren(DFTChildrenCPointer elemWithChildren, std::vector<std::string> const& children);

   private:
    /*!
     * Add given element to DFT.
     * @param element DFT element.
     */
    void addElement(DFTElementPointer element);

    /*!
     * Add given gate to DFT.
     * @param gate Gate.
     * @param children Names of children.
     */
    void addGate(DFTGatePointer gate, std::vector<std::string> const& children);

    /*!
     * Add given dependency to DFT.
     * @param dependency Dependency.
     * @parama children Names of children.
     */
    void addDependency(DFTDependencyPointer dependency, std::vector<std::string> const& children);

    /*!
     * Add given restriction to DFT.
     * @param restriction Restriction.
     * @param children Names of children.
     */
    void addRestriction(DFTRestrictionPointer restriction, std::vector<std::string> const& children);

    /**
     * Check whether the name is already used.
     * @param name Element name.
     * @return True iff name is already in use.
     */
    bool nameInUse(std::string const& name) const;

    /**
     * Check whether the given value is a probability and satisfies 0 <= value <= 1.
     * @param value Value.
     * @return True iff 0 <= value <= 1.
     */
    bool isValidProbability(ValueType value) const;

    /*!
     * Colouring used for topological sorting.
     * WHITE: not yet visited
     * BLACK: finished visit
     * GREY:  currently visiting
     */
    enum class topoSortColour { WHITE, BLACK, GREY };

    /*!
     * Visit elements in depth-first order.
     * @param element Current element.
     * @param visited Map storing which elements have been visited.
     * @param visitedElements List of elements sorted by the order in which they were visited. Acts as return type.
     */
    void topologicalVisit(DFTElementPointer const& element,
                          std::map<DFTElementPointer, topoSortColour, storm::dft::storage::OrderElementsById<ValueType>>& visited,
                          DFTElementVector& visitedElements);

    /*!
     * Return list of elements sorted topologically (depth-first).
     * @return Ordered list of elements.
     */
    DFTElementVector sortTopological();

    /*!
     * Compute rank (i.e., height in tree) for given element.
     * BEs/dependencies/restriction have rank 0.
     * Gates have maximal rank of their children + 1.
     * @param elem DFT element.
     * @return Rank.
     */
    size_t computeRank(DFTElementPointer const& elem);
};

}  // namespace builder
}  // namespace storm::dft

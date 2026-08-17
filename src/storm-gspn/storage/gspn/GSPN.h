#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <vector>

#include "storm/storage/expressions/ExpressionManager.h"

#include "storm-gspn/storage/gspn/ImmediateTransition.h"
#include "storm-gspn/storage/gspn/Marking.h"
#include "storm-gspn/storage/gspn/Place.h"
#include "storm-gspn/storage/gspn/PlacementInfo.h"
#include "storm-gspn/storage/gspn/TimedTransition.h"
#include "storm-gspn/storage/gspn/TransitionPartition.h"

namespace storm {
namespace gspn {
// Stores a GSPN
class GSPN {
   public:
    static uint64_t timedTransitionIdToTransitionId(uint64_t);
    static uint64_t immediateTransitionIdToTransitionId(uint64_t);
    static uint64_t transitionIdToTimedTransitionId(uint64_t);
    static uint64_t transitionIdToImmediateTransitionId(uint64_t);

    // Later, the rates and probabilities type should become a template, for now, let it be doubles.
    typedef double RateType;
    typedef double WeightType;

    GSPN(std::string const& name, std::vector<Place> const& places, std::vector<ImmediateTransition<WeightType>> const& itransitions,
         std::vector<TimedTransition<RateType>> const& ttransitions, std::vector<TransitionPartition> const& partitions,
         std::shared_ptr<storm::expressions::ExpressionManager> const& exprManager,
         std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantsSubstitution =
             std::map<storm::expressions::Variable, storm::expressions::Expression>());

    /*!
     * Returns the number of places in this gspn.
     *
     * @return The number of places.
     */
    uint64_t getNumberOfPlaces() const;

    uint64_t getNumberOfImmediateTransitions() const;

    uint64_t getNumberOfTimedTransitions() const;

    /*!
     *
     */
    std::vector<TransitionPartition> const& getPartitions() const;

    /*!
     * Returns the vector of timed transitions in this gspn.
     *
     * @return The vector of timed transitions.
     */
    std::vector<TimedTransition<GSPN::RateType>> const& getTimedTransitions() const;

    /*!
     * Returns the vector of immediate transitions in this gspn.
     *
     * @return The vector of immediate tansitions.
     */
    std::vector<ImmediateTransition<GSPN::WeightType>> const& getImmediateTransitions() const;

    /*!
     * Returns the places of this gspn
     */
    std::vector<storm::gspn::Place> const& getPlaces() const;

    /*
     * Computes the initial marking of the gspn.
     *
     * @param map The Map determines the number of bits for each place.
     * @return The initial Marking
     */
    std::shared_ptr<storm::gspn::Marking> getInitialMarking(std::map<uint64_t, uint64_t>& numberOfBits, uint64_t const& numberOfTotalBits) const;

    /*!
     * Returns the place with the corresponding id.
     *
     * @param id The id of the place.
     * @return A pointer to the place with the given id, and nullptr otherwise
     */
    storm::gspn::Place const* getPlace(uint64_t id) const;

    /*!
     * Returns the place with the corresponding name.
     *
     * @param name The name of the place.
     * @return A pointer to the place with the given name, and nullptr otherwise
     */
    storm::gspn::Place const* getPlace(std::string const& name) const;

    /*!
     * Returns the timed transition with the corresponding name.
     *
     * @param name The ID of the timed transition.
     * @return A pointer to the transition, and nullptr otherwise
     */
    storm::gspn::TimedTransition<GSPN::RateType> const* getTimedTransition(std::string const& name) const;

    /*!
     * Returns the immediate transition with the corresponding name.
     *
     * @param name The name of the timed transition.
     * @return A pointer to the transition, and nullptr otherwise
     */
    storm::gspn::ImmediateTransition<GSPN::WeightType> const* getImmediateTransition(std::string const& name) const;

    /*!
     * Returns the transition with the corresponding name
     *
     * @param name The name of the timed transition
     * @return A pointer to the transition, and nullptr otherwise
     */
    storm::gspn::Transition const* getTransition(std::string const& name) const;

    /*!
     * Write the gspn in a dot(graphviz) configuration.
     *
     * @param outStream The stream to which the output is written to.
     */
    void writeDotToStream(std::ostream& outStream) const;

    /*!
     * Set the name of the gspn to the given name.
     *
     * @param name The new name.
     */
    void setName(std::string const& name);

    /*!
     * Returns the name of the gspn.
     *
     * @return The name.
     */
    std::string const& getName() const;

    /*!
     * Obtain the expression manager used for expressions over GSPNs.
     *
     * @return
     */
    std::shared_ptr<storm::expressions::ExpressionManager> const& getExpressionManager() const;

    /*!
     * Gets an assignment of occurring constants of the GSPN to their value
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> const& getConstantsSubstitution() const;

    /**
     *  Set Capacities of places according to name->capacity map.
     */
    void setCapacities(std::unordered_map<std::string, uint64_t> const& mapping);

    void setPlaceLayoutInfo(uint64_t placeId, LayoutInfo const& layout) const;
    void setTransitionLayoutInfo(uint64_t transitionId, LayoutInfo const& layout) const;
    void setPlaceLayoutInfo(std::map<uint64_t, LayoutInfo> const& placeLayout) const;
    void setTransitionLayoutInfo(std::map<uint64_t, LayoutInfo> const& transitionLayout) const;

    std::map<uint64_t, LayoutInfo> const& getPlaceLayoutInfos() const;

    std::map<uint64_t, LayoutInfo> const& getTransitionLayoutInfos() const;

    /*!
     * Performe some checks
     * - testPlaces()
     * - testTransitions()
     *
     * @return true if no errors are found
     */
    bool isValid() const;
    // TODO doc
    void toPnpro(std::ostream& stream) const;
    // TODO doc
    void toPnml(std::ostream& stream) const;

    /*!
     * Export GSPN in Json format.
     *
     * @param stream Outputstream.
     */
    void toJson(std::ostream& stream) const;

    void writeStatsToStream(std::ostream& stream) const;

   private:
    storm::gspn::Place* getPlace(uint64_t id);
    storm::gspn::Place* getPlace(std::string const& name);

    /*!
     * Test
     *  - if places are unique (ids and names)
     *  - if the capacity is greater than the number of initial tokens
     *
     * @return true if no errors found
     */
    bool testPlaces() const;

    /*!
     * Test
     * - if transition have at least on input/inhibitor and one output place
     *
     * @return true if no errors found
     */
    bool testTransitions() const;

    // name of the gspn
    std::string name;

    // set containing all places
    std::vector<storm::gspn::Place> places;

    // set containing all immediate transitions
    std::vector<storm::gspn::ImmediateTransition<WeightType>> immediateTransitions;

    // set containing all timed transitions
    std::vector<storm::gspn::TimedTransition<RateType>> timedTransitions;

    std::vector<storm::gspn::TransitionPartition> partitions;

    std::shared_ptr<storm::expressions::ExpressionManager> exprManager;

    std::map<storm::expressions::Variable, storm::expressions::Expression> constantsSubstitution;

    // Layout information
    mutable std::map<uint64_t, LayoutInfo> placeLayout;
    mutable std::map<uint64_t, LayoutInfo> transitionLayout;
};
}  // namespace gspn
}  // namespace storm

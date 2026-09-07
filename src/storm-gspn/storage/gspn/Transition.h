#pragma once

#include <map>
#include <unordered_map>
#include <vector>

#include "storm-gspn/storage/gspn/Marking.h"
#include "storm-gspn/storage/gspn/Place.h"

namespace storm {
namespace gspn {
/*!
 * This class represents a transition in a gspn.
 */
class Transition {
   public:
    /*!
     * Set the multiplicity of the input arc originating from the place.
     * If the arc already exists, the former multiplicity is overwritten.
     * If the arc does not yet exists, it is created.
     *
     * @param place The place connected by an input arc.
     * @param multiplicity The multiplicity of the specified arc.
     */
    void setInputArcMultiplicity(storm::gspn::Place const& place, uint64_t multiplicity);

    /*!
     * Removes an input arc connected to a given place.
     *
     * @param place The place from which the input arc is originating.
     * @return True if the arc existed.
     */
    bool removeInputArc(storm::gspn::Place const& place);

    /*!
     * Checks whether the given place is connected to this transition via an input arc.
     *
     * @param place The place which is going to be checked.
     * @return True if the place is connected via an input arc.
     */
    bool existsInputArc(storm::gspn::Place const& place) const;

    /*!
     * Set the multiplicity of the output arc going to the place.
     * If the arc already exists, the former multiplicity is overwritten.
     * If the arc does not yet exists, it is created.
     *
     * @param place The place connected by an output arc.
     * @param multiplicity The multiplicity of the specified arc.
     */
    void setOutputArcMultiplicity(storm::gspn::Place const& place, uint64_t multiplicity);

    /*!
     * Removes an output arc connected to a given place.
     *
     * @param place The place from which the output arc is leading to.
     * @return True if the arc existed.
     */
    bool removeOutputArc(storm::gspn::Place const& place);

    /*!
     * Checks whether the given place is connected to this transition via an output arc.
     *
     * @param place The place which is going to be checked.
     * @return True if the place is connected via an output arc.
     */
    bool existsOutputArc(storm::gspn::Place const& place) const;

    /*!
     * Set the multiplicity of the inhibition arc originating from the place.
     * If the arc already exists, the former multiplicity is overwritten.
     * If the arc does not yet exists, it is created.
     *
     * @param place The place connected by an inhibition arc.
     * @param multiplicity The multiplicity of the specified arc.
     */
    void setInhibitionArcMultiplicity(storm::gspn::Place const& place, uint64_t multiplicity);

    /*!
     * Removes an inhibition arc connected to a given place.
     *
     * @param place The place from which the inhibition arc is originating.
     * @return True if the arc existed.
     */
    bool removeInhibitionArc(storm::gspn::Place const& place);

    /*!
     * Checks whether the given place is connected to this transition via an inhibition arc.
     *
     * @param place The place which is going to be checked.
     * @return True if the place is connected via an inhibition arc.
     */
    bool existsInhibitionArc(storm::gspn::Place const& place) const;

    /*!
     * Checks if the given marking enables the transition.
     *
     * @return True if the transition is enabled.
     */
    bool isEnabled(storm::gspn::Marking const& marking) const;

    /*!
     * Fire the transition if possible.
     *
     * @param marking The current marking before the transition is fired.
     * @return The marking after the transition was fired.
     */
    storm::gspn::Marking fire(storm::gspn::Marking const& marking) const;

    /*!
     * Set the name of the transition.
     *
     * @param name New name of the transition.
     */
    void setName(std::string const& name);

    /*!
     * Returns the name of the transition.
     *
     * @return The name of the transition.
     */
    std::string const& getName() const;

    std::unordered_map<uint64_t, uint64_t> const& getInputPlaces() const;

    std::unordered_map<uint64_t, uint64_t> const& getOutputPlaces() const;

    std::unordered_map<uint64_t, uint64_t> const& getInhibitionPlaces() const;

    /*!
     * Returns the corresponding multiplicity.
     *
     * @param place connected to this transition by an input arc
     * @return cardinality or 0 if the arc does not exists
     */
    uint64_t getInputArcMultiplicity(storm::gspn::Place const& place) const;

    /*!
     * Returns the corresponding multiplicity.
     *
     * @param place connected to this transition by an inhibition arc
     * @return cardinality or 0 if the arc does not exists
     */
    uint64_t getInhibitionArcMultiplicity(storm::gspn::Place const& place) const;

    /*!
     * Returns the corresponding multiplicity.
     *
     * @param place connected to this transition by an output arc
     * @return cardinality or 0 if the arc does not exists
     */
    uint64_t getOutputArcMultiplicity(storm::gspn::Place const& place) const;

    /*!
     * Sets the priority of this transtion.
     *
     * @param priority The new priority.
     */
    void setPriority(uint64_t const& priority);

    /*!
     * Returns the priority of this transition.
     *
     * @return The priority.
     */
    uint64_t getPriority() const;

    void setID(uint64_t const& id) {
        this->id = id;
    }

    uint64_t getID() const {
        return id;
    }

   private:
    // maps place ids connected to this transition with an input arc to the corresponding multiplicity
    std::unordered_map<uint64_t, uint64_t> inputMultiplicities;

    // maps place ids connected to this transition with an output arc to the corresponding multiplicities
    std::unordered_map<uint64_t, uint64_t> outputMultiplicities;

    // maps place ids connected to this transition with an inhibition arc to the corresponding multiplicity
    std::unordered_map<uint64_t, uint64_t> inhibitionMultiplicities;

    // name of the transition
    std::string name;

    // priority of this transition
    uint64_t priority = 0;

    uint64_t id;
};
}  // namespace gspn
}  // namespace storm

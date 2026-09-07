#pragma once

#include <map>
#include <string>
#include <vector>

#include "storm-gspn/storage/gspn/GSPN.h"

namespace storm {
namespace gspn {
class GspnBuilder {
   public:
    typedef double RateType;
    typedef double WeightType;

    /**
     * Set GSPN name
     */
    void setGspnName(std::string const& name);

    /**
     * Add a place to the gspn.
     * @param name The name must be unique for the gspn.
     * @param capacity The capacity is the limit of tokens in the place.
     *                 A capacity of -1 indicates an unbounded place.
     * @param initialTokens The number of inital tokens in the place.
     */
    uint_fast64_t addPlace(boost::optional<uint64_t> const& capacity = 1, uint_fast64_t const& initialTokens = 0, std::string const& name = "");

    void setPlaceLayoutInfo(uint64_t placeId, LayoutInfo const& layoutInfo);

    /**
     * Adds an immediate transition to the gspn.
     * @param priority The priority for the transtion.
     * @param weight The weight for the transition.
     */
    uint_fast64_t addImmediateTransition(uint_fast64_t const& priority = 0, WeightType const& weight = 0, std::string const& name = "");

    /**
     * Adds an timed transition to the gspn.
     * The transition is assumed to have Single-Server-Semantics
     * @param priority The priority for the transtion.
     * @param rate The rate for the transition.
     */
    uint_fast64_t addTimedTransition(uint_fast64_t const& priority, RateType const& rate, std::string const& name = "");

    /**
     * Adds an timed transition to the gspn.
     * @param priority The priority for the transtion.
     * @param rate The rate for the transition.
     * @param numServers The number of servers this transition has (in case of K-Server semantics) or boost::none (in case of Infinite-Server-Semantics).
     */
    uint_fast64_t addTimedTransition(uint_fast64_t const& priority, RateType const& rate, boost::optional<uint64_t> const& numServers,
                                     std::string const& name = "");

    void setTransitionLayoutInfo(uint64_t transitionId, LayoutInfo const& layoutInfo);

    /**
     * Adds an new input arc from a place to an transition.
     * @param from The place from which the arc is originating.
     * @param to The transtion to which the arc goes to.
     * @param multiplicity The multiplicity of the arc.
     */
    void addInputArc(uint_fast64_t const& from, uint_fast64_t const& to, uint_fast64_t const& multiplicity = 1);

    void addInputArc(std::string const& from, std::string const& to, uint64_t multiplicity = 1);
    /**
     * Adds an new input arc from a place to an transition.
     * @param from The place from which the arc is originating.
     * @param to The transtion to which the arc goes to.
     * @param multiplicity The multiplicity of the arc.
     */
    void addInhibitionArc(uint_fast64_t const& from, uint_fast64_t const& to, uint_fast64_t const& multiplicity = 1);

    void addInhibitionArc(std::string const& from, std::string const& to, uint64_t multiplicity = 1);

    /**
     * Adds an new input arc from a place to an transition.
     * @param from The place from which the arc is originating.
     * @param to The transtion to which the arc goes to.
     * @param multiplicity The multiplicity of the arc.
     */
    void addOutputArc(uint_fast64_t const& from, uint_fast64_t const& to, uint_fast64_t const& multiplicity = 1);

    void addOutputArc(std::string const& from, std::string const& to, uint64_t multiplicity = 1);

    /**
     * Adds an arc from a  named element to a named element.
     * Can be both input or output arc, but not an inhibition arc.
     * Convenience function for textual format parsers.
     *
     * @param from Source element in the GSPN from where this arc starts
     * @param to Target element in the GSPN where this arc ends
     * @param multiplicity (Optional) multiplicity for the arc, default = 1
     */
    void addNormalArc(std::string const& from, std::string const& to, uint64_t multiplicity = 1);

    /**
     * @param exprManager The expression manager that will be associated with the new gspn. If this is nullptr, a new expressionmanager will be created.
     * @return The gspn which is constructed by the builder.
     */
    storm::gspn::GSPN* buildGspn(std::shared_ptr<storm::expressions::ExpressionManager> const& exprManager = nullptr,
                                 std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantsSubstitution =
                                     std::map<storm::expressions::Variable, storm::expressions::Expression>()) const;

   private:
    bool isImmediateTransitionId(uint64_t) const;
    bool isTimedTransitionId(uint64_t) const;
    Transition& getTransition(uint64_t);

    std::map<std::string, uint64_t> placeNames;
    std::map<std::string, uint64_t> transitionNames;

    std::string gspnName = "_gspn_";

    std::map<uint64_t, std::vector<storm::gspn::TransitionPartition>> partitions;

    // set containing all immediate transitions
    std::vector<storm::gspn::ImmediateTransition<WeightType>> immediateTransitions;

    // set containing all timed transitions
    std::vector<storm::gspn::TimedTransition<RateType>> timedTransitions;

    // set containing all places
    std::vector<storm::gspn::Place> places;

    std::map<uint64_t, LayoutInfo> placeLayout;
    std::map<uint64_t, LayoutInfo> transitionLayout;
};
}  // namespace gspn
}  // namespace storm

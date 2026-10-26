#pragma once

#include <boost/any.hpp>

#include "storm/languages/jani/traverser/JaniTraverser.h"

namespace storm {

namespace expressions {
class Variable;
class Expression;
}  // namespace expressions

namespace jani {

class Model;

class RewardModelInformation : public ConstJaniTraverser {
   public:
    RewardModelInformation(bool hasStateRewards, bool hasActionRewards, bool hasTransitionRewards);
    RewardModelInformation(storm::jani::Model const& janiModel, std::string const& rewardModelNameIdentifier);
    RewardModelInformation(storm::jani::Model const& janiModel, storm::expressions::Expression const& rewardModelExpression);

    virtual ~RewardModelInformation() = default;
    using ConstJaniTraverser::traverse;

    virtual void traverse(Location const& location, boost::any const& data) override;
    virtual void traverse(TemplateEdge const& templateEdge, boost::any const& data) override;
    virtual void traverse(TemplateEdgeDestination const& TemplateEdgeDestination, boost::any const& data) override;

    /*!
     * Returns the resulting information when joining the two reward models
     */
    RewardModelInformation join(RewardModelInformation const& other) const;

    /*!
     * Returns true iff the given reward model has state rewards
     */
    bool hasStateRewards() const;

    /*!
     * Returns true iff the given reward model has action rewards
     */
    bool hasActionRewards() const;

    /*!
     * Returns true iff the given reward model has transition rewards
     */
    bool hasTransitionRewards() const;

    bool stateRewards;
    bool actionRewards;
    bool transitionRewards;
};
}  // namespace jani
}  // namespace storm

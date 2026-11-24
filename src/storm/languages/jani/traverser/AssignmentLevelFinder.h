#pragma once

#include "storm/languages/jani/traverser/JaniTraverser.h"

namespace storm {
namespace jani {
class AssignmentLevelFinder : public ConstJaniTraverser {
   public:
    AssignmentLevelFinder() = default;
    virtual ~AssignmentLevelFinder() = default;

    int64_t getLowestAssignmentLevel(Model const& model);

    virtual void traverse(Assignment const& assignment, boost::any const& data) override;
};
}  // namespace jani
}  // namespace storm

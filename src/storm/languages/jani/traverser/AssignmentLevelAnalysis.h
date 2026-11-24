#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>

#include "storm/expressions/Expression.h"
#include "storm/expressions/Variable.h"

namespace storm {
namespace jani {

class Model;

/*!
 * The assignment levels of a JANI model as needed by the symbolic builder.
 *
 * The symbolic builder requires every variable to be assigned at no more than one level. The value of a variable x
 * that is read at level k is then statically known: it is the successor value of x if x is assigned at a level below
 * k, and otherwise the predecessor value (or, for a transient variable, its initial value).
 *
 * A transient variable whose value is read at a level above the one where it is written is called "passed". The
 * builder treats it like a global variable that is only ever read in its successor version: when it is not written
 * in a step, it takes its initial value (instead of keeping its predecessor value), and after composition its
 * successor version is summed out.
 */
struct AssignmentLevels {
    // All levels at which edge (destination) assignments occur.
    std::set<int64_t> levels;
    // The (unique) level at which each variable is assigned on edges.
    std::map<storm::expressions::Variable, int64_t> writeLevel;
    // The initial values of all transient variables (with defined constants substituted).
    std::map<storm::expressions::Variable, storm::expressions::Expression> transientInitialValues;
    // The passed transient variables (see above).
    std::set<storm::expressions::Variable> passedTransientVariables;
    // If non-empty, the reason why the symbolic builder can not handle the model's assignment levels.
    std::string error;

    int64_t lowestLevel() const;

    bool isPassed(storm::expressions::Variable const& variable) const;

    /*!
     * Retrieves whether the given variable is read in its successor version when read at the given level.
     */
    bool isReadAsSuccessor(storm::expressions::Variable const& variable, int64_t level) const;
};

/*!
 * Analyzes the assignment levels of the given model (see AssignmentLevels). If the symbolic builder can not handle
 * them, the error of the result is set.
 */
AssignmentLevels analyzeAssignmentLevels(Model const& model);

}  // namespace jani
}  // namespace storm

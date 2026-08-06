#pragma once

#include <map>
#include <string>
#include <vector>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/prism/Update.h"

namespace storm {
namespace prism {
class Command : public LocatedInformation {
   public:
    /*!
     * Creates a command with the given action name, guard and updates.
     *
     * @param globalIndex The global index of the command.
     * @param markovian A flag indicating whether the command's update probabilities are to be interpreted as
     * rates in a continuous-time model.
     * @param actionIndex The index of the action of the command.
     * @param actionName The action name of the command.
     * @param guardExpression the expression that defines the guard of the command.
     * @param updates A list of updates that is associated with this command.
     * @param filename The filename in which the command is defined.
     * @param lineNumber The line number in which the command is defined.
     */
    Command(uint_fast64_t globalIndex, bool markovian, uint_fast64_t actionIndex, std::string const& actionName,
            storm::expressions::Expression const& guardExpression, std::vector<storm::prism::Update> const& updates, std::string const& filename = "",
            uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    Command() = default;
    Command(Command const& other) = default;
    Command& operator=(Command const& other) = default;
    Command(Command&& other) = default;
    Command& operator=(Command&& other) = default;

    /*!
     * Retrieves the action name of this command.
     *
     * @return The action name of this command.
     */
    std::string const& getActionName() const;

    /*!
     * Retrieves the action index of this command.
     *
     * @return The action index of the command.
     */
    uint_fast64_t getActionIndex() const;

    /*!
     * Retrieves whether the command is a Markovian command, i.e. it's update likelihoods are to be interpreted
     * as rates in a continuous-time model.
     *
     * @return True iff the command is Markovian.
     */
    bool isMarkovian() const;

    /*!
     * Sets whether this command is a Markovian command, i.e. it's update likelihoods are to be interpreted as
     * rates in a continuous-time model.
     *
     * @param value The command is flagged as Markovian iff this flag is set.
     */
    void setMarkovian(bool value);

    /*!
     * Retrieves a reference to the guard of the command.
     *
     * @return A reference to the guard of the command.
     */
    storm::expressions::Expression const& getGuardExpression() const;

    /*!
     * Retrieves the number of updates associated with this command.
     *
     * @return The number of updates associated with this command.
     */
    std::size_t getNumberOfUpdates() const;

    /*!
     * Retrieves a reference to the update with the given index.
     *
     * @return A reference to the update with the given index.
     */
    storm::prism::Update const& getUpdate(uint_fast64_t index) const;

    /*!
     * Retrieves a vector of all updates associated with this command.
     *
     * @return A vector of updates associated with this command.
     */
    std::vector<storm::prism::Update> const& getUpdates() const;

    /*!
     * Retrieves a vector of all updates associated with this command.
     *
     * @return A vector of updates associated with this command.
     */
    std::vector<storm::prism::Update>& getUpdates();

    /*!
     * Retrieves the global index of the command, that is, a unique index over all modules.
     *
     * @return The global index of the command.
     */
    uint_fast64_t getGlobalIndex() const;

    /*!
     * Substitutes all identifiers in the command according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting command.
     */
    Command substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    Command substituteNonStandardPredicates() const;
    /*!
     * Retrieves whether the command possesses a synchronization label.
     *
     * @return True iff the command is labeled.
     */
    bool isLabeled() const;

    /*!
     * Checks whether the given set of variables only appears in the update probabilities of the command.
     *
     * @param undefinedConstantVariables The set of variables that may only appear in the update probabilities
     * of the command.
     * @return True iff the given set of variables only appears in the update probabilities of the command.
     */
    bool containsVariablesOnlyInUpdateProbabilities(std::set<storm::expressions::Variable> const& undefinedConstantVariables) const;

    friend std::ostream& operator<<(std::ostream& stream, Command const& command);

    /*!
     * Simplifies this command.
     */
    Command simplify() const;

   private:
    //  The index of the action associated with this command.
    uint_fast64_t actionIndex;

    // A flag indicating whether the likelihoods attached to the updates are to be interpreted as rates rather
    // than probabilities.
    bool markovian;

    // The name of the command.
    std::string actionName;

    // The expression that defines the guard of the command.
    storm::expressions::Expression guardExpression;

    // The list of updates of the command.
    std::vector<storm::prism::Update> updates;

    // The global index of the command.
    uint_fast64_t globalIndex;

    // A flag indicating whether the command is labeled.
    bool labeled;

    Command copyWithNewUpdates(std::vector<Update>&& newUpdates) const;
};

}  // namespace prism
}  // namespace storm

#pragma once

#include <cstdint>

#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/storage/expressions/SimpleValuation.h"

#include <functional>
#include <initializer_list>
#include <set>
#include <unordered_set>
#include <vector>

namespace storm {
namespace solver {

/*!
 * An interface that captures the functionality of an SMT solver.
 */
class SmtSolver {
   public:
    //! possible check results
    enum class CheckResult { Sat, Unsat, Unknown };

    /*!
     * The base class for all model references. They are used to provide a lightweight method of accessing the
     * models the solver generates (that is, without constructing other objects).
     */
    class ModelReference {
       public:
        /*!
         * Creates a model reference that uses the given expression manager.
         *
         * @param manager The manager responsible for the variables whose value can be requested.
         */
        ModelReference(storm::expressions::ExpressionManager const& manager);
        virtual ~ModelReference() = default;

        virtual bool getBooleanValue(storm::expressions::Variable const& variable) const = 0;
        virtual int_fast64_t getIntegerValue(storm::expressions::Variable const& variable) const = 0;
        virtual double getRationalValue(storm::expressions::Variable const& variable) const = 0;

        /*!
         * Retrieves the expression manager associated with this model reference.
         *
         * @return The expression manager associated with this model reference.
         */
        storm::expressions::ExpressionManager const& getManager() const;

        virtual std::string toString() const = 0;

       private:
        // The expression manager responsible for the variables whose value can be requested via this model
        // reference.
        storm::expressions::ExpressionManager const& manager;
    };

   public:
    /*!
     * Constructs a new Smt solver with the given options.
     *
     * @param manager The expression manager responsible for all expressions that in some way or another interact
     * with this solver.
     * @throws storm::exceptions::IllegalArgumentValueException if an option is unsupported for the solver.
     */
    SmtSolver(storm::expressions::ExpressionManager& manager);

    /*!
     * Destructs the solver instance
     */
    virtual ~SmtSolver();

    SmtSolver(SmtSolver const& other) = default;

    SmtSolver(SmtSolver&& other) = default;
    SmtSolver& operator=(SmtSolver const& other) = delete;
    SmtSolver& operator=(SmtSolver&& other) = delete;

    /*!
     * Pushes a backtracking point on the solver's stack. A following call to pop() deletes exactly those
     * assertions from the solver's stack that were added after this call.
     */
    virtual void push() = 0;

    /*!
     * Pops a backtracking point from the solver's stack. This deletes all assertions from the solver's stack
     * that were added after the last call to push().
     */
    virtual void pop() = 0;

    /*!
     * Pops multiple backtracking points from the solver's stack in the same way as pop() does.
     *
     * @param n The number of backtracking points to pop.
     */
    virtual void pop(uint_fast64_t n);

    /*!
     * Removes all assertions from the solver's stack.
     */
    virtual void reset() = 0;

    /*!
     * Adds an assertion to the solver's stack.
     *
     * @param assertion The assertion to add.
     */
    virtual void add(storm::expressions::Expression const& assertion) = 0;

    /*!
     * Adds the given set of assertions to the solver's stack.
     *
     * @param assertions The assertions to add.
     */
    void add(std::set<storm::expressions::Expression> const& assertions);

    /*!
     * Adds the given list of assertions to the solver's stack.
     *
     * @param assertions The assertions to add.
     */
    void add(std::initializer_list<storm::expressions::Expression> const& assertions);

    /*!
     * If supported by the solver, this function tells the SMT solver to produce a model different from the current model.
     */
    virtual void addNotCurrentModel(bool performSolverReset = true);

    /*!
     * Checks whether the conjunction of assertions that are currently on the solver's stack is satisfiable.
     *
     * @return Sat if the conjunction of the asserted expressions is satisfiable, Unsat if it is unsatisfiable
     * and Unknown if the solver could not determine satisfiability.
     */
    virtual CheckResult check() = 0;

    /*!
     * Checks whether the conjunction of assertions that are currently on the solver's stack together with the
     * provided assumptions is satisfiable. The assumptions are, however, not added to the solver's stack, but
     * are merely considered for this one call.
     *
     * @param assumptions The assumptions to add to the call.
     * @return Sat if the conjunction of the asserted expressions together with the provided assumptions is
     * satisfiable, Unsat if it is unsatisfiable and Unknown if the solver could not determine satisfiability.
     */
    virtual CheckResult checkWithAssumptions(std::set<storm::expressions::Expression> const& assumptions) = 0;

    /*!
     * Checks whether the conjunction of assertions that are currently on the solver's stack together with the
     * provided assumptions is satisfiable. The assumptions are, however, not added to the solver's stack, but
     * are merely considered for this one call.
     *
     * @param assumptions The assumptions to add to the call.
     * @return Sat if the conjunction of the asserted expressions together with the provided assumptions is
     * satisfiable, Unsat if it is unsatisfiable and Unknown if the solver could not determine satisfiability.
     */
    virtual CheckResult checkWithAssumptions(std::initializer_list<storm::expressions::Expression> const& assumptions) = 0;

    /*!
     * If the last call to check() or checkWithAssumptions() returned Sat, this method retrieves a model that
     * satisfies all assertions on the solver's stack (as well as provided assumptions), provided that the
     * solver was instantiated with support for model generation. Note that this function may throw an exception
     * if it is not called immediately after a call to check() or checkWithAssumptions() that returned Sat
     * depending on the implementation.
     *
     * @return A valuation that holds the values of the variables in the current model.
     */
    virtual storm::expressions::SimpleValuation getModelAsValuation();

    /*!
     * If the last call to check() or checkWithAssumptions() returned Sat, this method retrieves a model that
     * satisfies all assertions on the solver's stack (as well as provided assumptions), provided that the
     * solver was instantiated with support for model generation. Note that this function may throw an exception
     * if it is not called immediately after a call to check() or checkWithAssumptions() that returned Sat
     * depending on the implementation.
     *
     * @return A reference to a model that can be queried for the values of specific variables.
     */
    virtual std::shared_ptr<ModelReference> getModel();

    /*!
     * Performs AllSat over the (provided) important atoms. That is, this function returns all models of the
     * assertions on the solver's stack.
     *
     * @warning If infinitely many models exist, this function will never return.
     *
     * @param important The set of important atoms over which to perform all sat.
     *
     * @returns the set of all valuations of the important atoms, such that the currently asserted formulas are satisfiable
     */
    virtual std::vector<storm::expressions::SimpleValuation> allSat(std::vector<storm::expressions::Variable> const& important);

    /*!
     * Performs AllSat over the (provided) important atoms. That is, this function determines all models of the
     * assertions on the solver's stack. While doing so, every time a model is found, the provided callback is
     * called and informed about the content of the current model. The callback function can signal to abort the
     * enumeration process by returning false.
     *
     * @param important The set of important atoms over which to perform all sat.
     * @param callback A function to call for each found model.
     *
     * @return The number of models of the important atoms that where found.
     */
    virtual uint_fast64_t allSat(std::vector<storm::expressions::Variable> const& important,
                                 std::function<bool(storm::expressions::SimpleValuation&)> const& callback);

    /*!
     * Performs AllSat over the (provided) important atoms. That is, this function determines all models of the
     * assertions on the solver's stack. While doing so, every time a model is found, the provided callback is
     * called and informed about the content of the current model. The callback function can signal to abort the
     * enumeration process by returning false.
     *
     * @param important The set of important atoms over which to perform all sat.
     * @param callback A function to call for each found model.
     *
     * @return The number of models of the important atoms that where found.
     */
    virtual uint_fast64_t allSat(std::vector<storm::expressions::Variable> const& important, std::function<bool(ModelReference&)> const& callback);

    /*!
     * If the last call to check() returned Unsat, this function can be used to retrieve the unsatisfiable core
     * of the assertions on the solver's stack, provided that the solver has been instantiated with support for
     * the generation of unsatisfiable cores.
     *
     * @return A subset of the asserted formulas whose conjunction is unsatisfiable.
     */
    virtual std::vector<storm::expressions::Expression> getUnsatCore();

    /*!
     * If the last call to checkWithAssumptions() returned Unsat, this function can be used to retrieve a subset
     * of the assumptions such that the assertion stack and these assumptions are unsatisfiable. This may only
     * be called provided that the solver has been instantiated with support for the generation of unsatisfiable
     * cores.
     *
     * @return A subset of the assumptions of the last call to checkWithAssumptions whose conjunction with the
     * solver's stack is unsatisfiable.
     */
    virtual std::vector<storm::expressions::Expression> getUnsatAssumptions();

    /*!
     * Sets the current interpolation group. All terms added to the assertion stack after this call will belong
     * to the set group until the next call to this function. Note that, depending on the solver, it might not
     * be possible to "re-open" groups, so this should be used with care. Also, this functionality is only
     * available if the solver has been instantiated with support for interpolant generation.
     *
     * @param group The index of the interpolation group with which all assertions added after this call will be
     * associated.
     */
    virtual void setInterpolationGroup(uint_fast64_t group);

    /*!
     * If the last call to check() returned Unsat, the solver has been instantiated with support for interpolant
     * generation and at least two non-empty interpolation groups have been added, the function can be used to
     * retrieve an interpolant for the pair (A, B) of formulas where A is the conjunction of all the assertions
     * in the groups provided as a parameter and B is the set of all other assertions. To obtain meaningful
     * results, the conjunction of the formulas within one group should be satisfiable.
     *
     * @param groupsA The indices of all interpolation groups whose conjunctions form the formula A.
     *
     * @return The interpolant for the formulas (A, B), i.e. an expression I that is implied by A but the
     * conjunction of I and B is inconsistent.
     */
    virtual storm::expressions::Expression getInterpolant(std::vector<uint_fast64_t> const& groupsA);

    /*!
     * Retrieves the expression manager associated with the solver.
     *
     * @return The expression manager associated with the solver.
     */
    storm::expressions::ExpressionManager const& getManager() const;

    /*!
     * Retrieves the expression manager associated with the solver.
     *
     * @return The expression manager associated with the solver.
     */
    storm::expressions::ExpressionManager& getManager();

    /*!
     * If supported by the solver, this will limit all subsequent satisfiability queries to the given number of
     * milliseconds.
     *
     * @param milliseconds The amount of milliseconds before timing out.
     * @return True iff the solver supports setting a timeout.
     */
    virtual bool setTimeout(uint_fast64_t milliseconds);

    /*!
     * If supported by the solver, this unsets a previous timeout.
     *
     * @return True iff the solver supports timeouts.
     */
    virtual bool unsetTimeout();

    /*!
     * If supported by the solver, this function returns the current assertions in the SMT-LIB format.
     *
     * @return the current assertions in the SMT-LIB format.
     */
    virtual std::string getSmtLibString() const;

   private:
    // The manager responsible for the expressions that interact with this solver.
    storm::expressions::ExpressionManager& manager;
};
}  // namespace solver
}  // namespace storm

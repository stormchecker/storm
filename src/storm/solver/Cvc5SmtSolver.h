#pragma once

#include "storm-config.h"
#include "storm/adapters/Cvc5ExpressionAdapter.h"
#include "storm/solver/SmtSolver.h"

#ifdef STORM_HAVE_CVC5
#include <cvc5/cvc5.h>
#endif

namespace storm {
namespace solver {
class Cvc5SmtSolver : public SmtSolver {
   public:
    class Cvc5ModelReference : public SmtSolver::ModelReference {
       public:
        /*!
         * Creates a new model reference that provides access to the values that the given model assigns.
         *
         * The values are taken over by the model reference. This way, the reference does not depend on the
         * lifetime of the solver that produced the model, nor on the lifetime of an expression adapter.
         *
         * @param manager The manager responsible for the variables whose values can be requested.
         * @param variableValues The values that the model assigns to the variables.
         */
        Cvc5ModelReference(storm::expressions::ExpressionManager const& manager, storm::expressions::SimpleValuation variableValues);

        virtual bool getBooleanValue(storm::expressions::Variable const& variable) const override;
        virtual int_fast64_t getIntegerValue(storm::expressions::Variable const& variable) const override;
        virtual double getRationalValue(storm::expressions::Variable const& variable) const override;
        virtual std::string toString() const override;

       private:
        /*!
         * Checks whether the given variable may be used with this model reference.
         */
        void checkVariable(storm::expressions::Variable const& variable) const;

        // The values that the model assigns to the variables.
        storm::expressions::SimpleValuation variableValues;
    };

   public:
    Cvc5SmtSolver(storm::expressions::ExpressionManager& manager);
    virtual ~Cvc5SmtSolver();

    virtual void push() override;

    virtual void pop() override;

    virtual void pop(uint_fast64_t n) override;

    virtual void reset() override;

    virtual void add(storm::expressions::Expression const& assertion) override;

    virtual void addNotCurrentModel(bool performSolverReset = true) override;

    virtual CheckResult check() override;

    virtual CheckResult checkWithAssumptions(std::set<storm::expressions::Expression> const& assumptions) override;

    virtual CheckResult checkWithAssumptions(std::initializer_list<storm::expressions::Expression> const& assumptions) override;

    virtual storm::expressions::SimpleValuation getModelAsValuation() override;

    virtual std::shared_ptr<SmtSolver::ModelReference> getModel() override;

    virtual std::vector<storm::expressions::SimpleValuation> allSat(std::vector<storm::expressions::Variable> const& important) override;

    virtual uint_fast64_t allSat(std::vector<storm::expressions::Variable> const& important,
                                 std::function<bool(storm::expressions::SimpleValuation&)> const& callback) override;

    virtual uint_fast64_t allSat(std::vector<storm::expressions::Variable> const& important, std::function<bool(ModelReference&)> const& callback) override;

    virtual std::vector<storm::expressions::Expression> getUnsatAssumptions() override;

    virtual bool setTimeout(uint_fast64_t milliseconds) override;

    virtual bool unsetTimeout() override;

   private:
#ifdef STORM_HAVE_CVC5
    /*!
     * Translates the result of the last satisfiability check to a check result.
     */
    SmtSolver::CheckResult translateResult(cvc5::Result const& result) const;

    /*!
     * Collects the values that the current model assigns to all variables of the expression manager.
     */
    std::unordered_map<storm::expressions::Variable, cvc5::Term> collectVariableValues() const;

    /*!
     * Interprets the model that was found last as a valuation of the expression manager's variables.
     *
     * Variables that the model does not constrain are left at the valuation's default value.
     *
     * @return The values that the model assigns to the variables.
     */
    storm::expressions::SimpleValuation collectModelAsValuation() const;

    /*!
     * Creates a term that describes the model that was found last.
     *
     * @param variables The variables over which to describe the model.
     * @return A term that is true iff the last model assigns the current values to the given variables.
     */
    cvc5::Term createModelExpression(std::vector<storm::expressions::Variable> const& variables) const;

    // The manager that terms and sorts are built with, which is also passed to the cvc5::Solver constructor. It
    // is declared before the solver so that it outlives it, as the terms of the solver refer to it.
    std::unique_ptr<cvc5::TermManager> termManager;

    // The actual solver object.
    std::unique_ptr<cvc5::Solver> solver;

    // An expression adapter that is used for translating the expression into CVC5's format.
    std::unique_ptr<storm::adapters::Cvc5ExpressionAdapter> expressionAdapter;

    // A flag storing whether the last call to a check method provided assumptions.
    bool lastCheckAssumptions;

    // The last result that was returned by any of the check methods.
    CheckResult lastResult;
#endif
};
}  // namespace solver
}  // namespace storm

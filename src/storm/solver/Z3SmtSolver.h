#pragma once

#include "storm-config.h"
#include "storm/adapters/Z3ExpressionAdapter.h"
#include "storm/solver/SmtSolver.h"

#ifdef STORM_HAVE_Z3
#include "z3++.h"
#include "z3.h"
#endif

namespace storm {
namespace solver {
class Z3SmtSolver : public SmtSolver {
   public:
    class Z3ModelReference : public SmtSolver::ModelReference {
       public:
#ifdef STORM_HAVE_Z3
        Z3ModelReference(storm::expressions::ExpressionManager const& manager, z3::model const& m, storm::adapters::Z3ExpressionAdapter& expressionAdapter);
#endif
        virtual bool getBooleanValue(storm::expressions::Variable const& variable) const override;
        virtual int_fast64_t getIntegerValue(storm::expressions::Variable const& variable) const override;
        virtual double getRationalValue(storm::expressions::Variable const& variable) const override;
        virtual std::string toString() const override;

       private:
#ifdef STORM_HAVE_Z3
        // The Z3 model out of which the information can be extracted.
        z3::model model;

        // The expression adapter that is used to translate the variable names.
        storm::adapters::Z3ExpressionAdapter& expressionAdapter;
#endif
    };

   public:
    Z3SmtSolver(storm::expressions::ExpressionManager& manager);
    virtual ~Z3SmtSolver();

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

    virtual std::string getSmtLibString() const override;

   private:
#ifdef STORM_HAVE_Z3
    /*!
     * Converts the given Z3 model to an evaluation.
     *
     * @param model The Z3 model to convert.
     * @return The valuation of variables corresponding to the given model.
     */
    storm::expressions::SimpleValuation convertZ3ModelToValuation(z3::model const& model);

    // The context used by the solver.
    std::unique_ptr<z3::context> context;

    // The actual solver object.
    std::unique_ptr<z3::solver> solver;

    // An expression adapter that is used for translating the expression into Z3's format.
    std::unique_ptr<storm::adapters::Z3ExpressionAdapter> expressionAdapter;

    // A flag storing whether the last call to a check method provided aussumptions.
    bool lastCheckAssumptions;

    // The last result that was returned by any of the check methods.
    CheckResult lastResult;
#endif
};
}  // namespace solver
}  // namespace storm

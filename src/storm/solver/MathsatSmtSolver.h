#pragma once

#include <boost/container/flat_map.hpp>
#include "storm-config.h"
#include "storm/adapters/MathsatExpressionAdapter.h"
#include "storm/solver/SmtSolver.h"

#ifdef STORM_HAVE_MATHSAT
#include "mathsat.h"
#endif

namespace storm {
namespace solver {
class MathsatSmtSolver : public SmtSolver {
   public:
    /*!
     * A class that captures options that may be passed to the Mathsat solver. Settings these options has some
     * implications due to the implementation of MathSAT. For example, enabling interpolation means that the
     * solver must not be used for checking satisfiable formulas.
     */
    class Options {
       public:
        Options(bool enableModelGeneration = true, bool enableUnsatCoreGeneration = false, bool enableInterpolantGeneration = false)
            : enableModelGeneration(enableModelGeneration),
              enableUnsatCoreGeneration(enableUnsatCoreGeneration),
              enableInterpolantGeneration(enableInterpolantGeneration) {
            // Intentionally left empty.
        }

        bool enableModelGeneration = false;
        bool enableUnsatCoreGeneration = false;
        bool enableInterpolantGeneration = false;
    };

#ifdef STORM_HAVE_MATHSAT
    class MathsatAllsatModelReference : public SmtSolver::ModelReference {
       public:
        MathsatAllsatModelReference(storm::expressions::ExpressionManager const& manager, msat_env const& env, msat_term* model,
                                    std::unordered_map<storm::expressions::Variable, uint_fast64_t> const& variableNameToSlotMapping);

        virtual bool getBooleanValue(storm::expressions::Variable const& variable) const override;
        virtual int_fast64_t getIntegerValue(storm::expressions::Variable const& variable) const override;
        virtual double getRationalValue(storm::expressions::Variable const& variable) const override;
        virtual std::string toString() const override;

       private:
        msat_env const& env;
        msat_term* model;
        std::unordered_map<storm::expressions::Variable, uint_fast64_t> const& variableToSlotMapping;
    };
#endif

#ifdef STORM_HAVE_MATHSAT
    class MathsatModelReference : public SmtSolver::ModelReference {
       public:
        MathsatModelReference(storm::expressions::ExpressionManager const& manager, msat_env const& env,
                              storm::adapters::MathsatExpressionAdapter& expressionAdapter);

        virtual bool getBooleanValue(storm::expressions::Variable const& variable) const override;
        virtual int_fast64_t getIntegerValue(storm::expressions::Variable const& variable) const override;
        virtual double getRationalValue(storm::expressions::Variable const& variable) const override;
        virtual std::string toString() const override;

       private:
        msat_env const& env;
        storm::adapters::MathsatExpressionAdapter& expressionAdapter;
    };
#endif

    MathsatSmtSolver(storm::expressions::ExpressionManager& manager, Options const& options = Options());

    virtual ~MathsatSmtSolver();

    virtual void push() override;

    virtual void pop() override;

    virtual void pop(uint_fast64_t n) override;

    virtual void reset() override;

    virtual void add(storm::expressions::Expression const& assertion) override;

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

    virtual void setInterpolationGroup(uint_fast64_t group) override;

    virtual storm::expressions::Expression getInterpolant(std::vector<uint_fast64_t> const& groupsA) override;

   private:
    storm::expressions::SimpleValuation convertMathsatModelToValuation();

#ifdef STORM_HAVE_MATHSAT
    // The MathSAT environment.
    msat_env env;

    // The expression adapter used to translate expressions to MathSAT's format. This has to be a pointer, since
    // it must be initialized after creating the environment, but the adapter class has no default constructor.
    std::unique_ptr<storm::adapters::MathsatExpressionAdapter> expressionAdapter;
#endif

    // A flag storing whether the last call was a check involving assumptions.
    bool lastCheckAssumptions;

    // The result of the last call to any of the check methods.
    CheckResult lastResult;

    // A mapping of interpolation group indices to their MathSAT identifier.
    typedef boost::container::flat_map<uint_fast64_t, int> InterpolationGroupMapType;
    InterpolationGroupMapType interpolationGroups;
};
}  // namespace solver
}  // namespace storm

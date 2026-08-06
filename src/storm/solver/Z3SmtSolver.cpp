#include "storm/solver/Z3SmtSolver.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
#ifdef STORM_HAVE_Z3
Z3SmtSolver::Z3ModelReference::Z3ModelReference(storm::expressions::ExpressionManager const& manager, z3::model const& model,
                                                storm::adapters::Z3ExpressionAdapter& expressionAdapter)
    : ModelReference(manager), model(model), expressionAdapter(expressionAdapter) {
    // Intentionally left empty.
}
#endif

bool Z3SmtSolver::Z3ModelReference::getBooleanValue(storm::expressions::Variable const& variable) const {
#ifdef STORM_HAVE_Z3
    STORM_LOG_ASSERT(variable.getManager() == this->getManager(), "Requested variable is managed by a different manager.");
    z3::expr z3Expr = this->expressionAdapter.translateExpression(variable);
    z3::expr z3ExprValuation = model.eval(z3Expr, true);
    return this->expressionAdapter.translateExpression(z3ExprValuation).isTrue();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

int_fast64_t Z3SmtSolver::Z3ModelReference::getIntegerValue(storm::expressions::Variable const& variable) const {
#ifdef STORM_HAVE_Z3
    STORM_LOG_ASSERT(variable.getManager() == this->getManager(), "Requested variable is managed by a different manager.");
    z3::expr z3Expr = this->expressionAdapter.translateExpression(variable);
    z3::expr z3ExprValuation = model.eval(z3Expr, true);
    return this->expressionAdapter.translateExpression(z3ExprValuation).evaluateAsInt();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

double Z3SmtSolver::Z3ModelReference::getRationalValue(storm::expressions::Variable const& variable) const {
#ifdef STORM_HAVE_Z3
    STORM_LOG_ASSERT(variable.getManager() == this->getManager(), "Requested variable is managed by a different manager.");
    z3::expr z3Expr = this->expressionAdapter.translateExpression(variable);
    z3::expr z3ExprValuation = model.eval(z3Expr, true);
    return this->expressionAdapter.translateExpression(z3ExprValuation).evaluateAsDouble();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

std::string Z3SmtSolver::Z3ModelReference::toString() const {
#ifdef STORM_HAVE_Z3
    std::stringstream sstr;
    sstr << model;
    return sstr.str();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

Z3SmtSolver::Z3SmtSolver(storm::expressions::ExpressionManager& manager)
    : SmtSolver(manager)
#ifdef STORM_HAVE_Z3
      ,
      context(nullptr),
      solver(nullptr),
      expressionAdapter(nullptr),
      lastCheckAssumptions(false),
      lastResult(CheckResult::Unknown)
#endif
{
#ifdef STORM_HAVE_Z3
    z3::config config;
    config.set("model", true);
    context = std::unique_ptr<z3::context>(new z3::context(config));
    solver = std::unique_ptr<z3::solver>(new z3::solver(*context));
    expressionAdapter = std::unique_ptr<storm::adapters::Z3ExpressionAdapter>(new storm::adapters::Z3ExpressionAdapter(this->getManager(), *context));
#endif
}

Z3SmtSolver::~Z3SmtSolver() {
    // Intentionally left empty.
}

void Z3SmtSolver::push() {
#ifdef STORM_HAVE_Z3
    this->solver->push();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

void Z3SmtSolver::pop() {
#ifdef STORM_HAVE_Z3
    this->solver->pop();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

void Z3SmtSolver::pop(uint_fast64_t n) {
#ifdef STORM_HAVE_Z3
    this->solver->pop(static_cast<unsigned int>(n));
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

void Z3SmtSolver::reset() {
#ifdef STORM_HAVE_Z3
    this->solver->reset();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

void Z3SmtSolver::add(storm::expressions::Expression const& assertion) {
#ifdef STORM_HAVE_Z3
    this->solver->add(expressionAdapter->translateExpression(assertion));
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

void Z3SmtSolver::addNotCurrentModel(bool performSolverReset) {
#ifdef STORM_HAVE_Z3
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");

    auto currentModel = this->solver->get_model();
    z3::expr notThisModel = currentModel.ctx().bool_val(true);
    for (auto const& variable : this->getManager().getVariables()) {
        z3::expr var = this->expressionAdapter->translateExpression(variable);
        auto value = currentModel.eval(var);
        if (notThisModel.is_const()) {
            notThisModel = var == value;
        } else {
            notThisModel = notThisModel && (var == value);
        }
    }
    // https://stackoverflow.com/questions/78261966/z3-non-incremental-search-for-multiple-models-works-incremental-search-does-no
    if (performSolverReset) {
        auto const allAssertions = this->solver->assertions();
        solver->reset();
        for (auto const& assertion : allAssertions) {
            solver->add(assertion);
        }
    }
    this->solver->add(!notThisModel);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

SmtSolver::CheckResult Z3SmtSolver::check() {
#ifdef STORM_HAVE_Z3
    lastCheckAssumptions = false;
    switch (this->solver->check()) {
        case z3::sat:
            this->lastResult = SmtSolver::CheckResult::Sat;
            break;
        case z3::unsat:
            this->lastResult = SmtSolver::CheckResult::Unsat;
            break;
        default:
            this->lastResult = SmtSolver::CheckResult::Unknown;
            break;
    }
    return this->lastResult;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

SmtSolver::CheckResult Z3SmtSolver::checkWithAssumptions(std::set<storm::expressions::Expression> const& assumptions) {
#ifdef STORM_HAVE_Z3
    lastCheckAssumptions = true;
    z3::expr_vector z3Assumptions(*this->context);

    for (storm::expressions::Expression assumption : assumptions) {
        z3Assumptions.push_back(this->expressionAdapter->translateExpression(assumption));
    }

    switch (this->solver->check(z3Assumptions)) {
        case z3::sat:
            this->lastResult = SmtSolver::CheckResult::Sat;
            break;
        case z3::unsat:
            this->lastResult = SmtSolver::CheckResult::Unsat;
            break;
        default:
            this->lastResult = SmtSolver::CheckResult::Unknown;
            break;
    }
    return this->lastResult;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

SmtSolver::CheckResult Z3SmtSolver::checkWithAssumptions(std::initializer_list<storm::expressions::Expression> const& assumptions) {
#ifdef STORM_HAVE_Z3
    lastCheckAssumptions = true;
    z3::expr_vector z3Assumptions(*this->context);

    for (storm::expressions::Expression assumption : assumptions) {
        z3Assumptions.push_back(this->expressionAdapter->translateExpression(assumption));
    }

    switch (this->solver->check(z3Assumptions)) {
        case z3::sat:
            this->lastResult = SmtSolver::CheckResult::Sat;
            break;
        case z3::unsat:
            this->lastResult = SmtSolver::CheckResult::Unsat;
            break;
        default:
            this->lastResult = SmtSolver::CheckResult::Unknown;
            break;
    }
    return this->lastResult;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

storm::expressions::SimpleValuation Z3SmtSolver::getModelAsValuation() {
#ifdef STORM_HAVE_Z3
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");
    return this->convertZ3ModelToValuation(this->solver->get_model());
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

std::shared_ptr<SmtSolver::ModelReference> Z3SmtSolver::getModel() {
#ifdef STORM_HAVE_Z3
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");
    return std::shared_ptr<SmtSolver::ModelReference>(new Z3ModelReference(this->getManager(), this->solver->get_model(), *this->expressionAdapter));
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

#ifdef STORM_HAVE_Z3
storm::expressions::SimpleValuation Z3SmtSolver::convertZ3ModelToValuation(z3::model const& model) {
    storm::expressions::SimpleValuation stormModel(this->getManager().getSharedPointer());

    for (unsigned i = 0; i < model.num_consts(); ++i) {
        z3::func_decl variableI = model.get_const_decl(i);
        storm::expressions::Variable stormVariable = this->expressionAdapter->getVariable(variableI);
        storm::expressions::Expression variableInterpretation = this->expressionAdapter->translateExpression(model.get_const_interp(variableI));

        if (variableInterpretation.getType().isBooleanType()) {
            stormModel.setBooleanValue(this->getManager().getVariable(variableI.name().str()), variableInterpretation.isTrue());
        } else if (variableInterpretation.getType().isIntegerType()) {
            stormModel.setIntegerValue(this->getManager().getVariable(variableI.name().str()), variableInterpretation.evaluateAsInt());
        } else if (variableInterpretation.getType().isRationalType()) {
            stormModel.setRationalValue(this->getManager().getVariable(variableI.name().str()), variableInterpretation.evaluateAsDouble());
        } else {
            STORM_LOG_ASSERT(false, "Variable interpretation in model is not of type bool, int or rational.");
        }
    }

    return stormModel;
}
#endif

std::vector<storm::expressions::SimpleValuation> Z3SmtSolver::allSat(std::vector<storm::expressions::Variable> const& important) {
#ifdef STORM_HAVE_Z3
    std::vector<storm::expressions::SimpleValuation> valuations;
    this->allSat(important, static_cast<std::function<bool(storm::expressions::SimpleValuation&)>>(
                                [&valuations](storm::expressions::SimpleValuation const& valuation) -> bool {
                                    valuations.push_back(valuation);
                                    return true;
                                }));
    return valuations;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

uint_fast64_t Z3SmtSolver::allSat(std::vector<storm::expressions::Variable> const& important,
                                  std::function<bool(storm::expressions::SimpleValuation&)> const& callback) {
#ifdef STORM_HAVE_Z3
    for (storm::expressions::Variable const& variable : important) {
        STORM_LOG_THROW(variable.hasBooleanType(), storm::exceptions::InvalidArgumentException, "The important atoms for AllSat must be boolean variables.");
    }

    uint_fast64_t numberOfModels = 0;
    bool proceed = true;

    // Save the current assertion stack, to be able to backtrack after the procedure.
    this->push();

    // Enumerate models as long as the conjunction is satisfiable and the callback has not aborted the enumeration.
    while (proceed && this->check() == CheckResult::Sat) {
        ++numberOfModels;
        z3::model model = this->solver->get_model();

        z3::expr modelExpr = this->context->bool_val(true);
        storm::expressions::SimpleValuation valuation(this->getManager().getSharedPointer());

        for (storm::expressions::Variable const& importantAtom : important) {
            z3::expr z3ImportantAtom = this->expressionAdapter->translateExpression(importantAtom.getExpression());
            z3::expr z3ImportantAtomValuation = model.eval(z3ImportantAtom, true);
            modelExpr = modelExpr && (z3ImportantAtom == z3ImportantAtomValuation);
            valuation.setBooleanValue(importantAtom, this->expressionAdapter->translateExpression(z3ImportantAtomValuation).isTrue());
        }

        // Check if we are required to proceed, and if so rule out the current model.
        proceed = callback(valuation);
        if (proceed) {
            this->solver->add(!modelExpr);
        }
    }

    // Restore the old assertion stack and return.
    this->pop();
    return numberOfModels;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

uint_fast64_t Z3SmtSolver::allSat(std::vector<storm::expressions::Variable> const& important, std::function<bool(SmtSolver::ModelReference&)> const& callback) {
#ifdef STORM_HAVE_Z3
    for (storm::expressions::Variable const& variable : important) {
        STORM_LOG_THROW(variable.hasBooleanType(), storm::exceptions::InvalidArgumentException, "The important atoms for AllSat must be boolean variables.");
    }

    uint_fast64_t numberOfModels = 0;
    bool proceed = true;

    // Save the current assertion stack, to be able to backtrack after the procedure.
    this->push();

    // Enumerate models as long as the conjunction is satisfiable and the callback has not aborted the enumeration.
    while (proceed && this->check() == CheckResult::Sat) {
        ++numberOfModels;
        z3::model model = this->solver->get_model();

        z3::expr modelExpr = this->context->bool_val(true);
        storm::expressions::SimpleValuation valuation(this->getManager().getSharedPointer());

        for (storm::expressions::Variable const& importantAtom : important) {
            z3::expr z3ImportantAtom = this->expressionAdapter->translateExpression(importantAtom.getExpression());
            z3::expr z3ImportantAtomValuation = model.eval(z3ImportantAtom, true);
            modelExpr = modelExpr && (z3ImportantAtom == z3ImportantAtomValuation);
        }
        Z3ModelReference modelRef(this->getManager(), model, *expressionAdapter);

        // Check if we are required to proceed, and if so rule out the current model.
        proceed = callback(modelRef);
        if (proceed) {
            this->solver->add(!modelExpr);
        }
    }

    this->pop();
    return numberOfModels;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

std::vector<storm::expressions::Expression> Z3SmtSolver::getUnsatAssumptions() {
#ifdef STORM_HAVE_Z3
    STORM_LOG_THROW(lastResult == SmtSolver::CheckResult::Unsat, storm::exceptions::InvalidStateException,
                    "Unable to generate unsatisfiable core of assumptions, because the last check did not determine the formulas to be unsatisfiable.");
    STORM_LOG_THROW(lastCheckAssumptions, storm::exceptions::InvalidStateException,
                    "Unable to generate unsatisfiable core of assumptions, because the last check did not involve assumptions.");

    z3::expr_vector z3UnsatAssumptions = this->solver->unsat_core();
    std::vector<storm::expressions::Expression> unsatAssumptions;

    for (unsigned int i = 0; i < z3UnsatAssumptions.size(); ++i) {
        unsatAssumptions.push_back(this->expressionAdapter->translateExpression(z3UnsatAssumptions[i]));
    }

    return unsatAssumptions;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

bool Z3SmtSolver::setTimeout(uint_fast64_t milliseconds) {
#ifdef STORM_HAVE_Z3
    z3::params paramObject(*context);
    paramObject.set(":timeout", static_cast<unsigned>(milliseconds));
    solver->set(paramObject);
    return true;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

bool Z3SmtSolver::unsetTimeout() {
#ifdef STORM_HAVE_Z3
    z3::params paramObject(*context);
    paramObject.set(":timeout", 0u);
    solver->set(paramObject);
    return true;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

std::string Z3SmtSolver::getSmtLibString() const {
#ifdef STORM_HAVE_Z3
    return solver->to_smt2();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Z3 support.");
#endif
}

}  // namespace solver
}  // namespace storm

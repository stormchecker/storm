#include "storm/solver/Cvc5SmtSolver.h"

#include <memory>

#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
// Deliberately not guarded by STORM_HAVE_CVC5: the header declares this constructor unconditionally, and it only
// touches Storm's valuation types. Guarding it would leave the definition out in a build without CVC5, so any
// client constructing a Cvc5ModelReference there would compile but fail to link.
Cvc5SmtSolver::Cvc5ModelReference::Cvc5ModelReference(storm::expressions::ExpressionManager const& manager, storm::expressions::SimpleValuation variableValues)
    : ModelReference(manager), variableValues(std::move(variableValues)) {
    // Intentionally left empty.
}

void Cvc5SmtSolver::Cvc5ModelReference::checkVariable(storm::expressions::Variable const& variable) const {
    STORM_LOG_ASSERT(variable.getManager() == this->getManager(), "Requested variable is managed by a different manager.");
}

bool Cvc5SmtSolver::Cvc5ModelReference::getBooleanValue(storm::expressions::Variable const& variable) const {
    this->checkVariable(variable);
    STORM_LOG_THROW(variable.getType().isBooleanType(), storm::exceptions::InvalidTypeException,
                    "Cannot retrieve the Boolean value of variable '" << variable.getName() << "' of non-Boolean type.");
    return this->variableValues.getBooleanValue(variable);
}

int_fast64_t Cvc5SmtSolver::Cvc5ModelReference::getIntegerValue(storm::expressions::Variable const& variable) const {
    this->checkVariable(variable);
    // Note that a bitvector variable is an integer type in Storm, and its value is stored among the integer
    // values. We therefore accept it here just like SimpleValuation does.
    STORM_LOG_THROW(variable.getType().isIntegerType(), storm::exceptions::InvalidTypeException,
                    "Cannot retrieve the integer value of variable '" << variable.getName() << "' of non-integer type.");
    return this->variableValues.getIntegerValue(variable);
}

double Cvc5SmtSolver::Cvc5ModelReference::getRationalValue(storm::expressions::Variable const& variable) const {
    this->checkVariable(variable);
    STORM_LOG_THROW(variable.getType().isRationalType(), storm::exceptions::InvalidTypeException,
                    "Cannot retrieve the rational value of variable '" << variable.getName() << "' of non-rational type.");
    return this->variableValues.getRationalValue(variable);
}

std::string Cvc5SmtSolver::Cvc5ModelReference::toString() const {
    return this->variableValues.toString();
}

Cvc5SmtSolver::Cvc5SmtSolver(storm::expressions::ExpressionManager& manager)
    : SmtSolver(manager)
#ifdef STORM_HAVE_CVC5
      ,
      termManager(nullptr),
      solver(nullptr),
      expressionAdapter(nullptr),
      lastCheckAssumptions(false),
      lastResult(CheckResult::Unknown)
#endif
{
#ifdef STORM_HAVE_CVC5
    this->termManager = std::make_unique<cvc5::TermManager>();
    this->solver = std::make_unique<cvc5::Solver>(*this->termManager);
    this->solver->setOption("produce-models", "true");
    this->solver->setOption("produce-unsat-assumptions", "true");
    this->solver->setOption("incremental", "true");
    this->expressionAdapter = std::make_unique<storm::adapters::Cvc5ExpressionAdapter>(this->getManager(), *this->solver);
#endif
}

Cvc5SmtSolver::~Cvc5SmtSolver() {
    // Intentionally left empty.
}

void Cvc5SmtSolver::push() {
#ifdef STORM_HAVE_CVC5
    this->solver->push();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

void Cvc5SmtSolver::pop() {
#ifdef STORM_HAVE_CVC5
    this->solver->pop();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

void Cvc5SmtSolver::pop(uint_fast64_t n) {
#ifdef STORM_HAVE_CVC5
    this->solver->pop(static_cast<uint32_t>(n));
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

void Cvc5SmtSolver::reset() {
#ifdef STORM_HAVE_CVC5
    this->solver->resetAssertions();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

void Cvc5SmtSolver::add(storm::expressions::Expression const& assertion) {
#ifdef STORM_HAVE_CVC5
    this->solver->assertFormula(this->expressionAdapter->translateExpression(assertion));
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

void Cvc5SmtSolver::addNotCurrentModel(bool performSolverReset) {
#ifdef STORM_HAVE_CVC5
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");

    // Note that bitvector variables are part of the model just like the other ones. They are represented as
    // integers in this backend, so createModelExpression can describe them without further ado. Leaving them out
    // would result in an empty conjunction, i.e., in a blocking clause that is simply false.
    std::vector<storm::expressions::Variable> variables;
    for (auto const& variable : this->getManager().getVariables()) {
        variables.push_back(variable);
    }

    cvc5::Term notThisModel = this->createModelExpression(variables);
    notThisModel = this->termManager->mkTerm(cvc5::Kind::NOT, {notThisModel});

    // Similar to Z3, CVC5 does not necessarily search for a different model when new assertions are added
    // in incremental mode. To circumvent this, we reset the solver and re-assert all assertions. Note that this
    // discards the push/pop scopes (as it does for the Z3 backend): all assertions end up on the base level.
    if (performSolverReset) {
        auto const allAssertions = this->solver->getAssertions();
        this->solver->resetAssertions();
        for (auto const& assertion : allAssertions) {
            this->solver->assertFormula(assertion);
        }
    }
    this->solver->assertFormula(notThisModel);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

SmtSolver::CheckResult Cvc5SmtSolver::check() {
#ifdef STORM_HAVE_CVC5
    this->lastCheckAssumptions = false;
    this->lastResult = this->translateResult(this->solver->checkSat());
    return this->lastResult;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

SmtSolver::CheckResult Cvc5SmtSolver::checkWithAssumptions(storm::expressions::ExpressionSet const& assumptions) {
#ifdef STORM_HAVE_CVC5
    this->lastCheckAssumptions = true;
    std::vector<cvc5::Term> cvc5Assumptions;
    for (storm::expressions::Expression const& assumption : assumptions) {
        cvc5Assumptions.push_back(this->expressionAdapter->translateExpression(assumption));
    }
    this->lastResult = this->translateResult(this->solver->checkSatAssuming(cvc5Assumptions));
    return this->lastResult;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

SmtSolver::CheckResult Cvc5SmtSolver::checkWithAssumptions(std::initializer_list<storm::expressions::Expression> const& assumptions) {
#ifdef STORM_HAVE_CVC5
    this->lastCheckAssumptions = true;
    std::vector<cvc5::Term> cvc5Assumptions;
    for (storm::expressions::Expression const& assumption : assumptions) {
        cvc5Assumptions.push_back(this->expressionAdapter->translateExpression(assumption));
    }
    this->lastResult = this->translateResult(this->solver->checkSatAssuming(cvc5Assumptions));
    return this->lastResult;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

#ifdef STORM_HAVE_CVC5
storm::expressions::SimpleValuation Cvc5SmtSolver::collectModelAsValuation() const {
    storm::expressions::SimpleValuation stormModel(this->getManager().getSharedPointer());
    for (auto const& variableValuePair : this->collectVariableValues()) {
        storm::expressions::Variable const& variable = variableValuePair.first;
        cvc5::Term const& value = variableValuePair.second;
        if (variable.getType().isBooleanType()) {
            stormModel.setBooleanValue(variable, value == this->termManager->mkBoolean(true));
        } else if (variable.getType().isIntegerType()) {
            auto const& interpretation = this->expressionAdapter->translateExpression(value);
            auto const* integerLiteral = dynamic_cast<storm::expressions::IntegerLiteralExpression const*>(&interpretation.getBaseExpression());
            STORM_LOG_ASSERT(integerLiteral != nullptr, "Variable interpretation in model is not an integer.");
            stormModel.setIntegerValue(variable, integerLiteral->getValue());
        } else if (variable.getType().isRationalType()) {
            // CVC5 reports a value that is not a rational number, such as the positive root of x^2 = 2, as a real
            // algebraic number. Since a SimpleValuation holds a double, we approximate such a value by its lower
            // bound, just like the Z3 backend does.
            cvc5::Term approximableValue = value;
            if (value.isRealAlgebraicNumber()) {
                STORM_LOG_WARN("Interpreting algebraic numbers as rational numbers. This is not an exact computation.");
                approximableValue = value.getRealAlgebraicNumberLowerBound();
            }
            auto const& interpretation = this->expressionAdapter->translateExpression(approximableValue);
            auto const* rationalLiteral = dynamic_cast<storm::expressions::RationalLiteralExpression const*>(&interpretation.getBaseExpression());
            STORM_LOG_ASSERT(rationalLiteral != nullptr, "Variable interpretation in model is not a rational number.");
            if (rationalLiteral != nullptr) {
                stormModel.setRationalValue(variable, storm::utility::convertNumber<double>(rationalLiteral->getValue()));
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException, "Variable interpretation in model is not of type bool, int or rational.");
        }
    }

    return stormModel;
}
#endif

storm::expressions::SimpleValuation Cvc5SmtSolver::getModelAsValuation() {
#ifdef STORM_HAVE_CVC5
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");
    return this->collectModelAsValuation();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

std::shared_ptr<SmtSolver::ModelReference> Cvc5SmtSolver::getModel() {
#ifdef STORM_HAVE_CVC5
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");
    return std::shared_ptr<SmtSolver::ModelReference>(new Cvc5ModelReference(this->getManager(), this->collectModelAsValuation()));
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

std::vector<storm::expressions::SimpleValuation> Cvc5SmtSolver::allSat(std::vector<storm::expressions::Variable> const& important) {
#ifdef STORM_HAVE_CVC5
    std::vector<storm::expressions::SimpleValuation> valuations;
    this->allSat(important, static_cast<std::function<bool(storm::expressions::SimpleValuation&)>>(
                                [&valuations](storm::expressions::SimpleValuation const& valuation) -> bool {
                                    valuations.push_back(valuation);
                                    return true;
                                }));
    return valuations;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

uint_fast64_t Cvc5SmtSolver::allSat(std::vector<storm::expressions::Variable> const& important,
                                    std::function<bool(storm::expressions::SimpleValuation&)> const& callback) {
#ifdef STORM_HAVE_CVC5
    for (storm::expressions::Variable const& variable : important) {
        STORM_LOG_THROW(variable.hasBooleanType(), storm::exceptions::InvalidArgumentException, "The important atoms for AllSat must be boolean variables.");
    }

    uint64_t numberOfModels = 0;
    bool proceed = true;

    // Save the current assertion stack, to be able to backtrack after the procedure.
    this->push();

    // Enumerate models as long as the conjunction is satisfiable and the callback has not aborted the enumeration.
    while (proceed && this->check() == CheckResult::Sat) {
        ++numberOfModels;

        cvc5::Term modelExpr = this->createModelExpression(important);
        storm::expressions::SimpleValuation valuation(this->getManager().getSharedPointer());

        for (storm::expressions::Variable const& importantAtom : important) {
            cvc5::Term value = this->solver->getValue(this->expressionAdapter->translateExpression(importantAtom));
            valuation.setBooleanValue(importantAtom, value == this->termManager->mkBoolean(true));
        }

        // Check if we are required to proceed, and if so rule out the current model.
        proceed = callback(valuation);
        if (proceed) {
            this->solver->assertFormula(this->termManager->mkTerm(cvc5::Kind::NOT, {modelExpr}));
        }
    }

    // Restore the old assertion stack and return.
    this->pop();
    return numberOfModels;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

uint_fast64_t Cvc5SmtSolver::allSat(std::vector<storm::expressions::Variable> const& important,
                                    std::function<bool(SmtSolver::ModelReference&)> const& callback) {
#ifdef STORM_HAVE_CVC5
    for (storm::expressions::Variable const& variable : important) {
        STORM_LOG_THROW(variable.hasBooleanType(), storm::exceptions::InvalidArgumentException, "The important atoms for AllSat must be boolean variables.");
    }

    uint64_t numberOfModels = 0;
    bool proceed = true;

    // Save the current assertion stack, to be able to backtrack after the procedure.
    this->push();

    // Enumerate models as long as the conjunction is satisfiable and the callback has not aborted the enumeration.
    while (proceed && this->check() == CheckResult::Sat) {
        ++numberOfModels;

        cvc5::Term modelExpr = this->createModelExpression(important);
        Cvc5ModelReference modelRef(this->getManager(), this->collectModelAsValuation());

        // Check if we are required to proceed, and if so rule out the current model.
        proceed = callback(modelRef);
        if (proceed) {
            this->solver->assertFormula(this->termManager->mkTerm(cvc5::Kind::NOT, {modelExpr}));
        }
    }

    this->pop();
    return numberOfModels;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

std::vector<storm::expressions::Expression> Cvc5SmtSolver::getUnsatAssumptions() {
#ifdef STORM_HAVE_CVC5
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Unsat, storm::exceptions::InvalidStateException,
                    "Unable to generate unsatisfiable core of assumptions, because the last check did not determine the formulas to be unsatisfiable.");
    STORM_LOG_THROW(this->lastCheckAssumptions, storm::exceptions::InvalidStateException,
                    "Unable to generate unsatisfiable core of assumptions, because the last check did not involve assumptions.");

    std::vector<storm::expressions::Expression> unsatAssumptions;
    for (auto const& cvc5Assumption : this->solver->getUnsatAssumptions()) {
        unsatAssumptions.push_back(this->expressionAdapter->translateExpression(cvc5Assumption));
    }

    return unsatAssumptions;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

bool Cvc5SmtSolver::setTimeout(uint_fast64_t milliseconds) {
#ifdef STORM_HAVE_CVC5
    this->solver->setOption("tlimit-per", std::to_string(milliseconds));
    return true;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

bool Cvc5SmtSolver::unsetTimeout() {
#ifdef STORM_HAVE_CVC5
    this->solver->setOption("tlimit-per", "0");
    return true;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without CVC5 support.");
#endif
}

#ifdef STORM_HAVE_CVC5
SmtSolver::CheckResult Cvc5SmtSolver::translateResult(cvc5::Result const& result) const {
    if (result.isSat()) {
        return SmtSolver::CheckResult::Sat;
    } else if (result.isUnsat()) {
        return SmtSolver::CheckResult::Unsat;
    }
    return SmtSolver::CheckResult::Unknown;
}

std::unordered_map<storm::expressions::Variable, cvc5::Term> Cvc5SmtSolver::collectVariableValues() const {
    std::unordered_map<storm::expressions::Variable, cvc5::Term> variableValues;
    for (auto const& variable : this->getManager().getVariables()) {
        if (variable.getType().isBooleanType() || variable.getType().isIntegerType() || variable.getType().isRationalType()) {
            cvc5::Term value;
            try {
                value = this->solver->getValue(this->expressionAdapter->translateExpression(variable));
            } catch (std::exception const&) {
                // If CVC5 cannot provide a value for the variable, we simply do not add it to the model.
                continue;
            }
            variableValues.insert({variable, value});
        }
    }
    return variableValues;
}

cvc5::Term Cvc5SmtSolver::createModelExpression(std::vector<storm::expressions::Variable> const& variables) const {
    // The model covers all variables of the manager. A variable that never occurred in an assertion gets the default
    // value CVC5 reports for it, so ruling out a model also rules out that value (as in a complete enumeration over
    // the declared variables).
    auto const modelValues = this->collectVariableValues();

    cvc5::Term modelExpression = this->termManager->mkBoolean(true);
    for (auto const& variable : variables) {
        auto const& modelValue = modelValues.find(variable);
        if (modelValue == modelValues.end()) {
            // CVC5 could not provide a value for this variable, so it is not part of the model expression.
            continue;
        }
        cvc5::Term variableTerm = this->expressionAdapter->translateExpression(variable);
        modelExpression =
            this->termManager->mkTerm(cvc5::Kind::AND, {modelExpression, this->termManager->mkTerm(cvc5::Kind::EQUAL, {variableTerm, modelValue->second})});
    }
    return modelExpression;
}
#endif

}  // namespace solver
}  // namespace storm

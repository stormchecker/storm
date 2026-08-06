#include "storm/solver/MathsatSmtSolver.h"

#include "storm/exceptions/InvalidStateException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace solver {

#ifdef STORM_HAVE_MATHSAT
MathsatSmtSolver::MathsatAllsatModelReference::MathsatAllsatModelReference(
    storm::expressions::ExpressionManager const& manager, msat_env const& env, msat_term* model,
    std::unordered_map<storm::expressions::Variable, uint_fast64_t> const& variableToSlotMapping)
    : ModelReference(manager), env(env), model(model), variableToSlotMapping(variableToSlotMapping) {
    // Intentionally left empty.
}

bool MathsatSmtSolver::MathsatAllsatModelReference::getBooleanValue(storm::expressions::Variable const& variable) const {
    std::unordered_map<storm::expressions::Variable, uint_fast64_t>::const_iterator variableSlotPair = variableToSlotMapping.find(variable);
    STORM_LOG_THROW(variableSlotPair != variableToSlotMapping.end(), storm::exceptions::InvalidArgumentException,
                    "Cannot retrieve value of unknown variable '" << variable.getName() << "' from model.");
    msat_term selectedTerm = model[variableSlotPair->second];

    if (msat_term_is_not(env, selectedTerm)) {
        return false;
    } else {
        return true;
    }
}

int_fast64_t MathsatSmtSolver::MathsatAllsatModelReference::getIntegerValue(storm::expressions::Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unable to retrieve integer value from model that only contains boolean values.");
}

double MathsatSmtSolver::MathsatAllsatModelReference::getRationalValue(storm::expressions::Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unable to retrieve double value from model that only contains boolean values.");
}

std::string MathsatSmtSolver::MathsatAllsatModelReference::toString() const {
    std::stringstream str;
    bool first = true;
    str << "[";
    for (auto const& varSlot : variableToSlotMapping) {
        if (first) {
            first = false;
        } else {
            str << ", ";
        }
        str << varSlot.first.getName() << "=" << std::boolalpha << getBooleanValue(varSlot.first);
    }
    str << "]";
    return str.str();
}

MathsatSmtSolver::MathsatModelReference::MathsatModelReference(storm::expressions::ExpressionManager const& manager, msat_env const& env,
                                                               storm::adapters::MathsatExpressionAdapter& expressionAdapter)
    : ModelReference(manager), env(env), expressionAdapter(expressionAdapter) {
    // Intentionally left empty.
}

bool MathsatSmtSolver::MathsatModelReference::getBooleanValue(storm::expressions::Variable const& variable) const {
    STORM_LOG_ASSERT(variable.hasBooleanType(), "Variable is non-boolean type.");
    msat_term msatVariable = expressionAdapter.translateExpression(variable);
    msat_term msatValue = msat_get_model_value(env, msatVariable);
    STORM_LOG_ASSERT(
        !MSAT_ERROR_TERM(msatValue),
        "Unable to retrieve value of variable in model. This could be caused by calls to the solver between checking for satisfiability and model retrieval.");
    storm::expressions::Expression value = expressionAdapter.translateExpression(msatValue);
    return value.evaluateAsBool();
}

int_fast64_t MathsatSmtSolver::MathsatModelReference::getIntegerValue(storm::expressions::Variable const& variable) const {
    STORM_LOG_ASSERT(variable.hasIntegerType(), "Variable is non-boolean type.");
    msat_term msatVariable = expressionAdapter.translateExpression(variable);
    msat_term msatValue = msat_get_model_value(env, msatVariable);
    STORM_LOG_ASSERT(
        !MSAT_ERROR_TERM(msatValue),
        "Unable to retrieve value of variable in model. This could be caused by calls to the solver between checking for satisfiability and model retrieval.");
    storm::expressions::Expression value = expressionAdapter.translateExpression(msatValue);
    return value.evaluateAsInt();
}

double MathsatSmtSolver::MathsatModelReference::getRationalValue(storm::expressions::Variable const& variable) const {
    STORM_LOG_ASSERT(variable.hasRationalType(), "Variable is non-boolean type.");
    msat_term msatVariable = expressionAdapter.translateExpression(variable);
    msat_term msatValue = msat_get_model_value(env, msatVariable);
    STORM_LOG_ASSERT(
        !MSAT_ERROR_TERM(msatValue),
        "Unable to retrieve value of variable in model. This could be caused by calls to the solver between checking for satisfiability and model retrieval.");
    storm::expressions::Expression value = expressionAdapter.translateExpression(msatValue);
    return value.evaluateAsDouble();
}

std::string MathsatSmtSolver::MathsatModelReference::toString() const {
    std::stringstream str;
    bool first = true;
    str << "[";
    for (auto const& varDecl : expressionAdapter.getAllDeclaredVariables()) {
        if (first) {
            first = false;
        } else {
            str << ", ";
        }
        msat_term msatValue = msat_get_model_value(env, expressionAdapter.translateExpression(varDecl.first));
        STORM_LOG_ASSERT(!MSAT_ERROR_TERM(msatValue),
                         "Unable to retrieve value of variable in model. This could be caused by calls to the solver between checking for satisfiability and "
                         "model retrieval.");
        str << varDecl.first.getName() << "=" << expressionAdapter.translateExpression(msatValue);
    }
    str << "]";
    return str.str();
}

#endif

MathsatSmtSolver::MathsatSmtSolver(storm::expressions::ExpressionManager& manager, Options const& options)
    : SmtSolver(manager)
#ifdef STORM_HAVE_MATHSAT
      ,
      expressionAdapter(nullptr),
      lastCheckAssumptions(false),
      lastResult(CheckResult::Unknown)
#endif
{
#ifdef STORM_HAVE_MATHSAT
    msat_config config = msat_create_config();
    if (options.enableInterpolantGeneration) {
        msat_set_option(config, "interpolation", "true");
    }
    if (options.enableModelGeneration) {
        msat_set_option(config, "model_generation", "true");
    }
    if (options.enableUnsatCoreGeneration) {
        msat_set_option(config, "unsat_core_generation", "true");
    }
    STORM_LOG_THROW(!MSAT_ERROR_CONFIG(config), storm::exceptions::UnexpectedException, "Unable to create Mathsat configuration.");

    // Based on the configuration, build the environment, check for errors and destroy the configuration.
    env = msat_create_env(config);
    STORM_LOG_THROW(!MSAT_ERROR_ENV(env), storm::exceptions::UnexpectedException, "Unable to create Mathsat environment.");
    msat_destroy_config(config);

    expressionAdapter = std::make_unique<storm::adapters::MathsatExpressionAdapter>(manager, env);
#else
    (void)options;
#endif
}

MathsatSmtSolver::~MathsatSmtSolver() {
#ifdef STORM_HAVE_MATHSAT
    if (!MSAT_ERROR_ENV(env)) {
        msat_destroy_env(env);
    } else {
        STORM_LOG_ERROR("Trying to destroy illegal MathSAT environment.");
    }
#else
    // Empty.
#endif
}

void MathsatSmtSolver::push() {
#ifdef STORM_HAVE_MATHSAT
    msat_push_backtrack_point(env);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

void MathsatSmtSolver::pop() {
#ifdef STORM_HAVE_MATHSAT
    msat_pop_backtrack_point(env);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

void MathsatSmtSolver::pop(uint_fast64_t n) {
#ifdef STORM_HAVE_MATHSAT
    SmtSolver::pop(n);
#else
    (void)n;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

void MathsatSmtSolver::reset() {
#ifdef STORM_HAVE_MATHSAT
    msat_reset_env(env);
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

void MathsatSmtSolver::add(storm::expressions::Expression const& e) {
#ifdef STORM_HAVE_MATHSAT
    msat_term expression = expressionAdapter->translateExpression(e);
    msat_assert_formula(env, expression);
    if (expressionAdapter->hasAdditionalConstraints()) {
        for (auto const& constraint : expressionAdapter->getAdditionalConstraints()) {
            msat_assert_formula(env, constraint);
        }
    }
#else
    (void)e;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

SmtSolver::CheckResult MathsatSmtSolver::check() {
#ifdef STORM_HAVE_MATHSAT
    lastCheckAssumptions = false;
    switch (msat_solve(env)) {
        case MSAT_SAT:
            this->lastResult = SmtSolver::CheckResult::Sat;
            break;
        case MSAT_UNSAT:
            this->lastResult = SmtSolver::CheckResult::Unsat;
            break;
        default:
            this->lastResult = SmtSolver::CheckResult::Unknown;
            break;
    }
    return this->lastResult;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

SmtSolver::CheckResult MathsatSmtSolver::checkWithAssumptions(std::set<storm::expressions::Expression> const& assumptions) {
#ifdef STORM_HAVE_MATHSAT
    lastCheckAssumptions = true;
    std::vector<msat_term> mathSatAssumptions;
    mathSatAssumptions.reserve(assumptions.size());

    for (storm::expressions::Expression assumption : assumptions) {
        mathSatAssumptions.push_back(this->expressionAdapter->translateExpression(assumption));
    }

    switch (msat_solve_with_assumptions(env, mathSatAssumptions.data(), mathSatAssumptions.size())) {
        case MSAT_SAT:
            this->lastResult = SmtSolver::CheckResult::Sat;
            break;
        case MSAT_UNSAT:
            this->lastResult = SmtSolver::CheckResult::Unsat;
            break;
        default:
            this->lastResult = SmtSolver::CheckResult::Unknown;
            break;
    }
    return this->lastResult;
#else
    (void)assumptions;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

SmtSolver::CheckResult MathsatSmtSolver::checkWithAssumptions(std::initializer_list<storm::expressions::Expression> const& assumptions) {
#ifdef STORM_HAVE_MATHSAT
    lastCheckAssumptions = true;
    std::vector<msat_term> mathSatAssumptions;
    mathSatAssumptions.reserve(assumptions.size());

    for (storm::expressions::Expression assumption : assumptions) {
        mathSatAssumptions.push_back(this->expressionAdapter->translateExpression(assumption));
    }

    switch (msat_solve_with_assumptions(env, mathSatAssumptions.data(), mathSatAssumptions.size())) {
        case MSAT_SAT:
            this->lastResult = SmtSolver::CheckResult::Sat;
            break;
        case MSAT_UNSAT:
            this->lastResult = SmtSolver::CheckResult::Unsat;
            break;
        default:
            this->lastResult = SmtSolver::CheckResult::Unknown;
            break;
    }
    return this->lastResult;
#else
    (void)assumptions;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

storm::expressions::SimpleValuation MathsatSmtSolver::getModelAsValuation() {
#ifdef STORM_HAVE_MATHSAT
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");
    return this->convertMathsatModelToValuation();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

std::shared_ptr<SmtSolver::ModelReference> MathsatSmtSolver::getModel() {
#ifdef STORM_HAVE_MATHSAT
    STORM_LOG_THROW(this->lastResult == SmtSolver::CheckResult::Sat, storm::exceptions::InvalidStateException,
                    "Unable to create model for formula that was not determined to be satisfiable.");
    return std::shared_ptr<SmtSolver::ModelReference>(new MathsatModelReference(this->getManager(), env, *expressionAdapter));
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

#ifdef STORM_HAVE_MATHSAT
storm::expressions::SimpleValuation MathsatSmtSolver::convertMathsatModelToValuation() {
    storm::expressions::SimpleValuation stormModel(this->getManager().getSharedPointer());

    msat_model_iterator modelIterator = msat_create_model_iterator(env);
    STORM_LOG_THROW(!MSAT_ERROR_MODEL_ITERATOR(modelIterator), storm::exceptions::UnexpectedException, "MathSat returned an illegal model iterator.");

    while (msat_model_iterator_has_next(modelIterator)) {
        msat_term t, v;
        msat_model_iterator_next(modelIterator, &t, &v);

        storm::expressions::Expression variableInterpretation = this->expressionAdapter->translateExpression(v);
        storm::expressions::Variable stormVariable = this->expressionAdapter->getVariable(msat_term_get_decl(t));

        if (stormVariable.hasBooleanType()) {
            stormModel.setBooleanValue(stormVariable, variableInterpretation.isTrue());
        } else if (stormVariable.hasIntegerType()) {
            stormModel.setIntegerValue(stormVariable, variableInterpretation.evaluateAsInt());
        } else if (stormVariable.hasRationalType()) {
            stormModel.setRationalValue(stormVariable, variableInterpretation.evaluateAsDouble());
        } else {
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException, "Variable interpretation in model is not of type bool, int or rational.");
        }
    }

    return stormModel;
}
#endif

std::vector<storm::expressions::SimpleValuation> MathsatSmtSolver::allSat(std::vector<storm::expressions::Variable> const& important) {
#ifdef STORM_HAVE_MATHSAT
    std::vector<storm::expressions::SimpleValuation> valuations;
    this->allSat(important, [&valuations](storm::expressions::SimpleValuation const& valuation) -> bool {
        valuations.push_back(valuation);
        return true;
    });
    return valuations;
#else
    (void)important;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

#ifdef STORM_HAVE_MATHSAT
class AllsatValuationCallbackUserData {
   public:
    AllsatValuationCallbackUserData(storm::expressions::ExpressionManager const& manager, storm::adapters::MathsatExpressionAdapter& adapter, msat_env& env,
                                    std::function<bool(storm::expressions::SimpleValuation&)> const& callback)
        : manager(manager), adapter(adapter), env(env), callback(callback) {
        // Intentionally left empty.
    }

    static int allsatValuationsCallback(msat_term* model, int size, void* user_data) {
        AllsatValuationCallbackUserData* user = reinterpret_cast<AllsatValuationCallbackUserData*>(user_data);

        storm::expressions::SimpleValuation valuation(user->manager.getSharedPointer());
        for (int i = 0; i < size; ++i) {
            bool currentTermValue = true;
            msat_term currentTerm = model[i];
            if (msat_term_is_not(user->env, currentTerm)) {
                currentTerm = msat_term_get_arg(currentTerm, 0);
                currentTermValue = false;
            }
            storm::expressions::Variable stormVariable = user->adapter.getVariable(msat_term_get_decl(currentTerm));
            valuation.setBooleanValue(stormVariable, currentTermValue);
        }

        if (user->callback(valuation)) {
            return 1;
        } else {
            return 0;
        }
    }

   protected:
    // The manager responsible for the expression.s
    storm::expressions::ExpressionManager const& manager;

    // The adapter to use for expression translation.
    storm::adapters::MathsatExpressionAdapter& adapter;

    // The MathSAT environment. It is used to retrieve the values of the atoms in a model.
    msat_env& env;

    // The function that is to be called when the MathSAT model has been translated to a valuation.
    std::function<bool(storm::expressions::SimpleValuation&)> const& callback;
};

class AllsatModelReferenceCallbackUserData {
   public:
    AllsatModelReferenceCallbackUserData(storm::expressions::ExpressionManager const& manager, msat_env& env,
                                         std::unordered_map<storm::expressions::Variable, uint_fast64_t> const& atomToSlotMapping,
                                         std::function<bool(storm::solver::SmtSolver::ModelReference&)> const& callback)
        : manager(manager), env(env), atomToSlotMapping(atomToSlotMapping), callback(callback) {
        // Intentionally left empty.
    }

    static int allsatModelReferenceCallback(msat_term* model, int, void* user_data) {
        AllsatModelReferenceCallbackUserData* user = reinterpret_cast<AllsatModelReferenceCallbackUserData*>(user_data);
        MathsatSmtSolver::MathsatAllsatModelReference modelReference(user->manager, user->env, model, user->atomToSlotMapping);
        if (user->callback(modelReference)) {
            return 1;
        } else {
            return 0;
        }
    }

   protected:
    // The manager responsible for the expression.s
    storm::expressions::ExpressionManager const& manager;

    // The MathSAT environment. It is used to retrieve the values of the atoms in a model.
    msat_env& env;

    // Store a mapping from atoms to their slots in the model.
    std::unordered_map<storm::expressions::Variable, uint_fast64_t> const& atomToSlotMapping;

    // The function that is to be called when the MathSAT model has been translated to a valuation.
    std::function<bool(storm::solver::SmtSolver::ModelReference&)> const& callback;
};
#endif

uint_fast64_t MathsatSmtSolver::allSat(std::vector<storm::expressions::Variable> const& important,
                                       std::function<bool(storm::expressions::SimpleValuation&)> const& callback) {
#ifdef STORM_HAVE_MATHSAT
    // Create a backtracking point, because MathSAT will modify the assertions stack during its AllSat procedure.
    this->push();

    std::vector<msat_term> msatImportant;
    msatImportant.reserve(important.size());

    for (storm::expressions::Variable const& variable : important) {
        STORM_LOG_THROW(variable.hasBooleanType(), storm::exceptions::InvalidArgumentException, "The important atoms for AllSat must be boolean variables.");
        msatImportant.push_back(expressionAdapter->translateExpression(variable));
    }

    AllsatValuationCallbackUserData allSatUserData(this->getManager(), *expressionAdapter, env, callback);
    int numberOfModels =
        msat_all_sat(env, msatImportant.data(), msatImportant.size(), &AllsatValuationCallbackUserData::allsatValuationsCallback, &allSatUserData);

    // Restore original assertion stack and return.
    this->pop();
    return static_cast<uint_fast64_t>(numberOfModels);
#else
    (void)important;
    (void)callback;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

uint_fast64_t MathsatSmtSolver::allSat(std::vector<storm::expressions::Variable> const& important,
                                       std::function<bool(SmtSolver::ModelReference&)> const& callback) {
#ifdef STORM_HAVE_MATHSAT
    // Create a backtracking point, because MathSAT will modify the assertions stack during its AllSat procedure.
    this->push();

    std::vector<msat_term> msatImportant;
    msatImportant.reserve(important.size());
    std::unordered_map<storm::expressions::Variable, uint_fast64_t> atomToSlotMapping;

    for (storm::expressions::Variable const& variable : important) {
        STORM_LOG_THROW(variable.hasBooleanType(), storm::exceptions::InvalidArgumentException, "The important atoms for AllSat must be boolean variables.");
        msatImportant.push_back(expressionAdapter->translateExpression(variable));
        atomToSlotMapping[variable] = msatImportant.size() - 1;
    }

    AllsatModelReferenceCallbackUserData allSatUserData(this->getManager(), env, atomToSlotMapping, callback);
    int numberOfModels =
        msat_all_sat(env, msatImportant.data(), msatImportant.size(), &AllsatModelReferenceCallbackUserData::allsatModelReferenceCallback, &allSatUserData);

    // Restore original assertion stack and return.
    this->pop();
    return static_cast<uint_fast64_t>(numberOfModels);
#else
    (void)important;
    (void)callback;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

std::vector<storm::expressions::Expression> MathsatSmtSolver::getUnsatAssumptions() {
#ifdef STORM_HAVE_MATHSAT
    STORM_LOG_THROW(lastResult == SmtSolver::CheckResult::Unsat, storm::exceptions::InvalidStateException,
                    "Unable to generate unsatisfiable core of assumptions, because the last check did not determine the formulas to be unsatisfiable.");
    STORM_LOG_THROW(lastCheckAssumptions, storm::exceptions::InvalidStateException,
                    "Unable to generate unsatisfiable core of assumptions, because the last check did not involve assumptions.");

    size_t numUnsatAssumpations;
    msat_term* msatUnsatAssumptions = msat_get_unsat_assumptions(env, &numUnsatAssumpations);

    std::vector<storm::expressions::Expression> unsatAssumptions;
    unsatAssumptions.reserve(numUnsatAssumpations);

    for (unsigned int i = 0; i < numUnsatAssumpations; ++i) {
        unsatAssumptions.push_back(this->expressionAdapter->translateExpression(msatUnsatAssumptions[i]));
    }

    return unsatAssumptions;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

void MathsatSmtSolver::setInterpolationGroup(uint64_t group) {
#ifdef STORM_HAVE_MATHSAT
    auto groupIter = this->interpolationGroups.find(group);
    if (groupIter == this->interpolationGroups.end()) {
        int newGroup = msat_create_itp_group(env);
        auto insertResult = this->interpolationGroups.insert(std::make_pair(group, newGroup));
        groupIter = insertResult.first;
    }
    msat_set_itp_group(env, groupIter->second);
#else
    (void)group;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}

storm::expressions::Expression MathsatSmtSolver::getInterpolant(std::vector<uint64_t> const& groupsA) {
#ifdef STORM_HAVE_MATHSAT
    STORM_LOG_THROW(lastResult == SmtSolver::CheckResult::Unsat, storm::exceptions::InvalidStateException,
                    "Unable to generate interpolant, because the last check did not determine the formulas to be unsatisfiable.");
    STORM_LOG_THROW(!lastCheckAssumptions, storm::exceptions::InvalidStateException,
                    "Unable to generate interpolant, because the last check for satisfiability involved assumptions.");

    std::vector<int> msatInterpolationGroupsA;
    msatInterpolationGroupsA.reserve(groupsA.size());
    for (auto groupOfA : groupsA) {
        auto groupIter = this->interpolationGroups.find(groupOfA);
        STORM_LOG_THROW(groupIter != this->interpolationGroups.end(), storm::exceptions::InvalidArgumentException,
                        "Unable to generate interpolant, because an unknown interpolation group was referenced.");
        msatInterpolationGroupsA.push_back(groupIter->second);
    }
    msat_term interpolant = msat_get_interpolant(env, msatInterpolationGroupsA.data(), msatInterpolationGroupsA.size());

    STORM_LOG_THROW(!MSAT_ERROR_TERM(interpolant), storm::exceptions::UnexpectedException, "Unable to retrieve an interpolant.");

    return this->expressionAdapter->translateExpression(interpolant);
#else
    (void)groupsA;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without MathSAT support.");
#endif
}
}  // namespace solver
}  // namespace storm

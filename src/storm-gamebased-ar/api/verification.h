#pragma once

#include "storm-gamebased-ar/abstraction/AbstractionOptions.h"
#include "storm-gamebased-ar/modelchecker/abstraction/BisimulationAbstractionRefinementModelChecker.h"
#include "storm-gamebased-ar/modelchecker/abstraction/GameBasedMdpModelChecker.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/macros.h"

namespace storm::gbar {
namespace api {

//
// Verifying with Abstraction Refinement engine
//
using AbstractionRefinementOptions = storm::gbar::abstraction::AbstractionOptions;

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<!std::is_same<ValueType, storm::RationalFunction>::value, std::unique_ptr<storm::modelchecker::CheckResult>>::type
verifyWithAbstractionRefinementEngine(storm::Environment const& env, storm::languages::SymbolicModelDescription const& model,
                                      storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task,
                                      AbstractionRefinementOptions const& options = AbstractionRefinementOptions()) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;
    if (model.getModelType() == storm::languages::SymbolicModelDescription::ModelType::DTMC) {
        storm::gbar::modelchecker::GameBasedMdpModelChecker<DdType, storm::models::symbolic::Dtmc<DdType, ValueType>> modelchecker(model, options);
        if (modelchecker.canHandle(task)) {
            result = modelchecker.check(env, task);
        }
    } else if (model.getModelType() == storm::languages::SymbolicModelDescription::ModelType::MDP) {
        storm::gbar::modelchecker::GameBasedMdpModelChecker<DdType, storm::models::symbolic::Mdp<DdType, ValueType>> modelchecker(model, options);
        if (modelchecker.canHandle(task)) {
            result = modelchecker.check(env, task);
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                        "The model type " << model.getModelType() << " is not supported by the abstraction refinement engine.");
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<std::is_same<ValueType, storm::RationalFunction>::value, std::unique_ptr<storm::modelchecker::CheckResult>>::type
verifyWithAbstractionRefinementEngine(storm::Environment const&, storm::languages::SymbolicModelDescription const&,
                                      storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const&,
                                      AbstractionRefinementOptions const& = AbstractionRefinementOptions()) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Abstraction-refinement engine does not support data type.");
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> verifyWithAbstractionRefinementEngine(
    storm::languages::SymbolicModelDescription const& model, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task,
    AbstractionRefinementOptions const& options = AbstractionRefinementOptions()) {
    Environment env;
    return verifyWithAbstractionRefinementEngine<DdType, ValueType>(env, model, task, options);
}

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<std::is_same<ValueType, double>::value, std::unique_ptr<storm::modelchecker::CheckResult>>::type verifyWithAbstractionRefinementEngine(
    storm::Environment const& env, std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model,
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task,
    AbstractionRefinementOptions const& options = AbstractionRefinementOptions()) {
    std::unique_ptr<storm::modelchecker::CheckResult> result;
    model->getManager().execute([&]() {
        if (model->getType() == storm::models::ModelType::Dtmc) {
            storm::gbar::modelchecker::BisimulationAbstractionRefinementModelChecker<storm::models::symbolic::Dtmc<DdType, ValueType>> modelchecker(
                *model->template as<storm::models::symbolic::Dtmc<DdType, double>>(), options);
            if (modelchecker.canHandle(task)) {
                result = modelchecker.check(env, task);
            }
        } else if (model->getType() == storm::models::ModelType::Mdp) {
            storm::gbar::modelchecker::BisimulationAbstractionRefinementModelChecker<storm::models::symbolic::Mdp<DdType, ValueType>> modelchecker(
                *model->template as<storm::models::symbolic::Mdp<DdType, double>>(), options);
            if (modelchecker.canHandle(task)) {
                result = modelchecker.check(env, task);
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "The model type " << model->getType() << " is not supported by the abstraction refinement engine.");
        }
    });
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<!std::is_same<ValueType, double>::value, std::unique_ptr<storm::modelchecker::CheckResult>>::type verifyWithAbstractionRefinementEngine(
    storm::Environment const&, std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const&,
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const&, AbstractionRefinementOptions const& = AbstractionRefinementOptions()) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Abstraction-refinement engine does not support data type.");
}

template<storm::dd::DdType DdType, typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> verifyWithAbstractionRefinementEngine(
    std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model,
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task,
    AbstractionRefinementOptions const& options = AbstractionRefinementOptions()) {
    Environment env;
    return verifyWithAbstractionRefinementEngine<DdType, ValueType>(env, model, task, options);
}

}  // namespace api
}  // namespace storm::gbar

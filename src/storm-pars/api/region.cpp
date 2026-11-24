#include "storm-pars/api/region.h"

#include <memory>

#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/modelchecker/region/SparseMdpParameterLiftingModelChecker.h"
#include "storm-pars/modelchecker/region/ValidatingSparseParameterLiftingModelChecker.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"

namespace storm {
namespace api {

template<typename ParametricType, typename ImpreciseType, typename PreciseType>
std::unique_ptr<storm::modelchecker::RegionModelChecker<ParametricType>> createRegionModelChecker(storm::modelchecker::RegionCheckEngine engine,
                                                                                                  storm::models::ModelType modelType) {
    STORM_LOG_THROW(modelType == storm::models::ModelType::Dtmc || modelType == storm::models::ModelType::Mdp, storm::exceptions::NotSupportedException,
                    "Unable to create a region checker for the provided model type.");

    switch (engine) {
        case storm::modelchecker::RegionCheckEngine::ParameterLifting:
            if (modelType == storm::models::ModelType::Dtmc) {
                return std::make_unique<
                    storm::modelchecker::SparseDtmcParameterLiftingModelChecker<storm::models::sparse::Dtmc<ParametricType>, ImpreciseType>>();
            } else {
                return std::make_unique<
                    storm::modelchecker::SparseMdpParameterLiftingModelChecker<storm::models::sparse::Mdp<ParametricType>, ImpreciseType>>();
            }
        case storm::modelchecker::RegionCheckEngine::ExactParameterLifting:
            if (modelType == storm::models::ModelType::Dtmc) {
                return std::make_unique<
                    storm::modelchecker::SparseDtmcParameterLiftingModelChecker<storm::models::sparse::Dtmc<ParametricType>, PreciseType>>();
            } else {
                return std::make_unique<storm::modelchecker::SparseMdpParameterLiftingModelChecker<storm::models::sparse::Mdp<ParametricType>, PreciseType>>();
            }
        case storm::modelchecker::RegionCheckEngine::RobustParameterLifting:
            return std::make_unique<
                storm::modelchecker::SparseDtmcParameterLiftingModelChecker<storm::models::sparse::Dtmc<ParametricType>, ImpreciseType, true>>();
        case storm::modelchecker::RegionCheckEngine::ValidatingParameterLifting:
            if (modelType == storm::models::ModelType::Dtmc) {
                return std::make_unique<storm::modelchecker::ValidatingSparseParameterLiftingModelChecker<storm::models::sparse::Dtmc<ParametricType>,
                                                                                                          ImpreciseType, PreciseType>>();
            } else {
                return std::make_unique<storm::modelchecker::ValidatingSparseParameterLiftingModelChecker<storm::models::sparse::Mdp<ParametricType>,
                                                                                                          ImpreciseType, PreciseType>>();
            }
        default:
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected region model checker type.");
    }
    return nullptr;
}

template std::unique_ptr<storm::modelchecker::RegionModelChecker<storm::RationalFunction>>
createRegionModelChecker<storm::RationalFunction, double, storm::RationalNumber>(storm::modelchecker::RegionCheckEngine engine,
                                                                                 storm::models::ModelType modelType);

}  // namespace api
}  // namespace storm

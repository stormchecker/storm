#include "storm-pars/modelchecker/instantiation/SparseInstantiationModelChecker.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"

namespace storm {
namespace modelchecker {

template<typename SparseModelType, typename ConstantType>
SparseInstantiationModelChecker<SparseModelType, ConstantType>::SparseInstantiationModelChecker(SparseModelType const& parametricModel)
    : parametricModel(parametricModel), instantiationsAreGraphPreserving(false) {
    // Intentionally left empty
}

template<typename SparseModelType, typename ConstantType>
void SparseInstantiationModelChecker<SparseModelType, ConstantType>::specifyFormula(
    storm::modelchecker::CheckTask<storm::logic::Formula, typename SparseModelType::ValueType> const& checkTask) {
    currentFormula = checkTask.getFormula().asSharedPointer();
    currentCheckTask = std::make_unique<storm::modelchecker::CheckTask<storm::logic::Formula, ConstantType>>(
        checkTask.substituteFormula(*currentFormula).template convertValueType<ConstantType>());
}

template<typename SparseModelType, typename ConstantType>
void SparseInstantiationModelChecker<SparseModelType, ConstantType>::setInstantiationsAreGraphPreserving(bool value) {
    instantiationsAreGraphPreserving = value;
}

template<typename SparseModelType, typename ConstantType>
bool SparseInstantiationModelChecker<SparseModelType, ConstantType>::getInstantiationsAreGraphPreserving() const {
    return instantiationsAreGraphPreserving;
}

template<typename SparseModelType, typename ConstantType>
SparseModelType const& SparseInstantiationModelChecker<SparseModelType, ConstantType>::getOriginalModel() const {
    return parametricModel;
}

template class SparseInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double>;
template class SparseInstantiationModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>, double>;
template class SparseInstantiationModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, double>;

template class SparseInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber>;
template class SparseInstantiationModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>, storm::RationalNumber>;
template class SparseInstantiationModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>, storm::RationalNumber>;

}  // namespace modelchecker
}  // namespace storm

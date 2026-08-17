#include "storm/storage/dd/bisimulation/SignatureRefiner.h"

#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/bisimulation/InternalCuddSignatureRefiner.h"
#include "storm/storage/dd/bisimulation/InternalSignatureRefiner.h"
#include "storm/storage/dd/bisimulation/InternalSylvanSignatureRefiner.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
SignatureRefiner<DdType, ValueType>::SignatureRefiner(storm::dd::DdManager<DdType> const& manager, storm::expressions::Variable const& blockVariable,
                                                      std::set<storm::expressions::Variable> const& stateRowVariables,
                                                      std::set<storm::expressions::Variable> const& stateColumnVariables, bool shiftStateVariables,
                                                      std::set<storm::expressions::Variable> const& nondeterminismVariables,
                                                      BisimulationOptions const& bisimulationOptions)
    : manager(&manager) {
    storm::dd::Bdd<DdType> nonBlockVariablesCube = manager.getBddOne();
    storm::dd::Bdd<DdType> nondeterminismVariablesCube = manager.getBddOne();
    for (auto const& var : nondeterminismVariables) {
        auto cube = manager.getMetaVariable(var).getCube();
        nonBlockVariablesCube &= cube;
        nondeterminismVariablesCube &= cube;
    }
    for (auto const& var : stateRowVariables) {
        auto cube = manager.getMetaVariable(var).getCube();
        nonBlockVariablesCube &= cube;
    }

    internalRefiner = std::make_unique<InternalSignatureRefiner<DdType, ValueType>>(
        manager, blockVariable, shiftStateVariables ? stateColumnVariables : stateRowVariables, nondeterminismVariablesCube, nonBlockVariablesCube,
        InternalSignatureRefinerOptions(shiftStateVariables, bisimulationOptions));
}

template<storm::dd::DdType DdType, typename ValueType>
Partition<DdType, ValueType> SignatureRefiner<DdType, ValueType>::refine(Partition<DdType, ValueType> const& oldPartition,
                                                                         Signature<DdType, ValueType> const& signature) {
    Partition<DdType, ValueType> result = internalRefiner->refine(oldPartition, signature);
    return result;
}

template class SignatureRefiner<storm::dd::DdType::CUDD, double>;

template class SignatureRefiner<storm::dd::DdType::Sylvan, double>;
template class SignatureRefiner<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SignatureRefiner<storm::dd::DdType::Sylvan, storm::RationalFunction>;

}  // namespace bisimulation
}  // namespace dd
}  // namespace storm

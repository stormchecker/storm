#pragma once

#include "storm/logic/Formula.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/storage/dd/Odd.h"

namespace storm {
namespace transformer {

template<storm::dd::DdType Type, typename ValueType>
class SymbolicDtmcToSparseDtmcTransformer {
   public:
    std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> translate(
        storm::models::symbolic::Dtmc<Type, ValueType> const& symbolicDtmc,
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas = std::vector<std::shared_ptr<storm::logic::Formula const>>());
    storm::dd::Odd const& getOdd() const;

   private:
    storm::dd::Odd odd;
};

template<storm::dd::DdType Type, typename ValueType>
class SymbolicMdpToSparseMdpTransformer {
   public:
    static std::shared_ptr<storm::models::sparse::Mdp<ValueType>> translate(
        storm::models::symbolic::Mdp<Type, ValueType> const& symbolicMdp,
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas = std::vector<std::shared_ptr<storm::logic::Formula const>>());
};

template<storm::dd::DdType Type, typename ValueType>
class SymbolicCtmcToSparseCtmcTransformer {
   public:
    static std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> translate(
        storm::models::symbolic::Ctmc<Type, ValueType> const& symbolicCtmc,
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas = std::vector<std::shared_ptr<storm::logic::Formula const>>());
};

template<storm::dd::DdType Type, typename ValueType>
class SymbolicMaToSparseMaTransformer {
   public:
    static std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> translate(
        storm::models::symbolic::MarkovAutomaton<Type, ValueType> const& symbolicMa,
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas = std::vector<std::shared_ptr<storm::logic::Formula const>>());
};
}  // namespace transformer
}  // namespace storm

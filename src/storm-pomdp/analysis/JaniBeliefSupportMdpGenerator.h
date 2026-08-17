#pragma once
#include "storm/environment/Environment.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/jani/Model.h"

namespace storm {

namespace pomdp {

namespace qualitative {
template<typename ValueType>
class JaniBeliefSupportMdpGenerator {
   public:
    JaniBeliefSupportMdpGenerator(storm::models::sparse::Pomdp<ValueType> const& pomdp);
    void generate(storm::storage::BitVector const& targetStates, storm::storage::BitVector const& badStates);
    void verifySymbolic(storm::Environment const& env, bool onlyInitial = true);
    bool isInitialWinning() const;

   private:
    storm::models::sparse::Pomdp<ValueType> const& pomdp;
    jani::Model model;
    bool initialIsWinning = false;
};

}  // namespace qualitative
}  // namespace pomdp
}  // namespace storm
#pragma once

#include "storm/models/sparse/Ctmc.h"

namespace storm {
namespace builder {

/*!
 * Build a parallel composition of Markov chains.
 */
template<typename ValueType>
class ParallelCompositionBuilder {
   public:
    static std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> compose(std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> const& ctmcA,
                                                                           std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> const& ctmcB, bool labelAnd);
};

}  // namespace builder
}  // namespace storm

#include "storm-dft/storage/OrderDFTElementsById.h"

#include "storm-dft/storage/elements/DFTElements.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace storage {

template<typename ValueType>
bool OrderElementsById<ValueType>::operator()(std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType>> const& a,
                                              std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType>> const& b) const {
    return a->id() < b->id();
}

template<typename ValueType>
bool OrderElementsById<ValueType>::operator()(const std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>>& a,
                                              const std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType>>& b) const {
    return a->id() < b->id();
}

template<typename ValueType>
bool OrderElementsByRank<ValueType>::operator()(const std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType>>& a,
                                                const std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType>>& b) const {
    return a->rank() > b->rank();
}

// Explicitly instantiate the class.
template struct OrderElementsById<double>;
template struct OrderElementsByRank<double>;

template struct OrderElementsById<storm::RationalFunction>;
template struct OrderElementsByRank<storm::RationalFunction>;

}  // namespace storage
}  // namespace storm::dft

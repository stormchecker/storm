#include "storm-dft/storage/elements/DFTBE.h"

#include "storm-dft/storage/elements/DFTDependency.h"
#include "storm-dft/storage/elements/DFTGate.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::dft {
namespace storage {
namespace elements {

template<typename ValueType>
void DFTBE<ValueType>::extendSubDft(std::set<size_t>& elemsInSubtree, std::vector<size_t> const& parentsOfSubRoot, bool blockParents,
                                    bool sparesAsLeaves) const {
    if (elemsInSubtree.count(this->id())) {
        return;
    }
    DFTElement<ValueType>::extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
    if (elemsInSubtree.empty()) {
        // Parent in the subDFT, i.e., it is *not* a subDFT
        return;
    }
    for (auto const& inDep : ingoingDependencies()) {
        inDep->extendSubDft(elemsInSubtree, parentsOfSubRoot, blockParents, sparesAsLeaves);
        if (elemsInSubtree.empty()) {
            // Parent in the subDFT, i.e., it is *not* a subDFT
            return;
        }
    }
}

// Explicitly instantiate the class.
template class DFTBE<double>;
template class DFTBE<RationalFunction>;

}  // namespace elements
}  // namespace storage
}  // namespace storm::dft

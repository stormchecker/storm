#include "storm/models/sparse/DeterministicModel.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/io/export.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/constants.h"

namespace storm {
namespace models {
namespace sparse {

template<typename ValueType, typename RewardModelType>
DeterministicModel<ValueType, RewardModelType>::DeterministicModel(ModelType modelType,
                                                                   storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components)
    : Model<ValueType, RewardModelType>(modelType, components) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
DeterministicModel<ValueType, RewardModelType>::DeterministicModel(ModelType modelType,
                                                                   storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components)
    : Model<ValueType, RewardModelType>(modelType, std::move(components)) {
    // Intentionally left empty
}

template<typename ValueType, typename RewardModelType>
void DeterministicModel<ValueType, RewardModelType>::writeDotToStream(std::ostream& outStream, size_t maxWidthLabel, bool includeLabeling,
                                                                      storm::storage::BitVector const* subsystem, std::vector<ValueType> const* firstValue,
                                                                      std::vector<ValueType> const* secondValue,
                                                                      std::vector<uint_fast64_t> const* stateColoring, std::vector<std::string> const* colors,
                                                                      std::vector<uint_fast64_t>* scheduler, bool finalizeOutput) const {
    Model<ValueType, RewardModelType>::writeDotToStream(outStream, maxWidthLabel, includeLabeling, subsystem, firstValue, secondValue, stateColoring, colors,
                                                        scheduler, false);

    // iterate over all transitions and draw the arrows with probability information attached.
    auto rowIt = this->getTransitionMatrix().begin();
    for (uint_fast64_t i = 0; i < this->getTransitionMatrix().getRowCount(); ++i, ++rowIt) {
        // Put in an intermediate node if there is a choice labeling
        std::string arrowOrigin = std::to_string(i);
        if (this->hasChoiceLabeling()) {
            arrowOrigin = "\"" + arrowOrigin + "c\"";
            outStream << "\t" << arrowOrigin << " [shape = \"point\"]\n";
            outStream << "\t" << i << " -> " << arrowOrigin << " [label= \"{";
            storm::io::outputFixedWidth(outStream, this->getChoiceLabeling().getLabelsOfChoice(i), maxWidthLabel);
            outStream << "}\"];\n";
        }

        typename storm::storage::SparseMatrix<ValueType>::const_rows row = this->getTransitionMatrix().getRow(i);
        for (auto const& transition : row) {
            if (transition.getValue() != storm::utility::zero<ValueType>()) {
                if (subsystem == nullptr || subsystem->get(transition.getColumn())) {
                    outStream << "\t" << arrowOrigin << " -> " << transition.getColumn() << " [ label= \"" << transition.getValue() << "\" ];\n";
                }
            }
        }
    }

    if (finalizeOutput) {
        outStream << "}\n";
    }
}

template class DeterministicModel<double>;
template class DeterministicModel<double, storm::models::sparse::StandardRewardModel<storm::Interval>>;
template class DeterministicModel<storm::RationalNumber>;
template class DeterministicModel<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>;
template class DeterministicModel<storm::Interval>;
template class DeterministicModel<storm::RationalInterval>;
template class DeterministicModel<storm::RationalFunction>;
}  // namespace sparse
}  // namespace models
}  // namespace storm

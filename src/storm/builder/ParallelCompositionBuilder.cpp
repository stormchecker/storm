#include "storm/builder/ParallelCompositionBuilder.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm {
namespace builder {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> ParallelCompositionBuilder<ValueType>::compose(
    std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> const& ctmcA, std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> const& ctmcB, bool labelAnd) {
    STORM_LOG_TRACE("Parallel composition");

    storm::storage::SparseMatrix<ValueType> matrixA = ctmcA->getTransitionMatrix();
    storm::storage::SparseMatrix<ValueType> matrixB = ctmcB->getTransitionMatrix();
    storm::models::sparse::StateLabeling labelingA = ctmcA->getStateLabeling();
    storm::models::sparse::StateLabeling labelingB = ctmcB->getStateLabeling();
    size_t sizeA = ctmcA->getNumberOfStates();
    size_t sizeB = ctmcB->getNumberOfStates();
    size_t size = sizeA * sizeB;
    size_t rowIndex = 0;

    // Build matrix
    storm::storage::SparseMatrixBuilder<ValueType> builder(size, size, 0, true, false, 0);

    for (size_t stateA = 0; stateA < sizeA; ++stateA) {
        for (size_t stateB = 0; stateB < sizeB; ++stateB) {
            STORM_LOG_ASSERT(rowIndex == stateA * sizeB + stateB, "Row " << rowIndex << " is not correct");

            auto rowA = matrixA.getRow(stateA);
            auto itA = rowA.begin();
            auto rowB = matrixB.getRow(stateB);
            auto itB = rowB.begin();

            // First consider all target states of A < the current stateA
            while (itA != rowA.end() && itA->getColumn() < stateA) {
                builder.addNextValue(rowIndex, itA->getColumn() * sizeB + stateB, itA->getValue());
                ++itA;
            }

            // Then consider all target states of B
            while (itB != rowB.end()) {
                builder.addNextValue(rowIndex, stateA * sizeB + itB->getColumn(), itB->getValue());
                ++itB;
            }

            // Last consider all remaining target states of A > the current stateA
            while (itA != rowA.end()) {
                builder.addNextValue(rowIndex, itA->getColumn() * sizeB + stateB, itA->getValue());
                ++itA;
            }

            ++rowIndex;
        }
    }

    storm::storage::SparseMatrix<ValueType> matrixComposed = builder.build();
    STORM_LOG_ASSERT(matrixComposed.getRowCount() == size, "Row count is not correct.");
    STORM_LOG_ASSERT(matrixComposed.getColumnCount() == size, "Column count is not correct.");

    // Build labeling
    storm::models::sparse::StateLabeling labeling = storm::models::sparse::StateLabeling(sizeA * sizeB);

    if (labelAnd) {
        for (std::string const& label : labelingA.getLabels()) {
            if (labelingB.containsLabel(label)) {
                // Only consider labels contained in both CTMCs
                storm::storage::BitVector labelStates(size, false);
                for (auto entryA : labelingA.getStates(label)) {
                    for (auto entryB : labelingB.getStates(label)) {
                        labelStates.set(entryA * sizeB + entryB);
                    }
                }
                labeling.addLabel(label, labelStates);
            }
        }
    } else {
        // Set labels from A
        for (std::string const& label : labelingA.getLabels()) {
            if (label == "init") {
                // Initial states must be initial in both CTMCs
                STORM_LOG_ASSERT(labelingB.containsLabel(label), "B does not have init.");
                storm::storage::BitVector labelStates(size, false);
                for (auto entryA : labelingA.getStates(label)) {
                    for (auto entryB : labelingB.getStates(label)) {
                        labelStates.set(entryA * sizeB + entryB);
                    }
                }
                labeling.addLabel(label, labelStates);
            } else {
                storm::storage::BitVector labelStates(size, false);
                for (auto entry : labelingA.getStates(label)) {
                    for (size_t index = entry * sizeB; index < entry * sizeB + sizeB; ++index) {
                        labelStates.set(index, true);
                    }
                }
                labeling.addLabel(label, labelStates);
            }
        }
        // Set labels from B
        for (std::string const& label : labelingB.getLabels()) {
            if (label == "init") {
                continue;
            }
            if (labeling.containsLabel(label)) {
                // Label is already there from A
                for (auto entry : labelingB.getStates(label)) {
                    for (size_t index = 0; index < sizeA; ++index) {
                        labeling.addLabelToState(label, index * sizeB + entry);
                    }
                }
            } else {
                storm::storage::BitVector labelStates(size, false);
                for (auto entry : labelingB.getStates(label)) {
                    for (size_t index = 0; index < sizeA; ++index) {
                        labelStates.set(index * sizeB + entry, true);
                    }
                }
                labeling.addLabel(label, labelStates);
            }
        }
    }

    // Build CTMC
    std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> composedCtmc = std::make_shared<storm::models::sparse::Ctmc<ValueType>>(matrixComposed, labeling);

    // Print for debugging
    /*std::cout << "Matrix A:\n";
    std::cout << matrixA << '\n';
    std::cout << "Matrix B:\n";
    std::cout << matrixB << '\n';
    std::cout << "Composed matrix: \n" << matrixComposed;
    std::cout << "Labeling A\n";
    labelingA.printLabelingInformationToStream(std::cout);
    for (size_t stateA = 0; stateA < sizeA; ++stateA) {
        std::cout << "State " << stateA << ": ";
        for (auto label : labelingA.getLabelsOfState(stateA)) {
            std::cout << label << ", ";
        }
        std::cout << '\n';
    }
    std::cout << "Labeling B\n";
    labelingB.printLabelingInformationToStream(std::cout);
    for (size_t stateB = 0; stateB < sizeB; ++stateB) {
        std::cout << "State " << stateB << ": ";
        for (auto label : labelingB.getLabelsOfState(stateB)) {
            std::cout << label << ", ";
        }
        std::cout << '\n';
    }
    std::cout << "Labeling Composed\n";
    labeling.printLabelingInformationToStream(std::cout);
    for (size_t state = 0; state < size; ++state) {
        std::cout << "State " << state << ": ";
        for (auto label : labeling.getLabelsOfState(state)) {
            std::cout << label << ", ";
        }
        std::cout << '\n';
    }*/

    return composedCtmc;
}

// Explicitly instantiate the class.
template class ParallelCompositionBuilder<double>;
template class ParallelCompositionBuilder<storm::RationalFunction>;

}  // namespace builder
}  // namespace storm

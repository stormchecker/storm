#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <vector>

#include "storm/logic/Formulas.h"
#include "storm/modelchecker/multiobjective/Objective.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/memorystructure/SparseModelMemoryProduct.h"
#include "storm/storage/memorystructure/SparseModelMemoryProductReverseData.h"

#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {
namespace preprocessing {

template<class SparseModelType>
struct SparseMultiObjectivePreprocessorResult {
    enum class QueryType { Achievability, Quantitative, Pareto };

    // Original data
    storm::logic::MultiObjectiveFormula const& originalFormula;
    SparseModelType const& originalModel;

    // Mapping of incorporated memory to model+memory
    std::optional<storm::storage::SparseModelMemoryProductReverseData> memoryIncorporationReverseData;

    // The preprocessed model and objectives
    std::shared_ptr<SparseModelType> preprocessedModel;
    std::vector<Objective<typename SparseModelType::ValueType>> objectives;

    // Data about the query
    QueryType queryType;

    // Indices of the objectives that can potentially yield infinite reward
    storm::storage::BitVector maybeInfiniteRewardObjectives;

    SparseMultiObjectivePreprocessorResult(storm::logic::MultiObjectiveFormula const& originalFormula, SparseModelType const& originalModel)
        : originalFormula(originalFormula), originalModel(originalModel) {
        // Intentionally left empty
    }

    uint64_t getNumberOfTotalRewardFormulas() const {
        uint64_t count = 0;
        for (auto const& obj : objectives) {
            if (obj.formula->isRewardOperatorFormula() && obj.formula->getSubformula().isTotalRewardFormula()) {
                ++count;
            }
        }
        return count;
    }

    bool containsOnlyTotalRewardFormulas() const {
        return getNumberOfTotalRewardFormulas() == objectives.size();
    }

    uint64_t getNumberOfLongRunAverageRewardFormulas() const {
        uint64_t count = 0;
        for (auto const& obj : objectives) {
            if (obj.formula->isRewardOperatorFormula() && obj.formula->getSubformula().isLongRunAverageRewardFormula()) {
                ++count;
            }
        }
        return count;
    }

    bool containsLongRunAverageRewardFormulas() const {
        return getNumberOfLongRunAverageRewardFormulas() > 0;
    }

    bool containsRewardBoundedObjective() const {
        return std::any_of(objectives.begin(), objectives.end(), [](Objective<typename SparseModelType::ValueType> const& obj) {
            if (obj.formula->isRewardOperatorFormula() && obj.formula->getSubformula().isCumulativeRewardFormula()) {
                auto const& subf = obj.formula->getSubformula().asCumulativeRewardFormula();
                return subf.isMultiDimensional() || subf.getTimeBoundReference().isRewardBound();
            } else if (obj.formula->isProbabilityOperatorFormula() && obj.formula->getSubformula().isBoundedUntilFormula()) {
                auto const& subf = obj.formula->getSubformula().asBoundedUntilFormula();
                return subf.isMultiDimensional() || subf.getTimeBoundReference().isRewardBound();
            } else {
                return false;
            }
        });
    }

    void printToStream(std::ostream& out) const {
        out << "\n---------------------------------------------------------------------------------------------------------------------------------------\n";
        out << "                                                       Multi-objective Query                                              \n";
        out << "---------------------------------------------------------------------------------------------------------------------------------------\n";
        out << "\nOriginal Formula: \n";
        out << "--------------------------------------------------------------\n";
        out << "\t" << originalFormula << '\n';
        out << "\nThe query considers " << objectives.size() << " objectives:\n";
        out << "--------------------------------------------------------------\n";
        for (auto const& obj : objectives) {
            obj.printToStream(out);
            out << '\n';
        }
        out << "Number of Long-Run-Average Reward Objectives (after preprocessing): " << getNumberOfLongRunAverageRewardFormulas() << ".\n";
        out << "Number of Total Reward Objectives (after preprocessing): " << getNumberOfTotalRewardFormulas() << ".\n";
        out << "--------------------------------------------------------------\n";
        out << "\nOriginal Model Information:\n";
        originalModel.printModelInformationToStream(out);
        out << "\nPreprocessed Model Information:\n";
        preprocessedModel->printModelInformationToStream(out);
        out << "\n---------------------------------------------------------------------------------------------------------------------------------------\n";
    }

    friend std::ostream& operator<<(std::ostream& out, SparseMultiObjectivePreprocessorResult<SparseModelType> const& ret) {
        ret.printToStream(out);
        return out;
    }
};
}  // namespace preprocessing
}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm

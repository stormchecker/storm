#pragma once

#include <boost/variant.hpp>
#include <cstdint>
#include <vector>

#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/jani/eliminator/ArrayEliminator.h"
#include "storm/utility/macros.h"

namespace storm {
namespace generator {
class ArrayVariableReplacementInformation {
   public:
    ArrayVariableReplacementInformation(std::vector<ArrayVariableReplacementInformation>&& children);
    ArrayVariableReplacementInformation(uint64_t informationIndex);

    bool isInformationIndex() const;
    uint64_t getInformationIndex() const;
    uint64_t getNumberOfChildren() const;
    ArrayVariableReplacementInformation const& getChild(uint64_t arrayAccessIndex) const;
    uint64_t getVariableInformationIndex(std::vector<uint64_t> arrayIndexVector) const;

   private:
    boost::variant<uint64_t, std::vector<ArrayVariableReplacementInformation>> data;
};

template<typename InfoType>
ArrayVariableReplacementInformation convertArrayReplacement(typename storm::jani::ArrayEliminatorData::Replacement const& replacement,
                                                            InfoType const& relevantVariableInfo) {
    if (replacement.isVariable()) {
        auto const& replVar = replacement.getVariable().getExpressionVariable();
        uint64_t index = 0;
        for (auto const& info : relevantVariableInfo) {
            if (info.variable == replVar) {
                return index;
            }
            ++index;
        }
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                        "Could not find a basic variable for replacement of array variable " << replVar.getName() << " .");
    } else {
        std::vector<ArrayVariableReplacementInformation> result;
        result.reserve(replacement.size());
        for (auto const& child : replacement.getReplacements()) {
            result.push_back(convertArrayReplacement(child, relevantVariableInfo));
        }
        return result;
    }
}

}  // namespace generator
}  // namespace storm
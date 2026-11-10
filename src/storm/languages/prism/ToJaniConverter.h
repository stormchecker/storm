#pragma once

#include <map>
#include <string>

#include "storm/expressions/Expression.h"
#include "storm/expressions/Variable.h"

namespace storm {
namespace jani {
class Model;
class Property;
}  // namespace jani

namespace prism {

class Program;

class ToJaniConverter {
   public:
    storm::jani::Model convert(storm::prism::Program const& program, bool allVariablesGlobal = true,
                               std::set<storm::expressions::Variable> const& variablesToMakeGlobal = {}, std::string suffix = "");

    bool labelsWereRenamed() const;
    bool rewardModelsWereRenamed() const;
    std::map<std::string, std::string> const& getLabelRenaming() const;
    std::map<std::string, std::string> const& getRewardModelRenaming() const;

    storm::jani::Property applyRenaming(storm::jani::Property const& property) const;
    std::vector<storm::jani::Property> applyRenaming(std::vector<storm::jani::Property> const& property) const;

   private:
    std::map<std::string, std::string> labelRenaming;
    std::map<std::string, std::string> rewardModelRenaming;
    std::map<storm::expressions::Variable, storm::expressions::Expression> formulaToFunctionCallMap;
};

}  // namespace prism
}  // namespace storm

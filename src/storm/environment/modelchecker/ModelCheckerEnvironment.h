#pragma once

#include <boost/optional.hpp>
#include <memory>
#include <string>

#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"
#include "storm/environment/modelchecker/CvarModelCheckerEnvironment.h"
#include "storm/modelchecker/helper/conditional/ConditionalAlgorithmSetting.h"
#include "storm/modelchecker/helper/infinitehorizon/SteadyStateDistributionAlgorithm.h"

namespace storm {

// Forward declare subenvironments
class MultiObjectiveModelCheckerEnvironment;

class ModelCheckerEnvironment {
   public:
    ModelCheckerEnvironment();
    ~ModelCheckerEnvironment();

    CvarModelCheckerEnvironment& cvar();
    CvarModelCheckerEnvironment const& cvar() const;

    MultiObjectiveModelCheckerEnvironment& multi();
    MultiObjectiveModelCheckerEnvironment const& multi() const;

    SteadyStateDistributionAlgorithm getSteadyStateDistributionAlgorithm() const;
    void setSteadyStateDistributionAlgorithm(SteadyStateDistributionAlgorithm value);

    ConditionalAlgorithmSetting getConditionalAlgorithmSetting() const;
    void setConditionalAlgorithmSetting(ConditionalAlgorithmSetting value);

    bool isLtl2daToolSet() const;
    std::string const& getLtl2daTool() const;
    void setLtl2daTool(std::string const& value);
    void unsetLtl2daTool();

   private:
    SubEnvironment<CvarModelCheckerEnvironment> cvarModelCheckerEnvironment;
    SubEnvironment<MultiObjectiveModelCheckerEnvironment> multiObjectiveModelCheckerEnvironment;
    boost::optional<std::string> ltl2daTool;
    SteadyStateDistributionAlgorithm steadyStateDistributionAlgorithm;
    ConditionalAlgorithmSetting conditionalAlgorithmSetting;
};
}  // namespace storm

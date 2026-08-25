#pragma once

#include <boost/optional.hpp>
#include <string>

#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"
#include "storm/modelchecker/helper/infinitehorizon/SteadyStateDistributionAlgorithm.h"

namespace storm {

// Forward declare subenvironments
class ConditionalModelCheckerEnvironment;
class MultiObjectiveModelCheckerEnvironment;

// Explicitly instantiated once in CoreEnvironments.cpp
// Avoids redundant re-instantiation elsewhere
extern template class SubEnvironment<ConditionalModelCheckerEnvironment>;
extern template class SubEnvironment<MultiObjectiveModelCheckerEnvironment>;

class ModelCheckerEnvironment {
   public:
    ModelCheckerEnvironment();
    ~ModelCheckerEnvironment();

    ConditionalModelCheckerEnvironment& conditional();
    ConditionalModelCheckerEnvironment const& conditional() const;

    MultiObjectiveModelCheckerEnvironment& multi();
    MultiObjectiveModelCheckerEnvironment const& multi() const;

    SteadyStateDistributionAlgorithm getSteadyStateDistributionAlgorithm() const;
    void setSteadyStateDistributionAlgorithm(SteadyStateDistributionAlgorithm value);

    bool isLtl2daToolSet() const;
    std::string const& getLtl2daTool() const;
    void setLtl2daTool(std::string const& value);
    void unsetLtl2daTool();

    bool isFilterRewZeroSet() const;
    void setFilterRewZero(bool value);

    bool isExportCdfSet() const;
    void setExportCdf(bool value);
    std::string const& getExportCdfDirectory() const;
    void setExportCdfDirectory(std::string const& value);

   private:
    SubEnvironment<ConditionalModelCheckerEnvironment> conditionalModelCheckerEnvironment;
    SubEnvironment<MultiObjectiveModelCheckerEnvironment> multiObjectiveModelCheckerEnvironment;

    boost::optional<std::string> ltl2daTool;
    SteadyStateDistributionAlgorithm steadyStateDistributionAlgorithm;
    bool filterRewZero;
    bool exportCdfEnabled;
    std::string exportCdfDirectory;
};

}  // namespace storm

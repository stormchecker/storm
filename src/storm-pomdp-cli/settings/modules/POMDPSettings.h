#pragma once

#include "storm-config.h"
#include "storm-pomdp/storage/PomdpMemory.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for POMDP model checking.
 */
class POMDPSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of POMDP settings.
     */
    POMDPSettings();

    virtual ~POMDPSettings() = default;

    bool isExportToParametricSet() const;
    std::string getExportToParametricFilename() const;

    bool isQualitativeReductionSet() const;

    bool isNoCanonicSet() const;
    bool isBeliefExplorationSet() const;
    bool isBeliefExplorationDiscretizeSet() const;
    bool isBeliefExplorationUnfoldSet() const;
    bool isAnalyzeUniqueObservationsSet() const;
    bool isSelfloopReductionSet() const;
    bool isCheckFullyObservableSet() const;
    bool isQualitativeAnalysisSet() const;
    bool isBoundedToUnboundedReachabilityTransformationSet() const;
    std::vector<uint64_t> getLevelWidthForBoundedReachability() const;
    bool isRewardObservableSet() const;
    uint64_t getMemoryBound() const;

    storm::storage::PomdpMemoryPattern getMemoryPattern() const;

    bool check() const override;
    void finalize() override;

    // The name of the module.
    static const std::string moduleName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm

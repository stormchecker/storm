#pragma once

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
class ToParametricSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of POMDP settings.
     */
    ToParametricSettings();

    virtual ~ToParametricSettings() = default;

    bool isMecReductionSet() const;
    bool isTransformSimpleSet() const;
    bool isTransformBinarySet() const;
    bool allowPostSimplifications() const;
    std::string getFscApplicationTypeString() const;

    bool check() const override;
    void finalize() override;

    // The name of the module.
    static const std::string moduleName;

   private:
};

}  // namespace modules
}  // namespace settings
}  // namespace storm

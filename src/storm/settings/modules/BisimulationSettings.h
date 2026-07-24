#pragma once

#include "storm/settings/modules/ModuleSettings.h"

#include "storm/storage/dd/bisimulation/QuotientFormat.h"
#include "storm/storage/dd/bisimulation/SignatureMode.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the bisimulation settings.
 */
class BisimulationSettings : public ModuleSettings {
   public:
    // An enumeration of all available bisimulation types.
    enum class BisimulationType { Strong, Weak };

    enum class ReuseMode { None, BlockNumbers };

    enum class InitialPartitionMode { Regular, Finer };

    enum class RefinementMode { Full, ChangedStates };

    /*!
     * Creates a new set of bisimulation settings.
     */
    BisimulationSettings();

    /*!
     * Retrieves whether strong bisimulation is to be used.
     *
     * @return True iff strong bisimulation is to be used.
     */
    bool isStrongBisimulationSet() const;

    /*!
     * Retrieves whether weak bisimulation is to be used.
     *
     * @return True iff weak bisimulation is to be used.
     */
    bool isWeakBisimulationSet() const;

    /*!
     * Retrieves whether the format in which the quotient is to be extracted has been set from its default value.
     *
     * @return True iff it has been set from its default value.
     */
    bool isQuotientFormatSetFromDefaultValue() const;

    /*!
     * Retrieves the format in which the quotient is to be extracted.
     * NOTE: only applies to DD-based bisimulation.
     */
    storm::dd::bisimulation::QuotientFormat getQuotientFormat() const;

    /*!
     * Retrieves whether representatives for blocks are to be used instead of the block numbers.
     * NOTE: only applies to DD-based bisimulation.
     */
    bool isUseRepresentativesSet() const;

    /*!
     * Retrieves whether the extracted quotient model is supposed to use the same variables as the original
     * model.
     * NOTE: only applies to DD-based bisimulation.
     */
    bool isUseOriginalVariablesSet() const;

    /*!
     * Retrieves whether exact arithmetic is to be used in symbolic bisimulation minimization.
     *
     * @return True iff exact arithmetic is to be used in symbolic bisimulation minimization.
     */
    bool useExactArithmeticInDdBisimulation() const;

    /*!
     * Retrieves the mode to compute signatures.
     */
    storm::dd::bisimulation::SignatureMode getSignatureMode() const;

    /*!
     * Retrieves the selected reuse mode.
     */
    ReuseMode getReuseMode() const;

    /*!
     * Retrieves the initial partition mode.
     */
    InitialPartitionMode getInitialPartitionMode() const;

    /*!
     * Retrieves the refinement mode to use.
     */
    RefinementMode getRefinementMode() const;

    virtual bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string typeOptionName;
    static const std::string representativeOptionName;
    static const std::string originalVariablesOptionName;
    static const std::string quotientFormatOptionName;
    static const std::string signatureModeOptionName;
    static const std::string reuseOptionName;
    static const std::string initialPartitionOptionName;
    static const std::string refinementModeOptionName;
    static const std::string parallelismModeOptionName;
    static const std::string exactArithmeticDdOptionName;
};
}  // namespace modules
}  // namespace settings
}  // namespace storm

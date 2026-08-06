#pragma once

#include <boost/optional.hpp>

#include "storm-config.h"
#include "storm/builder/ExplorationOrder.h"
#include "storm/io/CompressionMode.h"
#include "storm/io/ModelExportFormat.h"
#include "storm/modelchecker/helper/infinitehorizon/SteadyStateDistributionAlgorithm.h"
#include "storm/settings/modules/ModuleSettings.h"
#include "storm/solver/UncertaintyResolutionMode.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the markov chain settings.
 */
class IOSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of IO settings.
     */
    IOSettings();

    /*!
     * Retrieves whether the export-to-dot option was set.
     *
     * @return True if the export-to-dot option was set.
     */
    bool isExportDotSet() const;

    /*!
     * Retrieves the name in which to write the model in dot format, if the export-to-dot option was set.
     *
     * @return The name of the file in which to write the exported model.
     */
    std::string getExportDotFilename() const;

    /*!
     * Retrieves the maximal width for labels in the dot format.
     *
     * @return The maximal width.
     */
    size_t getExportDotMaxWidth() const;

    /*!
     * Retrieves whether the exportbuild option was set.
     */
    bool isExportBuildSet() const;

    /*!
     * Retrieves the name in which to write the model in json format, if export-to-json option was set.
     */
    std::string getExportBuildFilename() const;

    /*!
     * Retrieves the specified export format for the exportbuild option
     */
    storm::io::ModelExportFormat getExportBuildFormat() const;

    /*!
     * Retrieves whether a preferred compression mode has been set
     * @note The export might not support compression
     */
    bool isCompressionSet() const;

    /*!
     * Retrieves the preferred compression mode
     * @note The export might not support compression
     */
    storm::io::CompressionMode getCompressionMode() const;

    /*!
     * Retrieves whether the number of digits for exporting floating point numbers was set
     */
    bool isExportDigitsSet() const;

    /*!
     * Retrieves the number of digits for exporting floating point numbers
     */
    size_t getExportDigits() const;

    /*!
     * Retrieves whether the export-to-dot option for jani was set.
     *
     * @return True if the export-to-jani-dot option was set.
     */
    bool isExportJaniDotSet() const;

    /*!
     * Retrieves the name in which to write the jani model in dot format, if the export-to-jani-dot option was set.
     *
     * @return The name of the file in which to write the exported model.
     */
    std::string getExportJaniDotFilename() const;

    /*!
     * Retrieves whether the export-to-explicit option was set
     *
     * @return True if the export-to-explicit option was set
     */
    bool isExportExplicitSet() const;

    /*!
     * Retrieves the name in which to write the model in explicit format, if the option was set.
     *
     * @return The name of the file in which to write the exported model.
     */
    std::string getExportExplicitFilename() const;

    /*!
     * Retrieves whether the export-to-dd option was set
     *
     * @return True if the export-to-explicit option was set
     */
    bool isExportDdSet() const;

    /*!
     * Retrieves the name in which to write the model in dd format, if the option was set.
     *
     * @return The name of the file in which to write the exported model.
     */
    std::string getExportDdFilename() const;

    /*!
     * Retrieves whether the cumulative density function for reward bounded properties should be exported
     */
    bool isExportCdfSet() const;

    /*!
     * Retrieves a path to a directory in which the cdf files will be stored
     */
    std::string getExportCdfDirectory() const;

    /*!
     * Retrieves whether an optimal scheduler is to be exported
     */
    bool isExportSchedulerSet() const;

    /*!
     * Retrieves a filename to which an optimal scheduler will be exported.
     */
    std::string getExportSchedulerFilename() const;

    /*!
     * Retrieves whether the check result should be exported.
     */
    bool isExportCheckResultSet() const;

    /*!
     * Retrieves a filename to which the check result should be exported.
     */
    std::string getExportCheckResultFilename() const;

    /*!
     * Retrieves whether the explicit option was set.
     *
     * @return True if the explicit option was set.
     */
    bool isExplicitSet() const;

    /*!
     * Retrieves the name of the file that contains the transitions if the model was given using the explicit
     * option.
     *
     * @return The name of the file that contains the transitions.
     */
    std::string getTransitionFilename() const;

    /*!
     * Retrieves the name of the file that contains the state labeling if the model was given using the
     * explicit option.
     *
     * @return The name of the file that contains the state labeling.
     */
    std::string getLabelingFilename() const;

    /*!
     * Retrieves whether the explicit option with DRN was set.
     *
     * @return True if the explicit option with DRN was set.
     */
    bool isExplicitDRNSet() const;

    /*!
     * Retrieves the name of the file that contains the model in the DRN format.
     *
     * @return The name of the DRN file that contains the model.
     */
    std::string getExplicitDRNFilename() const;

    /*!
     * Retrieves whether we prevent the usage of placeholders in the explicit DRN format
     * @return
     */
    bool isExplicitExportPlaceholdersDisabled() const;

    /*!
     * Retrieves whether the explicit option with UMB was set.
     *
     * @return True if the explicit option with UMB was set.
     */
    bool isExplicitUmbSet() const;

    /*!
     * Retrieves the name of the file that contains the model in the UMB format.
     *
     * @return The name of the UMB file that contains the model.
     */
    std::string getExplicitUmbFilename() const;

    /*!
     * Retrieves whether the explicit option with IMCA was set.
     *
     * @return True if the explicit option with IMCA was set.
     */
    bool isExplicitIMCASet() const;

    /*!
     * Retrieves the name of the file that contains the model in the IMCA format.
     *
     * @return The name of the IMCA file that contains the model.
     */
    std::string getExplicitIMCAFilename() const;

    /*!
     * Retrieves whether the PRISM language option was set.
     *
     * @return True if the PRISM input option was set.
     */
    bool isPrismInputSet() const;

    /*!
     * Retrieves whether the JANI input option was set.
     *
     * @return True if the JANI input option was set.
     */
    bool isJaniInputSet() const;

    /*!
     * Retrieves whether the JANI or PRISM input option was set.
     *
     * @return True if either of the two options was set.
     */
    bool isPrismOrJaniInputSet() const;

    /*!
     * Retrieves whether the option to convert PRISM to JANI input was set.
     *
     * @return True if the option was set.
     */
    bool isPrismToJaniSet() const;

    /*!
     * Retrieves the name of the file that contains the PRISM model specification if the model was given
     * using the PRISM input option.
     *
     * @return The name of the file that contains the PRISM model specification.
     */
    std::string getPrismInputFilename() const;

    /*!
     * Retrieves the name of the file that contains the JANI model specification if the model was given
     * using the JANI input option.
     *
     * @return The name of the file that contains the JANI model specification.
     */
    std::string getJaniInputFilename() const;

    /*!
     * Retrieves whether the transition reward option was set.
     *
     * @return True if the transition reward option was set.
     */
    bool isTransitionRewardsSet() const;

    /*!
     * Retrieves the name of the file that contains the transition rewards if the model was given using the
     * explicit option.
     *
     * @return The name of the file that contains the transition rewards.
     */
    std::string getTransitionRewardsFilename() const;

    /*!
     * Retrieves whether the state reward option was set.
     *
     * @return True if the state reward option was set.
     */
    bool isStateRewardsSet() const;

    /*!
     * Retrieves the name of the file that contains the state rewards if the model was given using the
     * explicit option.
     *
     * @return The name of the file that contains the state rewards.
     */
    std::string getStateRewardsFilename() const;

    /*!
     * Retrieves whether the choice labeling option was set.
     *
     * @return True iff the choice labeling option was set.
     */
    bool isChoiceLabelingSet() const;

    /*!
     * Retrieves the name of the file that contains the choice labeling
     * if the model was given using the explicit option.
     *
     * @return The name of the file that contains the choice labeling.
     */
    std::string getChoiceLabelingFilename() const;

    /*!
     * Retrieves whether the constants option was set.
     *
     * @return True if the constants option was set.
     */
    bool isConstantsSet() const;

    /*!
     * Retrieves the string that defines the constants of a symbolic model (given via the symbolic option).
     *
     * @return The string that defines the constants of a symbolic model.
     */
    std::string getConstantDefinitionString() const;

    /*!
     * Retrieves whether the jani-property option was set
     * @return
     */
    bool isJaniPropertiesSet() const;

    /*!
     * Retrieves whether one or more jani-properties have been selected
     * @return
     */
    bool areJaniPropertiesSelected() const;

    /*!
     * @return  The names of the jani properties to check
     */
    std::vector<std::string> getSelectedJaniProperties() const;

    /*!
     * Retrieves whether the property option was set.
     *
     * @return True if the property option was set.
     */
    bool isPropertySet() const;

    /*!
     * Retrieves the property specified with the property option.
     *
     * @return The property specified with the property option.
     */
    std::string getProperty() const;

    /*!
     * Retrieves the property filter.
     *
     * @return The property filter.
     */
    std::string getPropertyFilter() const;

    /*!
     * Retrieves whether the CVaR option was set.
     *
     * @return True if the CVaR option was set.
     */
    bool isCvarSet() const;

    /*!
     * Retrieves the alpha value specified with the CVaR option.
     *
     * @return The alpha value specified with the CVaR option.
     */
    std::string getCvarAlpha() const;

    /*!
     * Retrieves whether the steady-state distribution is to be computed.
     */
    bool isComputeSteadyStateDistributionSet() const;

    /*!
     * @return the algorithm used for computing steady state distributions
     */
    storm::SteadyStateDistributionAlgorithm getSteadyStateDistributionAlgorithm() const;

    /*!
     * Retrieves whether the expected visiting times are to be computed.
     */
    bool isComputeExpectedVisitingTimesSet() const;

    /*!
     * Retrieves whether the input model is to be read from the quantitative verification benchmark set (QVBS)
     */
    bool isQvbsInputSet() const;

    /*!
     * Retrieves the specified model (short-)name of the QVBS
     */
    std::string getQvbsModelName() const;

    /*!
     * Retrieves the selected model instance (file + open parameters of the model)
     */
    uint64_t getQvbsInstanceIndex() const;

    /*!
     * Retrieves the selected property names
     */
    boost::optional<std::vector<std::string>> getQvbsPropertyFilter() const;

    /*!
     * Retrieves the specified root directory of qvbs
     */
    std::string getQvbsRoot() const;

    /*!
     * Retrieves whether the input properties are to be interpreted as a single multi-objective formula
     */
    bool isPropertiesAsMultiSet() const;

    /*!
     * Retrieves the mode deciding how the uncertainty should be resolved.
     *
     * @return The nature resolution mode
     */
    UncertaintyResolutionModeSetting getUncertaintyResolutionMode() const;

    /*!
     * @return whether the mode for how the uncertainty should be resolved has been set.
     */
    bool isUncertaintyResolutionModeSet() const;

    bool check() const override;
    void finalize() override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string exportDotOptionName;
    static const std::string exportDotMaxWidthOptionName;
    static const std::string exportBuildOptionName;
    static const std::string exportJaniDotOptionName;
    static const std::string exportExplicitOptionName;
    static const std::string exportDdOptionName;
    static const std::string exportCdfOptionName;
    static const std::string exportCdfOptionShortName;
    static const std::string exportSchedulerOptionName;
    static const std::string exportCheckResultOptionName;
    static const std::string exportCompressionOptionName;
    static const std::string exportDigitsOptionName;
    static const std::string explicitOptionName;
    static const std::string explicitOptionShortName;
    static const std::string explicitDrnOptionName;
    static const std::string explicitDrnOptionShortName;
    static const std::string explicitUmbOptionName;
    static const std::string explicitUmbOptionShortName;
    static const std::string explicitImcaOptionName;
    static const std::string explicitImcaOptionShortName;
    static const std::string prismInputOptionName;
    static const std::string janiInputOptionName;
    static const std::string prismToJaniOptionName;
    static const std::string transitionRewardsOptionName;
    static const std::string stateRewardsOptionName;
    static const std::string choiceLabelingOptionName;
    static const std::string constantsOptionName;
    static const std::string constantsOptionShortName;
    static const std::string janiPropertyOptionName;
    static const std::string janiPropertyOptionShortName;
    static const std::string propertyOptionName;
    static const std::string propertyOptionShortName;
    static const std::string steadyStateDistrOptionName;
    static const std::string expectedVisitingTimesOptionName;
    static const std::string qvbsInputOptionName;
    static const std::string qvbsInputOptionShortName;
    static const std::string qvbsRootOptionName;
    static const std::string propertiesAsMultiOptionName;
    static const std::string cvarOptionName;
    static const std::string uncertaintyResolutionModeName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm

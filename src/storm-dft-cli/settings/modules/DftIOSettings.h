#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm::dft {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for IO operations concerning DFTs.
 */
class DftIOSettings : public storm::settings::modules::ModuleSettings {
   public:
    /*!
     * Creates a new set of IO settings for DFTs.
     */
    DftIOSettings();

    /*!
     * Retrieves whether the dft file option was set.
     *
     * @return True if the dft file option was set.
     */
    bool isDftFileSet() const;

    /*!
     * Retrieves the name of the file that contains the dft specification.
     *
     * @return The name of the file that contains the dft specification.
     */
    std::string getDftFilename() const;

    /*!
     * Retrieves whether the dft file option for Json was set.
     *
     * @return True if the dft file option was set.
     */
    bool isDftJsonFileSet() const;

    /*!
     * Retrieves the name of the json file that contains the dft specification.
     *
     * @return The name of the json file that contains the dft specification.
     */
    std::string getDftJsonFilename() const;

    /*!
     * Retrieves whether the property expected time should be used.
     *
     * @return True iff the option was set.
     */
    bool usePropExpectedTime() const;

    /*!
     * Retrieves whether the property probability should be used.
     *
     * @return True iff the option was set.
     */
    bool usePropProbability() const;

    /*!
     * Retrieves whether the property timebound should be used.
     *
     * @return True iff the option was set.
     */
    bool usePropTimebound() const;

    /*!
     * Retrieves the timebound for the timebound property.
     *
     * @return The timebound.
     */
    double getPropTimebound() const;

    /*!
     * Retrieves whether the property timepoints should be used.
     *
     * @return True iff the option was set.
     */
    bool usePropTimepoints() const;

    /*!
     * Retrieves the settings for the timepoints property.
     *
     * @return The timepoints.
     */
    std::vector<double> getPropTimepoints() const;

    /*!
     * Retrieves whether the minimal value should be computed for non-determinism.
     *
     * @return True iff the option was set.
     */
    bool isComputeMinimalValue() const;

    /*!
     * Retrieves whether the maximal value should be computed for non-determinism.
     *
     * @return True iff the option was set.
     */
    bool isComputeMaximalValue() const;

    /*!
     * Retrieves whether the export to Json file option was set.
     *
     * @return True if the export to json file option was set.
     */
    bool isExportToJson() const;

    /*!
     * Retrieves whether the export to smtlib2 file option was set.
     *
     * @return True if the export to smtlib2 file option was set.
     */
    bool isExportToSmt() const;

    /*!
     * Retrieves whether the analyze with Bdds option was set.
     *
     * @return True if the analyze with Bdds option was set.
     */
    bool isAnalyzeWithBdds() const;

    /*!
     * Retrieves whether the minimal cut sets option was set.
     *
     * @return True if the minimal cut sets option was set.
     */
    bool isMinimalCutSets() const;

    /*!
     * Retrieves whether the export to Bdd Dot file option was set.
     *
     * @return True if the export to Bdd Dot file option was set.
     */
    bool isExportToBddDot() const;

    /*!
     * Retrieves the name of the json file to export to.
     *
     * @return The name of the json file to export to.
     */
    std::string getExportJsonFilename() const;

    /*!
     * Retrieves the name of the smtlib2 file to export to.
     *
     * @return The name of the smtlib2 file to export to.
     */
    std::string getExportSmtFilename() const;

    /*!
     * Retrieves the name of the dot file to export to.
     *
     * @return The name of the dot file to export to.
     */
    std::string getExportBddDotFilename() const;

    /*!
     * Retrieves whether statistics about the DFT analysis should be displayed.
     *
     * @return True if the statistics option was set.
     */
    bool isShowDftStatisticsSet() const;

    /*!
     * Retrieves whether to calculate an importance measure.
     *
     * @return True iff the option was set.
     */
    bool isImportanceMeasureSet() const;

    /*!
     * Retrieves the name of the importance measure to calculate.
     *
     * @return The name of the importance measure.
     */
    std::string getImportanceMeasure() const;

    /*!
     * Retrieves whether the varialble ordering file option was set.
     *
     * @return True if the variable ordering file option was set.
     */
    bool isVariableOrderingFileSet() const;

    /*!
     * Retrieves the name of the file that contains the variable ordering.
     *
     * @return The name of the file that contains the variable ordering.
     */
    std::string getVariableOrderingFilename() const;

    bool check() const override;
    void finalize() override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string dftFileOptionName;
    static const std::string dftFileOptionShortName;
    static const std::string dftJsonFileOptionName;
    static const std::string dftJsonFileOptionShortName;
    static const std::string propExpectedTimeOptionName;
    static const std::string propExpectedTimeOptionShortName;
    static const std::string propProbabilityOptionName;
    static const std::string propTimeboundOptionName;
    static const std::string propTimepointsOptionName;
    static const std::string minValueOptionName;
    static const std::string maxValueOptionName;
    static const std::string analyzeWithBdds;
    static const std::string minimalCutSets;
    static const std::string exportToJsonOptionName;
    static const std::string exportToSmtOptionName;
    static const std::string exportToBddDotOptionName;
    static const std::string dftStatisticsOptionName;
    static const std::string dftStatisticsOptionShortName;
    static const std::string importanceMeasureOptionName;
    static const std::string variableOrderingFileOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm::dft

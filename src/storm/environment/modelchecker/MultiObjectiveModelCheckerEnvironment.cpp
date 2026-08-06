#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/MultiObjectiveSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/IllegalArgumentException.h"
namespace storm {

MultiObjectiveModelCheckerEnvironment::MultiObjectiveModelCheckerEnvironment() {
    auto const& multiobjectiveSettings = storm::settings::getModule<storm::settings::modules::MultiObjectiveSettings>();
    method = multiobjectiveSettings.getMultiObjectiveMethod();
    if (multiobjectiveSettings.isExportPlotSet()) {
        plotPathUnderApprox = multiobjectiveSettings.getExportPlotDirectory() + "underapproximation.csv";
        plotPathOverApprox = multiobjectiveSettings.getExportPlotDirectory() + "overapproximation.csv";
        plotPathParetoPoints = multiobjectiveSettings.getExportPlotDirectory() + "paretopoints.csv";
    }

    precision = storm::utility::convertNumber<storm::RationalNumber>(multiobjectiveSettings.getPrecision());
    if (multiobjectiveSettings.getPrecisionAbsolute()) {
        precisionType = PrecisionType::Absolute;
    } else if (multiobjectiveSettings.getPrecisionRelativeToDiff()) {
        precisionType = PrecisionType::RelativeToDiff;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unhandled precision type.");
    }

    if (multiobjectiveSettings.isAutoEncodingSet()) {
        encodingType = EncodingType::Auto;
    } else if (multiobjectiveSettings.isClassicEncodingSet()) {
        encodingType = EncodingType::Classic;
    } else if (multiobjectiveSettings.isFlowEncodingSet()) {
        encodingType = EncodingType::Flow;
    }
    STORM_LOG_ASSERT(multiobjectiveSettings.isBsccDetectionViaOrderConstraintsSet() || multiobjectiveSettings.isBsccDetectionViaFlowConstraintsSet(),
                     "unexpected settings");
    bsccOrderEncoding = multiobjectiveSettings.isBsccDetectionViaOrderConstraintsSet();
    STORM_LOG_ASSERT(multiobjectiveSettings.isIndicatorConstraintsSet() || multiobjectiveSettings.isBigMConstraintsSet(), "unexpected settings");
    indicatorConstraints = multiobjectiveSettings.isIndicatorConstraintsSet();
    redundantBsccConstraints = multiobjectiveSettings.isRedundantBsccConstraintsSet();

    if (multiobjectiveSettings.isWeightedSumApproximationTradeoffSet()) {
        approximationTradeoff = storm::utility::convertNumber<storm::RationalNumber>(multiobjectiveSettings.getWeightedSumApproximationTradeoff());
    }
    if (multiobjectiveSettings.isMaxStepsSet()) {
        maxSteps = multiobjectiveSettings.getMaxSteps();
    }
    if (multiobjectiveSettings.hasSchedulerRestriction()) {
        schedulerRestriction = multiobjectiveSettings.getSchedulerRestriction();
    }

    printResults = multiobjectiveSettings.isPrintResultsSet();
}

MultiObjectiveModelCheckerEnvironment::~MultiObjectiveModelCheckerEnvironment() {
    // Intentionally left empty
}

storm::modelchecker::multiobjective::MultiObjectiveMethod const& MultiObjectiveModelCheckerEnvironment::getMethod() const {
    return this->method;
}

void MultiObjectiveModelCheckerEnvironment::setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod value) {
    this->method = value;
}

bool MultiObjectiveModelCheckerEnvironment::isExportPlotSet() const {
    return this->plotPathUnderApprox.is_initialized() || this->plotPathOverApprox.is_initialized() || this->plotPathParetoPoints.is_initialized();
}

boost::optional<std::string> MultiObjectiveModelCheckerEnvironment::getPlotPathUnderApproximation() const {
    return plotPathUnderApprox;
}

void MultiObjectiveModelCheckerEnvironment::setPlotPathUnderApproximation(std::string const& path) {
    plotPathUnderApprox = path;
}

void MultiObjectiveModelCheckerEnvironment::unsetPlotPathUnderApproximation() {
    plotPathUnderApprox = boost::none;
}

boost::optional<std::string> MultiObjectiveModelCheckerEnvironment::getPlotPathOverApproximation() const {
    return plotPathOverApprox;
}

void MultiObjectiveModelCheckerEnvironment::setPlotPathOverApproximation(std::string const& path) {
    plotPathOverApprox = path;
}

void MultiObjectiveModelCheckerEnvironment::unsetPlotPathOverApproximation() {
    plotPathOverApprox = boost::none;
}

boost::optional<std::string> MultiObjectiveModelCheckerEnvironment::getPlotPathParetoPoints() const {
    return plotPathParetoPoints;
}

void MultiObjectiveModelCheckerEnvironment::setPlotPathParetoPoints(std::string const& path) {
    plotPathParetoPoints = path;
}

void MultiObjectiveModelCheckerEnvironment::unsetPlotPathParetoPoints() {
    plotPathParetoPoints = boost::none;
}

storm::RationalNumber const& MultiObjectiveModelCheckerEnvironment::getPrecision() const {
    return precision;
}

void MultiObjectiveModelCheckerEnvironment::setPrecision(storm::RationalNumber const& value) {
    precision = value;
}

typename MultiObjectiveModelCheckerEnvironment::PrecisionType const& MultiObjectiveModelCheckerEnvironment::getPrecisionType() const {
    return precisionType;
}

void MultiObjectiveModelCheckerEnvironment::setPrecisionType(PrecisionType const& value) {
    precisionType = value;
}

typename MultiObjectiveModelCheckerEnvironment::EncodingType const& MultiObjectiveModelCheckerEnvironment::getEncodingType() const {
    return encodingType;
}

void MultiObjectiveModelCheckerEnvironment::setEncodingType(EncodingType const& value) {
    encodingType = value;
}

bool MultiObjectiveModelCheckerEnvironment::getUseIndicatorConstraints() const {
    return indicatorConstraints;
}
void MultiObjectiveModelCheckerEnvironment::setUseIndicatorConstraints(bool value) {
    indicatorConstraints = value;
}

bool MultiObjectiveModelCheckerEnvironment::getUseBsccOrderEncoding() const {
    return bsccOrderEncoding;
}
void MultiObjectiveModelCheckerEnvironment::setUseBsccOrderEncoding(bool value) {
    bsccOrderEncoding = value;
}

bool MultiObjectiveModelCheckerEnvironment::getUseRedundantBsccConstraints() const {
    return redundantBsccConstraints;
}

void MultiObjectiveModelCheckerEnvironment::setUseRedundantBsccConstraints(bool value) {
    redundantBsccConstraints = value;
}

bool MultiObjectiveModelCheckerEnvironment::isApproximationTradeoffSet() const {
    return approximationTradeoff.is_initialized();
}

storm::RationalNumber const& MultiObjectiveModelCheckerEnvironment::getApproximationTradeoff() const {
    return approximationTradeoff.get();
}

void MultiObjectiveModelCheckerEnvironment::setApproximationTradeoff(storm::RationalNumber const& value) {
    approximationTradeoff = value;
}

void MultiObjectiveModelCheckerEnvironment::unsetApproximationTradeoff() {
    approximationTradeoff = boost::none;
}

bool MultiObjectiveModelCheckerEnvironment::isMaxStepsSet() const {
    return maxSteps.is_initialized();
}

uint64_t const& MultiObjectiveModelCheckerEnvironment::getMaxSteps() const {
    return maxSteps.get();
}

void MultiObjectiveModelCheckerEnvironment::setMaxSteps(uint64_t const& value) {
    maxSteps = value;
}

void MultiObjectiveModelCheckerEnvironment::unsetMaxSteps() {
    maxSteps = boost::none;
}

bool MultiObjectiveModelCheckerEnvironment::isSchedulerRestrictionSet() const {
    return schedulerRestriction.is_initialized();
}

storm::storage::SchedulerClass const& MultiObjectiveModelCheckerEnvironment::getSchedulerRestriction() const {
    return schedulerRestriction.get();
}

void MultiObjectiveModelCheckerEnvironment::setSchedulerRestriction(storm::storage::SchedulerClass const& value) {
    schedulerRestriction = value;
}

void MultiObjectiveModelCheckerEnvironment::unsetSchedulerRestriction() {
    schedulerRestriction = boost::none;
}

bool MultiObjectiveModelCheckerEnvironment::isPrintResultsSet() const {
    return printResults;
}

void MultiObjectiveModelCheckerEnvironment::setPrintResults(bool value) {
    printResults = value;
}

}  // namespace storm
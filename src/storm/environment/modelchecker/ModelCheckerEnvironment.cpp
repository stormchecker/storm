#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"

#include "storm/environment/modelchecker/ConditionalModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"

#include "storm/modelchecker/helper/infinitehorizon/SteadyStateDistributionAlgorithm.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {

ModelCheckerEnvironment::ModelCheckerEnvironment()
    : ltl2daTool(boost::none),
      steadyStateDistributionAlgorithm(storm::SteadyStateDistributionAlgorithm::Automatic),
      filterRewZero(false),
      exportCdfEnabled(false),
      exportCdfDirectory("") {
    // Intentionally left empty.
}

ModelCheckerEnvironment::~ModelCheckerEnvironment() {
    // Intentionally left empty
}

ConditionalModelCheckerEnvironment& ModelCheckerEnvironment::conditional() {
    return conditionalModelCheckerEnvironment.get();
}

ConditionalModelCheckerEnvironment const& ModelCheckerEnvironment::conditional() const {
    return conditionalModelCheckerEnvironment.get();
}

MultiObjectiveModelCheckerEnvironment& ModelCheckerEnvironment::multi() {
    return multiObjectiveModelCheckerEnvironment.get();
}

MultiObjectiveModelCheckerEnvironment const& ModelCheckerEnvironment::multi() const {
    return multiObjectiveModelCheckerEnvironment.get();
}

SteadyStateDistributionAlgorithm ModelCheckerEnvironment::getSteadyStateDistributionAlgorithm() const {
    return steadyStateDistributionAlgorithm;
}

void ModelCheckerEnvironment::setSteadyStateDistributionAlgorithm(SteadyStateDistributionAlgorithm value) {
    steadyStateDistributionAlgorithm = value;
}

bool ModelCheckerEnvironment::isLtl2daToolSet() const {
    return ltl2daTool.is_initialized();
}

std::string const& ModelCheckerEnvironment::getLtl2daTool() const {
    return ltl2daTool.get();
}

void ModelCheckerEnvironment::setLtl2daTool(std::string const& value) {
    ltl2daTool = value;
}

void ModelCheckerEnvironment::unsetLtl2daTool() {
    ltl2daTool = boost::none;
}

bool ModelCheckerEnvironment::isFilterRewZeroSet() const {
    return filterRewZero;
}

void ModelCheckerEnvironment::setFilterRewZero(bool value) {
    filterRewZero = value;
}

bool ModelCheckerEnvironment::isExportCdfSet() const {
    return exportCdfEnabled;
}

void ModelCheckerEnvironment::setExportCdf(bool value) {
    exportCdfEnabled = value;
}

std::string const& ModelCheckerEnvironment::getExportCdfDirectory() const {
    return exportCdfDirectory;
}

void ModelCheckerEnvironment::setExportCdfDirectory(std::string const& value) {
    exportCdfDirectory = value;
}

}  // namespace storm

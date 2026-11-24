#include "storm-pars/settings/modules/DerivativeSettings.h"

#include "storm-pars/derivative/GradientDescentConstraintMethod.h"
#include "storm-pars/derivative/GradientDescentMethod.h"
#include "storm/settings/Argument.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
namespace modules {

const std::string DerivativeSettings::moduleName = "derivative";
const std::string DerivativeSettings::feasibleInstantiationSearch = "gradient-descent";
const std::string DerivativeSettings::learningRate = "learning-rate";
const std::string DerivativeSettings::miniBatchSize = "batch-size";
const std::string DerivativeSettings::adamParams = "adam-params";
const std::string DerivativeSettings::averageDecay = "average-decay";
const std::string DerivativeSettings::squaredAverageDecay = "squared-average-decay";
const std::string DerivativeSettings::terminationEpsilon = "termination-epsilon";
const std::string DerivativeSettings::printJson = "print-json";
const std::string DerivativeSettings::gradientDescentMethod = "descent-method";
const std::string DerivativeSettings::omitInconsequentialParams = "omit-inconsequential-params";
const std::string DerivativeSettings::constraintMethod = "constraint-method";

DerivativeSettings::DerivativeSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, feasibleInstantiationSearch, false,
                                                   "Search for a feasible instantiation (restart with new instantiation while not feasible)")
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, learningRate, false, "Sets the learning rate of gradient descent")
                        .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(learningRate, "The learning rate of the gradient descent")
                                         .setDefaultValueDouble(0.1)
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, miniBatchSize, false, "Sets the size of the minibatch")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createIntegerArgument(miniBatchSize, "The size of the minibatch").setDefaultValueInteger(32).build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, gradientDescentMethod, false, "Sets the gradient descent method")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument(
                                         gradientDescentMethod,
                                         "Gradient Descent method (adam, radam, rmsprop, plain, plain-sign, momentum, momentum-sign, nesterov, nesterov-sign)")
                                         .setDefaultValueString("adam")
                                         .build())
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(
            moduleName, adamParams, false,
            "Sets hyperparameters of the Gradient Descent algorithms, especially (R)ADAM's. If you're using RMSProp, averageDecay is RMSProp's decay.")
            .setIsAdvanced()
            .addArgument(
                storm::settings::ArgumentBuilder::createDoubleArgument(averageDecay, "Decay of decaying step average").setDefaultValueDouble(0.9).build())
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(squaredAverageDecay, "Decay of squared decaying step average")
                             .setDefaultValueDouble(0.999)
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, printJson, false, "Print the run as json after finishing (slow!)").setIsAdvanced().build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, terminationEpsilon, false,
                                       "The change in value that constitutes as a \"tiny change\", after a few of which the gradient descent will terminate")
            .addArgument(storm::settings::ArgumentBuilder::createDoubleArgument(terminationEpsilon, "The epsilon").setDefaultValueDouble(1e-6).build())
            .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, omitInconsequentialParams, false,
                                       "Parameters that are removed in minimization because they have no effect on the rational function are normally set to "
                                       "0.5 in the final instantiation. If this flag is set, they will be omitted from the final instantiation entirely.")
            .setIsAdvanced()
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, constraintMethod, false, "Constraint Method")
                        .setIsAdvanced()
                        .addArgument(storm::settings::ArgumentBuilder::createStringArgument(constraintMethod, "Method for dealing with constraints")
                                         .setDefaultValueString("project-gradient")
                                         .build())
                        .build());
}

bool DerivativeSettings::isFeasibleInstantiationSearchSet() const {
    return this->getOption(feasibleInstantiationSearch).getHasOptionBeenSet();
}

double DerivativeSettings::getLearningRate() const {
    return this->getOption(learningRate).getArgumentByName(learningRate).getValueAsDouble();
}
uint_fast64_t DerivativeSettings::getMiniBatchSize() const {
    return this->getOption(miniBatchSize).getArgumentByName(miniBatchSize).getValueAsInteger();
}
double DerivativeSettings::getAverageDecay() const {
    return this->getOption(adamParams).getArgumentByName(averageDecay).getValueAsDouble();
}
double DerivativeSettings::getSquaredAverageDecay() const {
    return this->getOption(adamParams).getArgumentByName(squaredAverageDecay).getValueAsDouble();
}

bool DerivativeSettings::isPrintJsonSet() const {
    return this->getOption(printJson).getHasOptionBeenSet();
}

double DerivativeSettings::getTerminationEpsilon() const {
    return this->getOption(terminationEpsilon).getArgumentByName(terminationEpsilon).getValueAsDouble();
}

boost::optional<derivative::GradientDescentMethod> DerivativeSettings::getGradientDescentMethod() const {
    return methodFromString(this->getOption(gradientDescentMethod).getArgumentByName(gradientDescentMethod).getValueAsString());
}

std::string DerivativeSettings::getGradientDescentMethodAsString() const {
    return this->getOption(gradientDescentMethod).getArgumentByName(gradientDescentMethod).getValueAsString();
}

bool DerivativeSettings::areInconsequentialParametersOmitted() const {
    return this->getOption(omitInconsequentialParams).getHasOptionBeenSet();
}

boost::optional<derivative::GradientDescentConstraintMethod> DerivativeSettings::getConstraintMethod() const {
    return constraintMethodFromString(this->getOption(constraintMethod).getArgumentByName(constraintMethod).getValueAsString());
}

std::string DerivativeSettings::getConstraintMethodAsString() const {
    return this->getOption(constraintMethod).getArgumentByName(constraintMethod).getValueAsString();
}

boost::optional<derivative::GradientDescentMethod> DerivativeSettings::methodFromString(const std::string &str) const {
    derivative::GradientDescentMethod method;
    if (str == "adam") {
        method = derivative::GradientDescentMethod::ADAM;
    } else if (str == "radam") {
        method = derivative::GradientDescentMethod::RADAM;
    } else if (str == "rmsprop") {
        method = derivative::GradientDescentMethod::RMSPROP;
    } else if (str == "plain") {
        method = derivative::GradientDescentMethod::PLAIN;
    } else if (str == "plain-sign") {
        method = derivative::GradientDescentMethod::PLAIN_SIGN;
    } else if (str == "momentum") {
        method = derivative::GradientDescentMethod::MOMENTUM;
    } else if (str == "momentum-sign") {
        method = derivative::GradientDescentMethod::MOMENTUM_SIGN;
    } else if (str == "nesterov") {
        method = derivative::GradientDescentMethod::NESTEROV;
    } else if (str == "nesterov-sign") {
        method = derivative::GradientDescentMethod::NESTEROV_SIGN;
    } else {
        return boost::none;
    }
    return method;
}

boost::optional<derivative::GradientDescentConstraintMethod> DerivativeSettings::constraintMethodFromString(const std::string &str) const {
    derivative::GradientDescentConstraintMethod method;
    if (str == "project-gradient") {
        method = derivative::GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT;
    } else if (str == "project") {
        method = derivative::GradientDescentConstraintMethod::PROJECT;
    } else if (str == "penalty-quadratic") {
        method = derivative::GradientDescentConstraintMethod::PENALTY_QUADRATIC;
    } else if (str == "barrier-logarithmic") {
        method = derivative::GradientDescentConstraintMethod::BARRIER_LOGARITHMIC;
    } else if (str == "barrier-infinity") {
        method = derivative::GradientDescentConstraintMethod::BARRIER_INFINITY;
    } else if (str == "logistic-sigmoid") {
        method = derivative::GradientDescentConstraintMethod::LOGISTIC_SIGMOID;
    } else {
        return boost::none;
    }
    return method;
}

}  // namespace modules
}  // namespace settings
}  // namespace storm

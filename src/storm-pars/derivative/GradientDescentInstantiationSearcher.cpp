#include "storm-pars/derivative/GradientDescentInstantiationSearcher.h"

#include <cmath>
#include <iostream>
#include <random>

#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/constants.h"

namespace storm {
namespace derivative {

template<typename FunctionType>
using VariableType = typename utility::parametric::VariableType<FunctionType>::type;
template<typename FunctionType>
using CoefficientType = typename utility::parametric::CoefficientType<FunctionType>::type;

template<typename FunctionType, typename ConstantType>
ConstantType GradientDescentInstantiationSearcher<FunctionType, ConstantType>::doStep(
    VariableType<FunctionType> steppingParameter, std::map<VariableType<FunctionType>, CoefficientType<FunctionType>>& position,
    const std::map<VariableType<FunctionType>, ConstantType>& gradient, uint64_t stepNum) {
    const ConstantType precisionAsConstant = utility::convertNumber<ConstantType>(this->env.modelTolerance());
    const CoefficientType<FunctionType> precision = storm::utility::convertNumber<CoefficientType<FunctionType>>(this->env.modelTolerance());
    CoefficientType<FunctionType> const oldPos = position[steppingParameter];
    ConstantType const oldPosAsConstant = utility::convertNumber<ConstantType>(position[steppingParameter]);

    ConstantType projectedGradient;
    if (constraintMethod == GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT) {
        // Project gradient
        ConstantType newPlainPosition = oldPosAsConstant + precisionAsConstant * gradient.at(steppingParameter);
        auto const lower =
            region ? utility::convertNumber<ConstantType>(region->getLowerBoundary(steppingParameter)) : utility::zero<ConstantType>() + precisionAsConstant;
        auto const upper =
            region ? utility::convertNumber<ConstantType>(region->getUpperBoundary(steppingParameter)) : utility::one<ConstantType>() - precisionAsConstant;
        if (newPlainPosition < lower || newPlainPosition > upper) {
            projectedGradient = 0;
        } else {
            projectedGradient = gradient.at(steppingParameter);
        }
    } else if (constraintMethod == GradientDescentConstraintMethod::LOGISTIC_SIGMOID) {
        // We want the derivative of f(logit(x)), this happens to be exp(x) * f'(logit(x)) / (exp(x) + 1)^2
        const double expX = std::exp(utility::convertNumber<double>(oldPos));
        projectedGradient = gradient.at(steppingParameter) * utility::convertNumber<ConstantType>(expX / std::pow(expX + 1, 2));
    } else if (constraintMethod == GradientDescentConstraintMethod::BARRIER_INFINITY) {
        if (oldPosAsConstant < precisionAsConstant) {
            projectedGradient = 1000;
        } else if (oldPosAsConstant > utility::one<ConstantType>() - precisionAsConstant) {
            projectedGradient = -1000;
        } else {
            projectedGradient = gradient.at(steppingParameter);
        }
    } else if (constraintMethod == GradientDescentConstraintMethod::BARRIER_LOGARITHMIC) {
        // Our barrier is:
        // log(x) if 0 < x < 0.5
        // log(1 - x) if 0.5 <= x < 1
        // -infinity otherwise
        // The gradient of this is
        // 1/x, 1/(1-x), +/-infinity respectively
        if (oldPosAsConstant >= precisionAsConstant && oldPosAsConstant <= utility::one<ConstantType>() - precisionAsConstant) {
            /* const double mu = (double) parameters.size() / (double) stepNum; */
            if (oldPosAsConstant * 2 < utility::one<ConstantType>()) {
                projectedGradient = gradient.at(steppingParameter) + logarithmicBarrierTerm / (oldPosAsConstant - precisionAsConstant);
            } else {
                projectedGradient =
                    gradient.at(steppingParameter) - logarithmicBarrierTerm / (utility::one<ConstantType>() - precisionAsConstant - oldPosAsConstant);
            }
        } else {
            if (oldPosAsConstant < precisionAsConstant) {
                projectedGradient = utility::one<ConstantType>() / logarithmicBarrierTerm;
            } else if (oldPosAsConstant > utility::one<ConstantType>() - precisionAsConstant) {
                projectedGradient = -utility::one<ConstantType>() / logarithmicBarrierTerm;
            }
        }
    } else {
        projectedGradient = gradient.at(steppingParameter);
    }

    // Compute step based on used gradient descent method
    ConstantType step;
    if (Adam* adam = boost::get<Adam>(&gradientDescentType)) {
        // For this algorihm, see the various sources available on the ADAM algorithm. This implementation should
        // be correct, as it is compared with a run of keras's ADAM optimizer in the test.
        adam->decayingStepAverage[steppingParameter] =
            adam->averageDecay * adam->decayingStepAverage[steppingParameter] + (utility::one<ConstantType>() - adam->averageDecay) * projectedGradient;
        adam->decayingStepAverageSquared[steppingParameter] = adam->squaredAverageDecay * adam->decayingStepAverageSquared[steppingParameter] +
                                                              (utility::one<ConstantType>() - adam->squaredAverageDecay) * utility::pow(projectedGradient, 2);

        const ConstantType correctedGradient =
            adam->decayingStepAverage[steppingParameter] / (utility::one<ConstantType>() - utility::pow(adam->averageDecay, stepNum + 1));
        const ConstantType correctedSquaredGradient =
            adam->decayingStepAverageSquared[steppingParameter] / (utility::one<ConstantType>() - utility::pow(adam->squaredAverageDecay, stepNum + 1));

        const ConstantType toSqrt = correctedSquaredGradient;
        ConstantType sqrtResult = constantTypeSqrt(toSqrt);

        step = (adam->learningRate / (sqrtResult + precisionAsConstant)) * correctedGradient;
    } else if (RAdam* radam = boost::get<RAdam>(&gradientDescentType)) {
        // You can compare this with the RAdam paper's "Algorithm 2: Rectified Adam".
        // The line numbers and comments are matched.
        // Initializing / Compute Gradient: Already happened.
        // 2: Compute maximum length of approximated simple moving average
        const ConstantType maxLengthApproxSMA = 2 / (utility::one<ConstantType>() - radam->squaredAverageDecay) - utility::one<ConstantType>();

        // 5: Update exponential moving 2nd moment
        radam->decayingStepAverageSquared[steppingParameter] = radam->squaredAverageDecay * radam->decayingStepAverageSquared[steppingParameter] +
                                                               (utility::one<ConstantType>() - radam->squaredAverageDecay) * utility::pow(projectedGradient, 2);
        // 6: Update exponential moving 1st moment
        radam->decayingStepAverage[steppingParameter] =
            radam->averageDecay * radam->decayingStepAverage[steppingParameter] + (utility::one<ConstantType>() - radam->averageDecay) * projectedGradient;
        // 7: Compute bias corrected moving average
        const ConstantType biasCorrectedMovingAverage =
            radam->decayingStepAverage[steppingParameter] / (utility::one<ConstantType>() - utility::pow(radam->averageDecay, stepNum + 1));
        const ConstantType squaredAverageDecayPow = utility::pow(radam->squaredAverageDecay, stepNum + 1);
        // 8: Compute the length of the approximated single moving average
        const ConstantType lengthApproxSMA =
            maxLengthApproxSMA -
            ((2 * (utility::convertNumber<ConstantType>(stepNum) + utility::one<ConstantType>()) * squaredAverageDecayPow) / (1 - squaredAverageDecayPow));
        // 9: If the variance is tractable, i.e. lengthApproxSMA > 4, then
        if (lengthApproxSMA > 4) {
            // 10: Compute adaptive learning rate
            const ConstantType adaptiveLearningRate =
                constantTypeSqrt((utility::one<ConstantType>() - squaredAverageDecayPow) / radam->decayingStepAverageSquared[steppingParameter]);
            // 11: Compute the variance rectification term
            const ConstantType varianceRectification =
                constantTypeSqrt(((lengthApproxSMA - 4) / (maxLengthApproxSMA - 4)) * ((lengthApproxSMA - 2) / (maxLengthApproxSMA - 2)) *
                                 ((maxLengthApproxSMA) / (lengthApproxSMA)));
            // 12: Update parameters with adaptive momentum
            step = radam->learningRate * varianceRectification * biasCorrectedMovingAverage * adaptiveLearningRate;
        } else {
            // 14: Update parameters with un-adapted momentum
            step = radam->learningRate * biasCorrectedMovingAverage;
        }
    } else if (RmsProp* rmsProp = boost::get<RmsProp>(&gradientDescentType)) {
        rmsProp->rootMeanSquare[steppingParameter] = rmsProp->averageDecay * rmsProp->rootMeanSquare[steppingParameter] +
                                                     (utility::one<ConstantType>() - rmsProp->averageDecay) * projectedGradient * projectedGradient;

        const ConstantType toSqrt = rmsProp->rootMeanSquare[steppingParameter] + precisionAsConstant;
        ConstantType sqrtResult = constantTypeSqrt(toSqrt);

        step = (rmsProp->learningRate / sqrtResult) * projectedGradient;
    } else if (Plain* plain = boost::get<Plain>(&gradientDescentType)) {
        if (useSignsOnly) {
            if (projectedGradient < utility::zero<ConstantType>()) {
                step = -plain->learningRate;
            } else if (projectedGradient > utility::zero<ConstantType>()) {
                step = plain->learningRate;
            } else {
                step = utility::zero<ConstantType>();
            }
        } else {
            step = plain->learningRate * projectedGradient;
        }
    } else if (Momentum* momentum = boost::get<Momentum>(&gradientDescentType)) {
        if (useSignsOnly) {
            if (projectedGradient < utility::zero<ConstantType>()) {
                step = -momentum->learningRate;
            } else if (projectedGradient > utility::zero<ConstantType>()) {
                step = momentum->learningRate;
            } else {
                step = utility::zero<ConstantType>();
            }
        } else {
            step = momentum->learningRate * projectedGradient;
        }
        step += momentum->momentumTerm * momentum->pastStep.at(steppingParameter);
        momentum->pastStep[steppingParameter] = step;
    } else if (Nesterov* nesterov = boost::get<Nesterov>(&gradientDescentType)) {
        if (useSignsOnly) {
            if (projectedGradient < utility::zero<ConstantType>()) {
                step = -nesterov->learningRate;
            } else if (projectedGradient > utility::zero<ConstantType>()) {
                step = nesterov->learningRate;
            } else {
                step = utility::zero<ConstantType>();
            }
        } else {
            step = nesterov->learningRate * projectedGradient;
        }
        step += nesterov->momentumTerm * nesterov->pastStep.at(steppingParameter);
        nesterov->pastStep[steppingParameter] = step;
    } else {
        STORM_LOG_ERROR("GradientDescentType was not a known one");
    }

    const CoefficientType<FunctionType> convertedStep = utility::convertNumber<CoefficientType<FunctionType>>(step);
    const CoefficientType<FunctionType> newPos = position[steppingParameter] + convertedStep;
    position[steppingParameter] = newPos;
    // Map parameter back to region
    if (constraintMethod == GradientDescentConstraintMethod::PROJECT || constraintMethod == GradientDescentConstraintMethod::PROJECT_WITH_GRADIENT) {
        auto const lower = region ? region->getLowerBoundary(steppingParameter) : utility::zero<CoefficientType<FunctionType>>() + precision;
        auto const upper = region ? region->getUpperBoundary(steppingParameter) : utility::one<CoefficientType<FunctionType>>() - precision;

        position[steppingParameter] = utility::max(lower, position[steppingParameter]);
        position[steppingParameter] = utility::min(upper, position[steppingParameter]);
    }
    return utility::abs<ConstantType>(oldPosAsConstant - utility::convertNumber<ConstantType>(position[steppingParameter]));
}

template<typename FunctionType, typename ConstantType>
ConstantType GradientDescentInstantiationSearcher<FunctionType, ConstantType>::stochasticGradientDescent(
    std::map<VariableType<FunctionType>, CoefficientType<FunctionType>>& position) {
    uint_fast64_t initialStateModel = model.getStates("init").getNextSetIndex(0);

    ConstantType currentValue;
    switch (this->synthesisTask->getBound().comparisonType) {
        case logic::ComparisonType::Greater:
        case logic::ComparisonType::GreaterEqual:
            currentValue = -utility::infinity<ConstantType>();
            break;
        case logic::ComparisonType::Less:
        case logic::ComparisonType::LessEqual:
            currentValue = utility::infinity<ConstantType>();
            break;
    }

    // We count the number of iterations where the value changes less than the threshold, and terminate if it is large enough.
    uint64_t tinyChangeIterations = 0;

    std::map<VariableType<FunctionType>, ConstantType> deltaVector;

    std::vector<VariableType<FunctionType>> parameterEnumeration;
    for (auto parameter : this->parameters) {
        parameterEnumeration.push_back(parameter);
    }

    utility::Stopwatch printUpdateStopwatch;
    printUpdateStopwatch.start();

    // The index to keep track of what parameter(s) to consider next.
    // The "mini-batch", so the parameters to consider, are parameterNum..parameterNum+miniBatchSize-1
    uint_fast64_t parameterNum = 0;
    for (uint_fast64_t stepNum = 0; true; ++stepNum) {
        if (printUpdateStopwatch.getTimeInSeconds() >= 15) {
            printUpdateStopwatch.restart();
            STORM_LOG_PROGRESS("Currently at " << currentValue << "\n");
        }

        std::vector<VariableType<FunctionType>> miniBatch;
        for (uint_fast64_t i = parameterNum; i < std::min((uint_fast64_t)parameterEnumeration.size(), parameterNum + miniBatchSize); i++) {
            miniBatch.push_back(parameterEnumeration[i]);
        }

        ConstantType oldValue = currentValue;
        CoefficientType<FunctionType> const precision = storm::utility::convertNumber<CoefficientType<FunctionType>>(this->env.modelTolerance());

        // If nesterov is enabled, we need to compute the gradient on the predicted position
        std::map<VariableType<FunctionType>, CoefficientType<FunctionType>> nesterovPredictedPosition(position);
        if (Nesterov* nesterov = boost::get<Nesterov>(&gradientDescentType)) {
            CoefficientType<FunctionType> const upperBound = (utility::one<CoefficientType<FunctionType>>() - precision);
            for (auto const& parameter : miniBatch) {
                ConstantType const addedTerm = nesterov->momentumTerm * nesterov->pastStep[parameter];
                nesterovPredictedPosition[parameter] += storm::utility::convertNumber<CoefficientType<FunctionType>>(addedTerm);
                nesterovPredictedPosition[parameter] = utility::max(precision, nesterovPredictedPosition[parameter]);
                nesterovPredictedPosition[parameter] = utility::min(upperBound, nesterovPredictedPosition[parameter]);
            }
        }
        if (constraintMethod == GradientDescentConstraintMethod::LOGISTIC_SIGMOID) {
            // Apply sigmoid function
            for (auto const& parameter : parameters) {
                nesterovPredictedPosition[parameter] =
                    utility::one<CoefficientType<FunctionType>>() /
                    (utility::one<CoefficientType<FunctionType>>() +
                     utility::convertNumber<CoefficientType<FunctionType>>(std::exp(-utility::convertNumber<double>(nesterovPredictedPosition[parameter]))));
            }
        }

        // Compute the value of our position and terminate if it satisfies the bound or is
        // zero or one when computing probabilities. The "valueVector" (just the probability/expected
        // reward for eventually reaching the target from every state) is also used for computing
        // the gradient later. We only need one computation of the "valueVector" per mini-batch.
        //
        // If nesterov is activated, we need to do this twice. First, to check the value of the current position.
        // Second, to compute the valueVector at the nesterovPredictedPosition.
        // If nesterov is deactivated, then nesterovPredictedPosition == position.

        // Are we at a stochastic (in bounds) position?
        bool stochasticPosition = true;
        for (auto const& parameter : parameters) {
            if (nesterovPredictedPosition[parameter] < 0 + precision || nesterovPredictedPosition[parameter] > 1 - precision) {
                stochasticPosition = false;
                break;
            }
        }

        bool computeValue = true;
        if (constraintMethod == GradientDescentConstraintMethod::BARRIER_INFINITY || constraintMethod == GradientDescentConstraintMethod::BARRIER_LOGARITHMIC) {
            if (!stochasticPosition) {
                computeValue = false;
            }
        }

        if (computeValue) {
            std::unique_ptr<storm::modelchecker::CheckResult> intermediateResult = instantiationModelChecker->check(env, nesterovPredictedPosition);
            std::vector<ConstantType> valueVector = intermediateResult->asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
            if (boost::get<Nesterov>(&gradientDescentType)) {
                std::map<VariableType<FunctionType>, CoefficientType<FunctionType>> modelCheckPosition(position);
                if (constraintMethod == GradientDescentConstraintMethod::LOGISTIC_SIGMOID) {
                    for (auto const& parameter : parameters) {
                        modelCheckPosition[parameter] =
                            utility::one<CoefficientType<FunctionType>>() /
                            (utility::one<CoefficientType<FunctionType>>() +
                             utility::convertNumber<CoefficientType<FunctionType>>(std::exp(-utility::convertNumber<double>(modelCheckPosition[parameter]))));
                    }
                }
                std::unique_ptr<storm::modelchecker::CheckResult> terminationResult = instantiationModelChecker->check(env, modelCheckPosition);
                std::vector<ConstantType> terminationValueVector = terminationResult->asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
                currentValue = terminationValueVector[initialStateModel];
            } else {
                currentValue = valueVector[initialStateModel];
            }

            if (synthesisTask->getBound().isSatisfied(currentValue) && stochasticPosition) {
                break;
            }

            for (auto const& parameter : miniBatch) {
                auto checkResult = derivativeEvaluationHelper->check(env, nesterovPredictedPosition, parameter, valueVector);
                ConstantType delta = checkResult->getValueVector()[derivativeEvaluationHelper->getInitialState()];
                if (synthesisTask->getBound().comparisonType == logic::ComparisonType::Less ||
                    synthesisTask->getBound().comparisonType == logic::ComparisonType::LessEqual) {
                    delta = -delta;
                }
                deltaVector[parameter] = delta;
            }
        } else {
            if (synthesisTask->getBound().comparisonType == logic::ComparisonType::Less ||
                synthesisTask->getBound().comparisonType == logic::ComparisonType::LessEqual) {
                currentValue = utility::infinity<ConstantType>();
            } else {
                currentValue = -utility::infinity<ConstantType>();
            }
        }

        // Log position and probability information for later use in visualizing the descent, if wished.
        if (recordRun) {
            VisualizationPoint point;
            point.position = nesterovPredictedPosition;
            point.value = currentValue;
            walk.push_back(point);
        }

        // Perform the step. The actualChange is the change in position the step caused. This is different from the
        // delta in multiple ways: First, it's multiplied with the learning rate and stuff. Second, if the current value
        // is at epsilon, and the delta would step out of the constrained which is then corrected, the actualChange is the
        // change from the last to the current corrected position (so might be zero while the delta is not).
        for (auto const& parameter : miniBatch) {
            doStep(parameter, position, deltaVector, stepNum);
        }

        if (storm::utility::abs<ConstantType>(oldValue - currentValue) < terminationEpsilon) {
            tinyChangeIterations += miniBatch.size();
            if (tinyChangeIterations > parameterEnumeration.size()) {
                break;
            }
        } else {
            tinyChangeIterations = 0;
        }

        // Consider the next parameter
        parameterNum = parameterNum + miniBatchSize;
        if (parameterNum >= parameterEnumeration.size()) {
            parameterNum = 0;
        }

        if (storm::utility::resources::isTerminate()) {
            STORM_LOG_WARN("Aborting Gradient Descent, returning non-optimal value.");
            break;
        }
    }
    return currentValue;
}

template<typename FunctionType, typename ConstantType>
std::pair<std::map<VariableType<FunctionType>, CoefficientType<FunctionType>>, ConstantType>
GradientDescentInstantiationSearcher<FunctionType, ConstantType>::gradientDescent() {
    STORM_LOG_ASSERT(this->synthesisTask, "Call setup before calling gradientDescent.");

    resetDynamicValues();

    STORM_LOG_ASSERT(this->synthesisTask->isBoundSet(), "Task does not involve a bound.");

    std::map<VariableType<FunctionType>, CoefficientType<FunctionType>> bestInstantiation;
    ConstantType bestValue;
    switch (this->synthesisTask->getBound().comparisonType) {
        case logic::ComparisonType::Greater:
        case logic::ComparisonType::GreaterEqual:
            bestValue = -utility::infinity<ConstantType>();
            break;
        case logic::ComparisonType::Less:
        case logic::ComparisonType::LessEqual:
            bestValue = utility::infinity<ConstantType>();
            break;
    }

    std::random_device device;
    std::default_random_engine engine(device());
    std::uniform_real_distribution<> dist(0, 1);
    bool initialGuess = true;
    std::map<VariableType<FunctionType>, CoefficientType<FunctionType>> point;
    while (true) {
        STORM_LOG_PROGRESS("Trying out a new starting point\n");
        if (initialGuess) {
            STORM_LOG_PROGRESS("Trying initial guess (p->0.5 for every parameter p or set start point)\n");
        }
        // Generate random starting point
        for (auto const& param : this->parameters) {
            if (initialGuess) {
                logarithmicBarrierTerm = utility::convertNumber<ConstantType>(0.1);
                if (startPoint) {
                    point[param] = (*startPoint)[param];
                } else {
                    point[param] = utility::convertNumber<CoefficientType<FunctionType>>(0.5 + 1e-6);
                }
            } else if (!initialGuess && constraintMethod == GradientDescentConstraintMethod::BARRIER_LOGARITHMIC &&
                       logarithmicBarrierTerm > utility::convertNumber<ConstantType>(0.00001)) {
                // Do nothing
            } else {
                logarithmicBarrierTerm = utility::convertNumber<ConstantType>(0.1);
                point[param] = utility::convertNumber<CoefficientType<FunctionType>>(dist(engine));
            }
        }
        initialGuess = false;

        /* walk.clear(); */

        stochasticWatch.start();
        STORM_LOG_PROGRESS("Starting at " << point << "\n");
        ConstantType prob = stochasticGradientDescent(point);
        stochasticWatch.stop();

        bool isFoundPointBetter = false;
        switch (this->synthesisTask->getBound().comparisonType) {
            case logic::ComparisonType::Greater:
            case logic::ComparisonType::GreaterEqual:
                isFoundPointBetter = prob > bestValue;
                break;
            case logic::ComparisonType::Less:
            case logic::ComparisonType::LessEqual:
                isFoundPointBetter = prob < bestValue;
                break;
        }
        if (isFoundPointBetter) {
            bestInstantiation = point;
            bestValue = prob;
        }

        if (synthesisTask->getBound().isSatisfied(bestValue)) {
            STORM_LOG_PROGRESS("Aborting because the bound is satisfied\n");
            break;
        } else if (storm::utility::resources::isTerminate()) {
            break;
        } else {
            if (constraintMethod == GradientDescentConstraintMethod::BARRIER_LOGARITHMIC) {
                logarithmicBarrierTerm = logarithmicBarrierTerm / 10;
                STORM_LOG_PROGRESS("Smaller term\n" << bestValue << "\n" << logarithmicBarrierTerm << "\n");
                continue;
            }
            STORM_LOG_PROGRESS("Sorry, couldn't satisfy the bound (yet). Best found value so far: " << bestValue << "\n");
            continue;
        }
    }

    if (constraintMethod == GradientDescentConstraintMethod::LOGISTIC_SIGMOID) {
        // Apply sigmoid function
        for (auto const& parameter : parameters) {
            bestInstantiation[parameter] =
                utility::one<CoefficientType<FunctionType>>() /
                (utility::one<CoefficientType<FunctionType>>() +
                 utility::convertNumber<CoefficientType<FunctionType>>(std::exp(-utility::convertNumber<double>(bestInstantiation[parameter]))));
        }
    }

    return std::make_pair(bestInstantiation, bestValue);
}

template<typename FunctionType, typename ConstantType>
void GradientDescentInstantiationSearcher<FunctionType, ConstantType>::resetDynamicValues() {
    if (Adam* adam = boost::get<Adam>(&gradientDescentType)) {
        for (auto const& parameter : this->parameters) {
            adam->decayingStepAverage[parameter] = utility::zero<ConstantType>();
            adam->decayingStepAverageSquared[parameter] = utility::zero<ConstantType>();
        }
    } else if (RAdam* radam = boost::get<RAdam>(&gradientDescentType)) {
        for (auto const& parameter : this->parameters) {
            radam->decayingStepAverage[parameter] = utility::zero<ConstantType>();
            radam->decayingStepAverageSquared[parameter] = utility::zero<ConstantType>();
        }
    } else if (RmsProp* rmsProp = boost::get<RmsProp>(&gradientDescentType)) {
        for (auto const& parameter : this->parameters) {
            rmsProp->rootMeanSquare[parameter] = utility::zero<ConstantType>();
        }
    } else if (Momentum* momentum = boost::get<Momentum>(&gradientDescentType)) {
        for (auto const& parameter : this->parameters) {
            momentum->pastStep[parameter] = utility::zero<ConstantType>();
        }
    } else if (Nesterov* nesterov = boost::get<Nesterov>(&gradientDescentType)) {
        for (auto const& parameter : this->parameters) {
            nesterov->pastStep[parameter] = utility::zero<ConstantType>();
        }
    }
}

template<typename FunctionType, typename ConstantType>
void GradientDescentInstantiationSearcher<FunctionType, ConstantType>::printRunAsJson() {
    // This emits a JSON document to stdout for external data collection, not a log message.
    std::cout << "[";
    for (auto s = walk.begin(); s != walk.end(); ++s) {
        std::cout << "{";
        auto point = s->position;
        for (auto iter = point.begin(); iter != point.end(); ++iter) {
            std::cout << "\"" << iter->first.name() << "\"";
            std::cout << ":" << utility::convertNumber<double>(iter->second) << ",";
        }
        std::cout << "\"value\":" << s->value << "}";
        if (std::next(s) != walk.end()) {
            std::cout << ",";
        }
    }
    std::cout << "]\n";
    // Print value at last step for data collection
    std::cout << storm::utility::convertNumber<double>(walk.at(walk.size() - 1).value) << "\n";
}

template<typename FunctionType, typename ConstantType>
std::vector<typename GradientDescentInstantiationSearcher<FunctionType, ConstantType>::VisualizationPoint>
GradientDescentInstantiationSearcher<FunctionType, ConstantType>::getVisualizationWalk() {
    return walk;
}

template class GradientDescentInstantiationSearcher<RationalFunction, RationalNumber>;
template class GradientDescentInstantiationSearcher<RationalFunction, double>;
}  // namespace derivative
}  // namespace storm

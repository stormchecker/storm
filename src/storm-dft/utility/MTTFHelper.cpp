#include "storm-dft/utility/MTTFHelper.h"
#include "storm-dft/modelchecker/DftModularizationChecker.h"

namespace {

/**
 * Fills Vector buffer with elements which are
 * stepsize apart starting with the given start
 *
 * \note
 * Helperfunction for MTTFHelper
 */
void linspace(std::vector<double> &buffer, double start, double const stepsize) {
    for (size_t i{0}; i < buffer.size(); ++i) {
        buffer[i] = i * stepsize + start;
    }
}

}  // namespace

namespace storm::dft {
namespace utility {

double MTTFHelperProceeding(std::shared_ptr<storm::dft::storage::DFT<double>> const dft, double const stepsize, double const precision) {
    constexpr size_t chunksize{1001};
    storm::dft::modelchecker::DftModularizationChecker<double> checker{dft};

    std::vector<double> timepoints{};
    timepoints.resize(chunksize);
    linspace(timepoints, 0.0, stepsize);
    std::vector<double> probabilities{checker.getProbabilitiesAtTimepoints(timepoints)};

    double delta{1.0};
    double rval{0.0};

    double y1{0.0}, y2{0.0}, y3{1.0 - probabilities[0]};
    size_t i{1};
    auto currentStepsize{stepsize};
    while (std::abs(delta) > precision) {
        if (i + 1 >= probabilities.size()) {
            double const start{timepoints.back()};
            // double stepsize
            if (currentStepsize < 1e-3) {
                currentStepsize *= 2;
            }
            linspace(timepoints, start, currentStepsize);
            probabilities = checker.getProbabilitiesAtTimepoints(timepoints);
            i = 1;
        }

        y1 = y3;
        y2 = 1.0 - probabilities[i];
        y3 = 1.0 - probabilities[i + 1];
        i += 2;

        delta = y1 + 4.0 * y2 + y3;
        delta /= 3.0;
        delta *= currentStepsize;
        rval += delta;
    }

    return rval;
}

double MTTFHelperVariableChange(std::shared_ptr<storm::dft::storage::DFT<double>> const dft, double const stepsize) {
    constexpr size_t chunksize{1001};
    storm::dft::modelchecker::DftModularizationChecker<double> checker{dft};

    std::vector<double> timepoints{};
    timepoints.resize(static_cast<size_t>(1 / stepsize) - 1);
    for (size_t i{0}; i < timepoints.size(); ++i) {
        double x = (i + 1) * stepsize;
        x = x / (1 - x);
        timepoints[i] = x;
    }

    std::vector<double> probabilities{checker.getProbabilitiesAtTimepoints(timepoints, chunksize)};

    double rval{0};
    for (size_t i{0}; i < probabilities.size(); ++i) {
        double x{(i + 1) * stepsize};
        x = 1 / (1 - x);
        x = x * x;
        auto &p{probabilities[i]};
        p = (1 - p) * x;

        rval += p;
    }
    // remove the ends
    rval -= (probabilities.front() + probabilities.back()) / 2;
    // resize
    rval *= stepsize;

    return rval;
}

}  // namespace utility
}  // namespace storm::dft

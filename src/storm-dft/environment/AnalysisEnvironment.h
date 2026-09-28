#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace storm::dft {

namespace builder {
enum class ApproximationHeuristic;
}

/*!
 * Environment configuring the DFT analysis.
 */
class AnalysisEnvironment {
   public:
    AnalysisEnvironment();
    ~AnalysisEnvironment();

    bool isUseModularisation() const;
    void setUseModularisation(bool value);

    bool isSolveWithSMT() const;
    void setSolveWithSMT(bool value);

    size_t getChunksize() const;
    void setChunksize(size_t value);

    double getMttfPrecision() const;
    void setMttfPrecision(double value);

    double getMttfStepsize() const;
    void setMttfStepsize(double value);

    std::string const& getMttfAlgorithm() const;
    void setMttfAlgorithm(std::string const& value);

    bool isApproximationErrorSet() const;
    double getApproximationError() const;
    void setApproximationError(double value);
    void unsetApproximationError();

    storm::dft::builder::ApproximationHeuristic getApproximationHeuristic() const;
    void setApproximationHeuristic(storm::dft::builder::ApproximationHeuristic value);

   private:
    bool useModularisation;
    bool solveWithSMT;
    size_t chunksize;
    double mttfPrecision;
    double mttfStepsize;
    std::string mttfAlgorithm;
    std::optional<double> approximationError;
    storm::dft::builder::ApproximationHeuristic approximationHeuristic;
};

}  // namespace storm::dft

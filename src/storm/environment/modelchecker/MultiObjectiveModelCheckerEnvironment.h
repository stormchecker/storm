#pragma once

#include <string>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelCheckingMethod.h"
#include "storm/storage/SchedulerClass.h"

namespace storm {

class MultiObjectiveModelCheckerEnvironment {
   public:
    enum class PrecisionType {
        Absolute,       /// Absolute precision
        RelativeToDiff  /// Relative to the difference between largest and smallest objective value(s)
    };

    enum class EncodingType {
        Auto,     /// Pick automatically
        Classic,  /// The classic backwards encoding
        Flow      /// The encoding as a flow network
    };

    MultiObjectiveModelCheckerEnvironment();
    ~MultiObjectiveModelCheckerEnvironment();

    storm::modelchecker::multiobjective::MultiObjectiveMethod const& getMethod() const;
    void setMethod(storm::modelchecker::multiobjective::MultiObjectiveMethod value);

    bool isExportPlotSet() const;
    boost::optional<std::string> getPlotPathUnderApproximation() const;
    void setPlotPathUnderApproximation(std::string const& path);
    void unsetPlotPathUnderApproximation();
    boost::optional<std::string> getPlotPathOverApproximation() const;
    void setPlotPathOverApproximation(std::string const& path);
    void unsetPlotPathOverApproximation();
    boost::optional<std::string> getPlotPathParetoPoints() const;
    void setPlotPathParetoPoints(std::string const& path);
    void unsetPlotPathParetoPoints();

    storm::RationalNumber const& getPrecision() const;
    void setPrecision(storm::RationalNumber const& value);
    PrecisionType const& getPrecisionType() const;
    void setPrecisionType(PrecisionType const& value);

    EncodingType const& getEncodingType() const;
    void setEncodingType(EncodingType const& value);

    bool getUseIndicatorConstraints() const;
    void setUseIndicatorConstraints(bool value);

    bool getUseBsccOrderEncoding() const;
    void setUseBsccOrderEncoding(bool value);

    bool getUseRedundantBsccConstraints() const;
    void setUseRedundantBsccConstraints(bool value);

    /*!
     * Configure approximation tradeoff between accuracy of weighted sum optimization vs. Pareto curve exploration.
     * See gamma in https://doi.org/10.18154/RWTH-2023-09669, Alg. 3.3. We must have 0 < gamma < 1.
     * A small value means that weighted sum optimization is done with higher accuracy, which can lead to convergence of Pareto exploration within fewer steps.
     * If no value is set explicitly, a value is set heuristically based on precision requirements.
     */
    bool isApproximationTradeoffSet() const;
    storm::RationalNumber const& getApproximationTradeoff() const;
    void setApproximationTradeoff(storm::RationalNumber const& value);
    void unsetApproximationTradeoff();

    bool isMaxStepsSet() const;
    uint64_t const& getMaxSteps() const;
    void setMaxSteps(uint64_t const& value);
    void unsetMaxSteps();

    bool isSchedulerRestrictionSet() const;
    storm::storage::SchedulerClass const& getSchedulerRestriction() const;
    void setSchedulerRestriction(storm::storage::SchedulerClass const& value);
    void unsetSchedulerRestriction();

    bool isPrintResultsSet() const;
    void setPrintResults(bool value);

   private:
    storm::modelchecker::multiobjective::MultiObjectiveMethod method;
    boost::optional<std::string> plotPathUnderApprox, plotPathOverApprox, plotPathParetoPoints;
    storm::RationalNumber precision;
    PrecisionType precisionType;
    EncodingType encodingType;
    bool indicatorConstraints;
    bool bsccOrderEncoding;
    bool redundantBsccConstraints;
    boost::optional<uint64_t> maxSteps;
    boost::optional<storm::RationalNumber> approximationTradeoff;
    boost::optional<storm::storage::SchedulerClass> schedulerRestriction;
    bool printResults;
};
}  // namespace storm

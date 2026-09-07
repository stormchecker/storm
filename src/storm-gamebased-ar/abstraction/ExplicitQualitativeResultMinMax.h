#pragma once

#include "storm-gamebased-ar/abstraction/QualitativeResultMinMax.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm::gbar {
namespace abstraction {
class ExplicitQualitativeResult;
class ExplicitQualitativeGameResultMinMax;

class ExplicitQualitativeResultMinMax : public QualitativeResultMinMax {
   public:
    ExplicitQualitativeResultMinMax() = default;

    virtual bool isExplicit() const override;

    ExplicitQualitativeResult const& getProb0Min() const;
    ExplicitQualitativeResult const& getProb1Min() const;
    ExplicitQualitativeResult const& getProb0Max() const;
    ExplicitQualitativeResult const& getProb1Max() const;
    ExplicitQualitativeResult& getProb0Min();
    ExplicitQualitativeResult& getProb1Min();
    ExplicitQualitativeResult& getProb0Max();
    ExplicitQualitativeResult& getProb1Max();

    virtual ExplicitQualitativeResult const& getProb0(storm::OptimizationDirection const& dir) const = 0;
    virtual ExplicitQualitativeResult const& getProb1(storm::OptimizationDirection const& dir) const = 0;
    virtual ExplicitQualitativeResult& getProb0(storm::OptimizationDirection const& dir) = 0;
    virtual ExplicitQualitativeResult& getProb1(storm::OptimizationDirection const& dir) = 0;
};

}  // namespace abstraction
}  // namespace storm::gbar

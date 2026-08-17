#pragma once

namespace storm {
class MinMaxLpSolverEnvironment {
   public:
    MinMaxLpSolverEnvironment();
    virtual ~MinMaxLpSolverEnvironment() = default;

    void setUseEqualityForSingleActions(bool newValue);
    void setOptimizeOnlyForInitialState(bool newValue);
    void setUseNonTrivialBounds(bool newValue);

    bool getUseEqualityForSingleActions() const;
    bool getOptimizeOnlyForInitialState() const;
    bool getUseNonTrivialBounds() const;

   private:
    bool useEqualityForSingleActions;
    bool optimizeOnlyForInitialState;
    bool useNonTrivialBounds;
};
}  // namespace storm
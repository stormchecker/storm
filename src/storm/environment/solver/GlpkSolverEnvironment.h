#pragma once

namespace storm {

class GlpkSolverEnvironment {
   public:
    GlpkSolverEnvironment();
    ~GlpkSolverEnvironment();

    double getIntegerTolerance() const;
    void setIntegerTolerance(double value);

    bool isMILPPresolverEnabled() const;
    void setMILPPresolverEnabled(bool value);

    bool isOutputSet() const;
    void setOutput(bool value);

   private:
    double integerTolerance;
    bool milpPresolverEnabled;
    bool output;
};
}  // namespace storm

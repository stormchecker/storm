#pragma once

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/modelchecker/helper/conditional/ConditionalAlgorithmSetting.h"

namespace storm {

class ConditionalModelCheckerEnvironment {
   public:
    ConditionalModelCheckerEnvironment();
    ~ConditionalModelCheckerEnvironment();

    ConditionalAlgorithmSetting getAlgorithm() const;
    void setAlgorithm(ConditionalAlgorithmSetting value);

    storm::RationalNumber getPrecision() const;
    bool isPrecisionSetFromDefault() const;
    void setPrecision(storm::RationalNumber const& value, bool setFromDefault);

    bool isRelativePrecision() const;
    void setRelativePrecision(bool value);

   private:
    ConditionalAlgorithmSetting algorithm;
    storm::RationalNumber precision;
    bool precisionSetFromDefault;
    bool relative;
};

}  // namespace storm

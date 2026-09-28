#pragma once

#include <cstdint>
#include <optional>

namespace storm::dft {

/*!
 * Environment configuring the (Markov) model building from a DFT.
 */
class ModelBuilderEnvironment {
   public:
    ModelBuilderEnvironment();
    ~ModelBuilderEnvironment();

    bool isUseSymmetryReduction() const;
    void setUseSymmetryReduction(bool value);

    bool isAllowDCForRelevantEvents() const;
    void setAllowDCForRelevantEvents(bool value);

    bool isAddLabelsClaiming() const;
    void setAddLabelsClaiming(bool value);

    bool isMaxDepthSet() const;
    uint_fast64_t getMaxDepth() const;
    void setMaxDepth(uint_fast64_t value);
    void unsetMaxDepth();

    bool isTakeFirstDependency() const;
    void setTakeFirstDependency(bool value);

    bool isUniqueFailedBE() const;
    void setUniqueFailedBE(bool value);

   private:
    bool useSymmetryReduction;
    bool allowDCForRelevantEvents;
    bool addLabelsClaiming;
    std::optional<uint_fast64_t> maxDepth;
    bool takeFirstDependency;
    bool uniqueFailedBE;
};

}  // namespace storm::dft

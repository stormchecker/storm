#pragma once

#include "storm/transformer/NonMarkovianChainTransformer.h"

namespace storm::dft {

/*!
 * Environment configuring transformation on the build model.
 */
class TransformationEnvironment {
   public:
    TransformationEnvironment();
    ~TransformationEnvironment();

    bool isUseBisimulation() const;
    void setUseBisimulation(bool value);

    bool isEliminateChains() const;
    void setEliminateChains(bool value);

    storm::transformer::EliminationLabelBehavior getLabelBehavior() const;
    void setLabelBehavior(storm::transformer::EliminationLabelBehavior value);

   private:
    bool useBisimulation;
    bool eliminateChains;
    storm::transformer::EliminationLabelBehavior labelBehavior;
};

}  // namespace storm::dft

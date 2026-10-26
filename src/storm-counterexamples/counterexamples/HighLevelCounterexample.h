#pragma once

#include "storm-counterexamples/counterexamples/Counterexample.h"
#include "storm/languages/SymbolicModelDescription.h"

namespace storm {
namespace counterexamples {

class HighLevelCounterexample : public Counterexample {
   public:
    HighLevelCounterexample(storm::languages::SymbolicModelDescription const& model);

    void writeToStream(std::ostream& out) const override;

    bool isPrismHighLevelCounterexample() const;
    bool isJaniHighLevelCounterexample() const;

    storm::languages::SymbolicModelDescription const& getModelDescription() const;

   private:
    storm::languages::SymbolicModelDescription model;
};

}  // namespace counterexamples
}  // namespace storm

#include "HighLevelCounterexample.h"

namespace storm {
namespace counterexamples {

HighLevelCounterexample::HighLevelCounterexample(storm::storage::SymbolicModelDescription const& model) : model(model) {
    // Intentionally left empty.
}

bool HighLevelCounterexample::isPrismHighLevelCounterexample() const {
    return model.isPrismProgram();
}

bool HighLevelCounterexample::isJaniHighLevelCounterexample() const {
    return model.isJaniModel();
}

storm::storage::SymbolicModelDescription const& HighLevelCounterexample::getModelDescription() const {
    return model;
}

void HighLevelCounterexample::writeToStream(std::ostream& out) const {
    out << "High-level counterexample: \n";
    out << model;
}

}  // namespace counterexamples
}  // namespace storm

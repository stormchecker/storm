#include "storm-counterexamples/counterexamples/PathCounterexample.h"

#include "storm/io/export.h"

namespace storm {
namespace counterexamples {

template<typename ValueType>
PathCounterexample<ValueType>::PathCounterexample(std::shared_ptr<storm::models::sparse::Model<ValueType>> model) : model(model) {
    // Intentionally left empty.
}

template<typename ValueType>
void PathCounterexample<ValueType>::addPath(std::vector<storage::sparse::state_type> path, size_t k) {
    if (k >= shortestPaths.size()) {
        shortestPaths.resize(k);
    }
    shortestPaths[k - 1] = path;
}

template<typename ValueType>
void PathCounterexample<ValueType>::writeToStream(std::ostream& out) const {
    out << "Shortest path counterexample with k = " << shortestPaths.size() << " paths: \n";
    for (size_t i = 0; i < shortestPaths.size(); ++i) {
        out << i + 1 << "-shortest path: \n";
        for (auto it = shortestPaths[i].rbegin(); it != shortestPaths[i].rend(); ++it) {
            out << "\tstate " << *it;
            if (model->hasStateValuations()) {
                out << ": " << model->getStateValuations().toString(*it);
            }
            out << ": {";
            storm::io::outputFixedWidth(out, model->getLabelsOfState(*it), 0);
            out << "}\n";
        }
    }
}

template class PathCounterexample<double>;
}  // namespace counterexamples
}  // namespace storm

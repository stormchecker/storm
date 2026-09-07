#pragma once

#include "storm-counterexamples/counterexamples/Counterexample.h"
#include "storm/models/sparse/Model.h"

namespace storm {
namespace counterexamples {

template<typename ValueType>
class PathCounterexample : public Counterexample {
   public:
    PathCounterexample(std::shared_ptr<storm::models::sparse::Model<ValueType>> model);

    void addPath(std::vector<storage::sparse::state_type> path, size_t k);

    void writeToStream(std::ostream& out) const override;

   private:
    std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
    std::vector<std::vector<storage::sparse::state_type>> shortestPaths;
};

}  // namespace counterexamples
}  // namespace storm

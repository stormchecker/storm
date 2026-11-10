#pragma once

#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"

// Forward declare sub-environments
namespace storm::dft {
class AnalysisEnvironment;
class ModelBuilderEnvironment;
class TransformationEnvironment;
}  // namespace storm::dft

namespace storm::dft {
// Avoid implementing ugly copy constructors for environment by using an internal environment.
struct InternalEnvironment {
    SubEnvironment<storm::Environment> coreEnvironment;
    SubEnvironment<AnalysisEnvironment> analysisEnvironment;
    SubEnvironment<ModelBuilderEnvironment> modelBuilderEnvironment;
    SubEnvironment<TransformationEnvironment> transformationEnvironment;
};
}  // namespace storm::dft

// Explicitly instantiated once in DftEnvironments.cpp
// Avoids redundant re-instantiation elsewhere
// (extern template declaration for SubEnvironment<storm::Environment> is in storm/environment/Environment.h)
namespace storm {
extern template class SubEnvironment<storm::dft::InternalEnvironment>;
extern template class SubEnvironment<storm::dft::AnalysisEnvironment>;
extern template class SubEnvironment<storm::dft::ModelBuilderEnvironment>;
extern template class SubEnvironment<storm::dft::TransformationEnvironment>;
}  // namespace storm

namespace storm::dft {

/*!
 * Environment for storm-dft.
 * Combines the core storm::Environment with DFT-specific configuration.
 */
class DftEnvironment {
   public:
    DftEnvironment();
    virtual ~DftEnvironment();
    DftEnvironment(DftEnvironment const& other);
    DftEnvironment& operator=(DftEnvironment const& other);

    storm::Environment& core();
    storm::Environment const& core() const;

    AnalysisEnvironment& analysis();
    AnalysisEnvironment const& analysis() const;
    ModelBuilderEnvironment& modelBuilder();
    ModelBuilderEnvironment const& modelBuilder() const;
    TransformationEnvironment& transformation();
    TransformationEnvironment const& transformation() const;

   private:
    SubEnvironment<InternalEnvironment> internalEnv;
};

}  // namespace storm::dft

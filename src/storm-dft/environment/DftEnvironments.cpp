// Explicit instantiations of DFT environments

#include "storm-dft/environment/AnalysisEnvironment.h"
#include "storm-dft/environment/DftEnvironment.h"
#include "storm-dft/environment/ModelBuilderEnvironment.h"
#include "storm-dft/environment/TransformationEnvironment.h"
#include "storm/environment/SubEnvironment.h"

// SubEnvironment<storm::Environment> is already instantiated CoreEnvironments.cpp

template class storm::SubEnvironment<storm::dft::InternalEnvironment>;

template class storm::SubEnvironment<storm::dft::AnalysisEnvironment>;
template class storm::SubEnvironment<storm::dft::ModelBuilderEnvironment>;
template class storm::SubEnvironment<storm::dft::TransformationEnvironment>;

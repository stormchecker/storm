#pragma once
#include "storm-config.h"

#ifdef STORM_HAVE_GMM

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"

#include <gmm/gmm_kernel.h>

#include <gmm/gmm_iter.h>
#include <gmm/gmm_matrix.h>

#include <gmm/gmm_precond_diagonal.h>
#include <gmm/gmm_precond_ilu.h>
#include <gmm/gmm_solver_bicgstab.h>
#include <gmm/gmm_solver_gmres.h>
#include <gmm/gmm_solver_qmr.h>

#pragma clang diagnostic pop

#endif

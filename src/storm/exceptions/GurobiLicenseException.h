#pragma once

#include "storm/exceptions/BaseException.h"
#include "storm/exceptions/ExceptionMacros.h"

namespace storm::exceptions {
// An exception that occurs if there is a problem with the Gurobi license.
STORM_NEW_EXCEPTION(GurobiLicenseException)

}  // namespace storm::exceptions

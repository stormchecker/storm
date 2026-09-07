#include "QualitativeCheckResult.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
QualitativeCheckResult& QualitativeCheckResult::operator&=(QualitativeCheckResult const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Unable to perform logical 'and' on the two check results.");
}

QualitativeCheckResult& QualitativeCheckResult::operator|=(QualitativeCheckResult const&) {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Unable to perform logical 'or' on the two check results.");
}

void QualitativeCheckResult::complement() {
    STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Unable to perform logical 'not' on the check result.");
}

bool QualitativeCheckResult::isQualitative() const {
    return true;
}
}  // namespace modelchecker
}  // namespace storm

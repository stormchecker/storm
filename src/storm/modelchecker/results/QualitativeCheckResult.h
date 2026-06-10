#ifndef STORM_MODELCHECKER_QUALITATIVECHECKRESULT_H_
#define STORM_MODELCHECKER_QUALITATIVECHECKRESULT_H_

#include <cstdint>

#include "storm/modelchecker/results/CheckResult.h"

namespace storm {
namespace modelchecker {
class QualitativeCheckResult : public CheckResult {
   public:
    virtual ~QualitativeCheckResult() = default;
    virtual QualitativeCheckResult& operator&=(QualitativeCheckResult const& other);
    virtual QualitativeCheckResult& operator|=(QualitativeCheckResult const& other);

    virtual void complement();

    virtual bool existsTrue() const = 0;
    virtual bool forallTrue() const = 0;
    virtual uint64_t count() const = 0;

    virtual bool isQualitative() const override;
};
}  // namespace modelchecker
}  // namespace storm

#endif /* STORM_MODELCHECKER_QUALITATIVECHECKRESULT_H_ */

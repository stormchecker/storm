#pragma once

#include "storm-gamebased-ar/abstraction/QualitativeResult.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace storage {
class BitVector;
}
}  // namespace storm

namespace storm::gbar {
namespace abstraction {

class ExplicitQualitativeGameResult;

class ExplicitQualitativeResult : public QualitativeResult {
   public:
    virtual ~ExplicitQualitativeResult() = default;

    ExplicitQualitativeGameResult& asExplicitQualitativeGameResult();
    ExplicitQualitativeGameResult const& asExplicitQualitativeGameResult() const;

    virtual storm::storage::BitVector const& getStates() const = 0;
};

}  // namespace abstraction
}  // namespace storm::gbar

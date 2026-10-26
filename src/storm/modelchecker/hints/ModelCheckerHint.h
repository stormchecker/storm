#pragma once

#include "storm/dd/DdType.h"

namespace storm {
namespace modelchecker {

template<typename ValueType>
class ExplicitModelCheckerHint;

/*!
 * This class contains information that might accelerate the model checking process.
 * @note The model checker has to make sure whether a given hint is actually applicable and thus a hint might be ignored.
 */
class ModelCheckerHint {
   public:
    ModelCheckerHint() = default;
    virtual ~ModelCheckerHint() = default;

    // Returns true iff this hint does not contain any information.
    virtual bool isEmpty() const;

    // Returns true iff this is an explicit model checker hint.
    virtual bool isExplicitModelCheckerHint() const;

    template<typename ValueType>
    ExplicitModelCheckerHint<ValueType>& asExplicitModelCheckerHint();

    template<typename ValueType>
    ExplicitModelCheckerHint<ValueType> const& asExplicitModelCheckerHint() const;
};

}  // namespace modelchecker
}  // namespace storm

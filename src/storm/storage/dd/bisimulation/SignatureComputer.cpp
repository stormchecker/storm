#include "storm/storage/dd/bisimulation/SignatureComputer.h"

#include "storm/storage/dd/DdManager.h"

#include "storm/models/symbolic/StandardRewardModel.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {
namespace bisimulation {

template<storm::dd::DdType DdType, typename ValueType>
SignatureIterator<DdType, ValueType>::SignatureIterator(SignatureComputer<DdType, ValueType> const& signatureComputer,
                                                        Partition<DdType, ValueType> const& partition)
    : signatureComputer(signatureComputer), partition(partition), position(0) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
bool SignatureIterator<DdType, ValueType>::hasNext() const {
    switch (signatureComputer.getSignatureMode()) {
        case SignatureMode::Qualitative:
        case SignatureMode::Eager:
            return position < 1;
        case SignatureMode::Lazy:
            return position < 2;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Unknown SignatureMode.");
}

template<storm::dd::DdType DdType, typename ValueType>
Signature<DdType, ValueType> SignatureIterator<DdType, ValueType>::next() {
    auto mode = signatureComputer.getSignatureMode();
    STORM_LOG_THROW(
        (mode == SignatureMode::Eager && position < 1) || (mode == SignatureMode::Lazy && position < 2) || (mode == SignatureMode::Qualitative && position < 1),
        storm::exceptions::OutOfRangeException, "Iterator is out of range.");
    Signature<DdType, ValueType> result;

    if (mode == SignatureMode::Eager) {
        if (position == 0) {
            result = signatureComputer.getFullSignature(partition);
        }
    } else if (mode == SignatureMode::Lazy) {
        if (position == 0) {
            result = signatureComputer.getQualitativeSignature(partition);
        } else {
            result = signatureComputer.getFullSignature(partition);
        }
    } else if (mode == SignatureMode::Qualitative) {
        if (position == 0) {
            result = signatureComputer.getQualitativeSignature(partition);
        }
    } else {
        STORM_LOG_ASSERT(false, "Unknown signature mode.");
    }

    ++position;
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
SignatureComputer<DdType, ValueType>::SignatureComputer(storm::models::symbolic::Model<DdType, ValueType> const& model, SignatureMode const& mode,
                                                        bool ensureQualitative)
    : SignatureComputer(model.getTransitionMatrix(), boost::none, model.getColumnVariables(), mode, ensureQualitative) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
SignatureComputer<DdType, ValueType>::SignatureComputer(storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                        std::set<storm::expressions::Variable> const& columnVariables, SignatureMode const& mode,
                                                        bool ensureQualitative)
    : SignatureComputer(transitionMatrix, boost::none, columnVariables, mode, ensureQualitative) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
SignatureComputer<DdType, ValueType>::SignatureComputer(storm::dd::Bdd<DdType> const& qualitativeTransitionMatrix,
                                                        std::set<storm::expressions::Variable> const& columnVariables, SignatureMode const& mode,
                                                        bool ensureQualitative)
    : SignatureComputer(qualitativeTransitionMatrix.template toAdd<ValueType>(), boost::none, columnVariables, mode, ensureQualitative) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType, typename ValueType>
SignatureComputer<DdType, ValueType>::SignatureComputer(storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                        boost::optional<storm::dd::Bdd<DdType>> const& qualitativeTransitionMatrix,
                                                        std::set<storm::expressions::Variable> const& columnVariables, SignatureMode const& mode,
                                                        bool ensureQualitative)
    : transitionMatrix(transitionMatrix), columnVariables(columnVariables), mode(mode), ensureQualitative(ensureQualitative) {
    if (DdType == storm::dd::DdType::Sylvan) {
        this->transitionMatrix =
            this->transitionMatrix.notZero().ite(this->transitionMatrix, this->transitionMatrix.getDdManager().template getAddUndefined<ValueType>());
    }

    if (qualitativeTransitionMatrix) {
        if (DdType == storm::dd::DdType::Sylvan || ensureQualitative) {
            this->transitionMatrix01 = qualitativeTransitionMatrix.get();
        } else {
            this->transitionMatrix01 = qualitativeTransitionMatrix.get().template toAdd<ValueType>();
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
SignatureIterator<DdType, ValueType> SignatureComputer<DdType, ValueType>::compute(Partition<DdType, ValueType> const& partition) {
    return SignatureIterator<DdType, ValueType>(*this, partition);
}

template<storm::dd::DdType DdType, typename ValueType>
void SignatureComputer<DdType, ValueType>::setSignatureMode(SignatureMode const& newMode) {
    this->mode = newMode;
}

template<storm::dd::DdType DdType, typename ValueType>
SignatureMode const& SignatureComputer<DdType, ValueType>::getSignatureMode() const {
    return mode;
}

template<storm::dd::DdType DdType, typename ValueType>
Signature<DdType, ValueType> SignatureComputer<DdType, ValueType>::getFullSignature(Partition<DdType, ValueType> const& partition) const {
    if (partition.storedAsBdd()) {
        if (partition.hasChangedStates()) {
            return Signature<DdType, ValueType>(this->transitionMatrix.multiplyMatrix(partition.asBdd() && partition.changedStatesAsBdd(), columnVariables));
        } else {
            return Signature<DdType, ValueType>(this->transitionMatrix.multiplyMatrix(partition.asBdd(), columnVariables));
        }
    } else {
        if (partition.hasChangedStates()) {
            return Signature<DdType, ValueType>(this->transitionMatrix.multiplyMatrix(partition.asAdd() * partition.changedStatesAsAdd(), columnVariables));
        } else {
            return Signature<DdType, ValueType>(this->transitionMatrix.multiplyMatrix(partition.asAdd(), columnVariables));
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
Signature<DdType, ValueType> SignatureComputer<DdType, ValueType>::getQualitativeSignature(Partition<DdType, ValueType> const& partition) const {
    if (!transitionMatrix01) {
        if (DdType == storm::dd::DdType::Sylvan || this->ensureQualitative) {
            this->transitionMatrix01 = this->transitionMatrix.notZero();
        } else {
            this->transitionMatrix01 = this->transitionMatrix.notZero().template toAdd<ValueType>();
        }
    }

    if (partition.storedAsBdd()) {
        return this->getQualitativeTransitionMatrixAsBdd().andExists(partition.asBdd(), columnVariables).template toAdd<ValueType>();
    } else {
        if (this->qualitativeTransitionMatrixIsBdd()) {
            return Signature<DdType, ValueType>(
                this->getQualitativeTransitionMatrixAsBdd().andExists(partition.asAdd().toBdd(), columnVariables).template toAdd<ValueType>());
        } else {
            return Signature<DdType, ValueType>(this->getQualitativeTransitionMatrixAsAdd().multiplyMatrix(partition.asAdd(), columnVariables));
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
bool SignatureComputer<DdType, ValueType>::qualitativeTransitionMatrixIsBdd() const {
    return transitionMatrix01.get().which() == 0;
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> const& SignatureComputer<DdType, ValueType>::getQualitativeTransitionMatrixAsBdd() const {
    STORM_LOG_ASSERT(this->transitionMatrix01, "Missing qualitative transition matrix.");
    return boost::get<storm::dd::Bdd<DdType>>(this->transitionMatrix01.get());
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Add<DdType, ValueType> const& SignatureComputer<DdType, ValueType>::getQualitativeTransitionMatrixAsAdd() const {
    STORM_LOG_ASSERT(this->transitionMatrix01, "Missing qualitative transition matrix.");
    return boost::get<storm::dd::Add<DdType, ValueType>>(this->transitionMatrix01.get());
}

template class SignatureIterator<storm::dd::DdType::CUDD, double>;
template class SignatureIterator<storm::dd::DdType::Sylvan, double>;
template class SignatureIterator<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SignatureIterator<storm::dd::DdType::Sylvan, storm::RationalFunction>;

template class SignatureComputer<storm::dd::DdType::CUDD, double>;
template class SignatureComputer<storm::dd::DdType::Sylvan, double>;
template class SignatureComputer<storm::dd::DdType::Sylvan, storm::RationalNumber>;
template class SignatureComputer<storm::dd::DdType::Sylvan, storm::RationalFunction>;
}  // namespace bisimulation
}  // namespace dd
}  // namespace storm

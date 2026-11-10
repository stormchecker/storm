#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include "storm/adapters/JsonForward.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/modelchecker/certificates/CertificateKind.h"

namespace storm {

namespace storage {
class BitVector;
}

namespace models {
template<typename ValueType>
class Model;
}

namespace modelchecker {

/*!
 * Certificates can be computed for models with these value types.
 */
template<typename ValueType>
inline constexpr bool SupportsCertificates = std::is_same_v<ValueType, double> || std::is_same_v<ValueType, storm::RationalNumber>;

/*!
 * A certificate for the result of a model checking query.
 * It can be checked independently of how it was computed, i.e., a valid certificate witnesses that the result is correct.
 * @see https://doi.org/10.1007/978-3-031-90653-4_7
 */
template<typename ValueType>
class Certificate {
   public:
    virtual ~Certificate() = default;
    virtual bool checkValidity(storm::models::Model<ValueType> const& model) const = 0;
    virtual storm::json<ValueType> toJson() const = 0;
    virtual std::string summaryString(storm::storage::BitVector const& relevantStates) const = 0;
    virtual std::unique_ptr<Certificate<ValueType>> clone() const = 0;

   protected:
    Certificate(CertificateKind kind) : kind(kind) {}

   private:
    CertificateKind const kind;
};

}  // namespace modelchecker
}  // namespace storm

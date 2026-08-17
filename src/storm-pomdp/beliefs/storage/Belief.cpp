#include "storm-pomdp/beliefs/storage/Belief.h"

#include "storm-pomdp/beliefs/storage/BeliefBuilder.h"
#include "storm-pomdp/beliefs/utility/BeliefNumerics.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/NumberTraits.h"

#include <boost/functional/hash.hpp>

namespace storm::pomdp::beliefs {

template<typename ValueType>
Belief<ValueType>::Belief(BeliefFlatMap<ValueType>&& data, BeliefObservationType&& obs) : data(std::move(data)), obs(std::move(obs)) {
    // Intentionally empty
}

template<typename ValueType>
std::size_t Belief<ValueType>::size() const {
    return data.size();
}

template<typename ValueType>
BeliefStateType Belief<ValueType>::representativeState() const {
    STORM_LOG_ASSERT(!data.empty(), "Empty belief");
    return data.begin()->first;
}

template<typename ValueType>
BeliefObservationType const& Belief<ValueType>::observation() const {
    return obs;
}

template<typename ValueType>
bool Belief<ValueType>::operator==(Belief const& other) const {
    if (obs != other.obs) {
        return false;
    }
    if (data.size() != other.size()) {
        return false;
    }
    static_assert(BeliefFlatMapIsOrdered);
    auto secondIt = other.data.cbegin();
    for (auto const& [state, value] : data) {
        if (state != secondIt->first) {
            return false;
        }
        if (!BeliefNumerics<ValueType>::equal(value, secondIt->second)) {
            return false;
        }
        ++secondIt;
    }
    return true;
}

template<typename ValueType>
std::string Belief<ValueType>::toString(bool convertToDouble) const {
    std::stringstream ss;
    ss << "Belief{ obs:" << obs;
    if (convertToDouble) {
        forEach([&ss](auto const& state, auto const& val) { ss << ", " << state << ":" << storm::utility::convertNumber<double>(val); });
    } else {
        forEach([&ss](auto const& state, auto const& val) { ss << ", " << state << ":" << val; });
    }
    ss << " }";
    return ss.str();
}

template<typename ValueType>
std::size_t Belief<ValueType>::BeliefHash::operator()(Belief const& belief) const {
    auto seed = static_cast<std::size_t>(belief.obs);
    static_assert(BeliefFlatMapIsOrdered);
    belief.forEach([&seed](auto const& state, auto const& val) {
        boost::hash_combine(seed, state);
        boost::hash_combine(seed, BeliefNumerics<ValueType>::valueForHash(val));
    });
    return seed;
}

template class Belief<double>;
template class Belief<storm::RationalNumber>;

}  // namespace storm::pomdp::beliefs

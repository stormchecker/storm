#pragma once
namespace storm::models::sparse {

template<typename ValueType>
class StandardRewardModel;
template<class CValueType, class CRewardModelType = StandardRewardModel<CValueType>>
class Model;
}  // namespace storm::models::sparse
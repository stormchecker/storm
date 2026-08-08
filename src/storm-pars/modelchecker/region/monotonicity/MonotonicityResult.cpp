#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"

#include <sstream>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"

namespace storm {
namespace analysis {

template<typename VariableType>
MonotonicityResult<VariableType>::MonotonicityResult() {
    this->done = false;
    this->somewhereMonotonicity = true;
    this->allMonotonicity = true;
}

template<typename VariableType>
void MonotonicityResult<VariableType>::addMonotonicityResult(VariableType var, MonotonicityResult<VariableType>::Monotonicity mon) {
    monotonicityResult.insert(std::pair<VariableType, MonotonicityResult<VariableType>::Monotonicity>(std::move(var), std::move(mon)));
}

template<typename VariableType>
void MonotonicityResult<VariableType>::updateMonotonicityResult(VariableType var, MonotonicityResult<VariableType>::Monotonicity mon, bool force) {
    STORM_LOG_ASSERT(!isDoneForVar(var), "Already done for variable.");
    if (force) {
        STORM_LOG_ASSERT(mon == MonotonicityResult<VariableType>::Monotonicity::Not, "Expected Not monotonicity for force.");
        if (monotonicityResult.find(var) == monotonicityResult.end()) {
            addMonotonicityResult(std::move(var), std::move(mon));
        } else {
            monotonicityResult[var] = mon;
        }
    } else {
        if (mon == MonotonicityResult<VariableType>::Monotonicity::Not) {
            mon = MonotonicityResult<VariableType>::Monotonicity::Unknown;
        }

        if (monotonicityResult.find(var) == monotonicityResult.end()) {
            addMonotonicityResult(std::move(var), std::move(mon));
        } else {
            auto monRes = monotonicityResult[var];
            if (monRes == MonotonicityResult<VariableType>::Monotonicity::Unknown || monRes == mon ||
                mon == MonotonicityResult<VariableType>::Monotonicity::Constant) {
                return;
            } else if (mon == MonotonicityResult<VariableType>::Monotonicity::Unknown || monRes == MonotonicityResult<VariableType>::Monotonicity::Constant) {
                monotonicityResult[var] = mon;
            } else {
                monotonicityResult[var] = MonotonicityResult<VariableType>::Monotonicity::Unknown;
            }
        }
        if (monotonicityResult[var] == MonotonicityResult<VariableType>::Monotonicity::Unknown) {
            setAllMonotonicity(false);
            setSomewhereMonotonicity(false);
        } else {
            setSomewhereMonotonicity(true);
        }
    }
}

template<typename VariableType>
typename MonotonicityResult<VariableType>::Monotonicity MonotonicityResult<VariableType>::getMonotonicity(VariableType var) const {
    auto itr = monotonicityResult.find(var);
    if (itr != monotonicityResult.end()) {
        return itr->second;
    }
    return Monotonicity::Unknown;
}

template<typename VariableType>
std::map<VariableType, typename MonotonicityResult<VariableType>::Monotonicity> const& MonotonicityResult<VariableType>::getMonotonicityResult() const {
    return monotonicityResult;
}

template<typename VariableType>
std::pair<std::set<VariableType>, std::set<VariableType>> MonotonicityResult<VariableType>::splitVariables(
    std::set<VariableType> const& consideredVariables) const {
    std::set<VariableType> nonMonotoneVariables;
    std::set<VariableType> monotoneVariables;
    for (auto var : consideredVariables) {
        if (isDoneForVar(var)) {
            auto res = getMonotonicity(var);
            if (res == Monotonicity::Not || res == Monotonicity::Unknown) {
                nonMonotoneVariables.insert(var);
            } else {
                monotoneVariables.insert(var);
            }
        } else {
            nonMonotoneVariables.insert(var);
        }
    }
    return std::make_pair(std::move(monotoneVariables), std::move(nonMonotoneVariables));
}

template<typename VariableType>
std::string MonotonicityResult<VariableType>::toString() const {
    std::stringstream stream;
    auto countIncr = 0;
    auto countDecr = 0;
    for (auto res : getMonotonicityResult()) {
        stream << res.first.name() << " " << res.second << "; ";
        countIncr += (res.second == Monotonicity::Incr) ? 1 : 0;
        countDecr += (res.second == Monotonicity::Decr) ? 1 : 0;
    }
    return "#Incr: " + std::to_string(countIncr) + " #Decr: " + std::to_string(countDecr) + "\n" + stream.str();
}

template<typename VariableType>
void MonotonicityResult<VariableType>::setDone(bool done) {
    this->done = done;
}

template<typename VariableType>
void MonotonicityResult<VariableType>::setDoneForVar(VariableType variable) {
    doneVariables.insert(variable);
}

template<typename VariableType>
bool MonotonicityResult<VariableType>::isDone() const {
    return done;
}

template<typename VariableType>
bool MonotonicityResult<VariableType>::isDoneForVar(VariableType var) const {
    return doneVariables.find(var) != doneVariables.end();
}

template<typename VariableType>
void MonotonicityResult<VariableType>::setSomewhereMonotonicity(bool somewhereMonotonicity) {
    this->somewhereMonotonicity = somewhereMonotonicity;
}

template<typename VariableType>
bool MonotonicityResult<VariableType>::existsMonotonicity() {
    if (!somewhereMonotonicity) {
        for (auto itr : monotonicityResult) {
            if (isDoneForVar(itr.first) && itr.second != MonotonicityResult<VariableType>::Monotonicity::Unknown &&
                itr.second != MonotonicityResult<VariableType>::Monotonicity::Not) {
                setSomewhereMonotonicity(true);
                break;
            }
        }
    }
    return monotonicityResult.size() > 0 && somewhereMonotonicity;
}

template<typename VariableType>
void MonotonicityResult<VariableType>::setAllMonotonicity(bool allMonotonicity) {
    this->allMonotonicity = allMonotonicity;
}

template<typename VariableType>
bool MonotonicityResult<VariableType>::isAllMonotonicity() const {
    return allMonotonicity;
}

template<typename VariableType>
std::shared_ptr<MonotonicityResult<VariableType>> MonotonicityResult<VariableType>::copy() const {
    std::shared_ptr<MonotonicityResult<VariableType>> copy = std::make_shared<MonotonicityResult<VariableType>>();
    copy->monotonicityResult = std::map<VariableType, Monotonicity>(monotonicityResult);
    copy->setAllMonotonicity(allMonotonicity);
    copy->setSomewhereMonotonicity(somewhereMonotonicity);
    copy->setDone(done);
    copy->setDoneVariables(doneVariables);
    return copy;
}

template<typename VariableType>
void MonotonicityResult<VariableType>::setDoneVariables(std::set<VariableType> doneVariables) {
    this->doneVariables = doneVariables;
}

template<typename VariableType>
void MonotonicityResult<VariableType>::splitBasedOnMonotonicity(const std::set<VariableType>& consideredVariables, std::set<VariableType>& monotoneIncr,
                                                                std::set<VariableType>& monotoneDecr, std::set<VariableType>& notMonotone) const {
    for (auto& var : consideredVariables) {
        if (!isDoneForVar(var)) {
            notMonotone.insert(var);
        } else {
            auto mon = getMonotonicity(var);
            if (mon == Monotonicity::Unknown || mon == Monotonicity::Not) {
                notMonotone.insert(var);
            } else if (mon == Monotonicity::Incr) {
                monotoneIncr.insert(var);
            } else {
                monotoneDecr.insert(var);
            }
        }
    }
}

template<typename VariableType>
bool MonotonicityResult<VariableType>::isMonotone(VariableType var) const {
    if (monotonicityResult.find(var) == monotonicityResult.end()) {
        return false;
    } else {
        auto monRes = monotonicityResult.at(var);
        return isDoneForVar(var) && (monRes == Monotonicity::Incr || monRes == Monotonicity::Decr || monRes == Monotonicity::Constant);
    }
}

template class MonotonicityResult<storm::RationalFunctionVariable>;
}  // namespace analysis
}  // namespace storm

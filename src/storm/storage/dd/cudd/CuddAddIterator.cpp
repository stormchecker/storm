#include "storm/storage/dd/cudd/CuddAddIterator.h"

#include <cmath>

#include "storm/exceptions/MissingLibraryException.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/DdMetaVariable.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_CUDD
template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>::AddIterator()
    : ddManager(),
      generator(),
      cube(),
      valueAsDouble(),
      isAtEnd(),
      metaVariables(),
      enumerateDontCareMetaVariables(),
      cubeCounter(),
      relevantDontCareDdVariables(),
      currentValuation() {
    // Intentionally left empty.
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>::AddIterator(DdManager<DdType::CUDD> const& ddManager, DdGen* generator, int* cube, ValueType const& value, bool isAtEnd,
                                                  std::set<storm::expressions::Variable> const* metaVariables, bool enumerateDontCareMetaVariables)
    : ddManager(&ddManager),
      generator(generator),
      cube(cube),
      valueAsDouble(storm::utility::convertNumber<double>(value)),
      isAtEnd(isAtEnd),
      metaVariables(metaVariables),
      enumerateDontCareMetaVariables(enumerateDontCareMetaVariables),
      cubeCounter(),
      relevantDontCareDdVariables(),
      currentValuation(ddManager.getExpressionManager().getSharedPointer()) {
    // If the given generator is not yet at its end, we need to create the current valuation from the cube from
    // scratch.
    if (!this->isAtEnd) {
        // And then get ready for treating the first cube.
        this->treatNewCube();
    }
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>::AddIterator(AddIterator<DdType::CUDD, ValueType>&& other)
    : ddManager(other.ddManager),
      generator(other.generator),
      cube(other.cube),
      valueAsDouble(other.valueAsDouble),
      isAtEnd(other.isAtEnd),
      metaVariables(other.metaVariables),
      cubeCounter(other.cubeCounter),
      relevantDontCareDdVariables(other.relevantDontCareDdVariables),
      currentValuation(other.currentValuation) {
    // Null-out the pointers of which we took possession.
    other.cube = nullptr;
    other.generator = nullptr;
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>& AddIterator<DdType::CUDD, ValueType>::operator=(AddIterator<DdType::CUDD, ValueType>&& other) {
    if (this != &other) {
        this->ddManager = other.ddManager;
        this->generator = other.generator;
        this->cube = other.cube;
        this->valueAsDouble = other.valueAsDouble;
        this->isAtEnd = other.isAtEnd;
        this->metaVariables = other.metaVariables;
        this->cubeCounter = other.cubeCounter;
        this->relevantDontCareDdVariables = other.relevantDontCareDdVariables;
        this->currentValuation = other.currentValuation;

        // Null-out the pointers of which we took possession.
        other.cube = nullptr;
        other.generator = nullptr;
    }
    return *this;
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>::~AddIterator() {
    // We free the pointers sind Cudd allocates them using malloc rather than new/delete.
    free(this->cube);
    free(this->generator);
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>& AddIterator<DdType::CUDD, ValueType>::operator++() {
    STORM_LOG_ASSERT(!this->isAtEnd, "Illegally incrementing iterator that is already at its end.");

    // If there were no (relevant) don't cares or we have enumerated all combination, we need to eliminate the
    // found solutions and get the next "first" cube.
    if (this->relevantDontCareDdVariables.empty() || this->cubeCounter >= std::pow(2, this->relevantDontCareDdVariables.size()) - 1) {
        // Get the next cube and check for emptiness.
        cudd::ABDD::NextCube(generator, &cube, &valueAsDouble);
        this->isAtEnd = (Cudd_IsGenEmpty(generator) != 0);

        // In case we are not done yet, we get ready to treat the next cube.
        if (!this->isAtEnd) {
            this->treatNewCube();
        }
    } else {
        // Treat the next solution of the cube.
        this->treatNextInCube();
    }

    return *this;
}

template<typename ValueType>
void AddIterator<DdType::CUDD, ValueType>::treatNextInCube() {
    // Start by increasing the counter and check which bits we need to set/unset in the new valuation.
    ++this->cubeCounter;

    for (uint_fast64_t index = 0; index < this->relevantDontCareDdVariables.size(); ++index) {
        auto const& ddMetaVariable = this->ddManager->getMetaVariable(std::get<0>(this->relevantDontCareDdVariables[index]));
        if (ddMetaVariable.getType() == MetaVariableType::Bool) {
            if ((this->cubeCounter & (1ull << index)) != 0) {
                currentValuation.setBooleanValue(std::get<0>(this->relevantDontCareDdVariables[index]), true);
            } else {
                currentValuation.setBooleanValue(std::get<0>(this->relevantDontCareDdVariables[index]), false);
            }
        } else {
            storm::expressions::Variable const& metaVariable = std::get<0>(this->relevantDontCareDdVariables[index]);
            if ((this->cubeCounter & (1ull << index)) != 0) {
                currentValuation.setBitVectorValue(metaVariable, ((currentValuation.getBitVectorValue(metaVariable) - ddMetaVariable.getLow()) |
                                                                  (1ull << std::get<1>(this->relevantDontCareDdVariables[index]))) +
                                                                     ddMetaVariable.getLow());
            } else {
                currentValuation.setBitVectorValue(metaVariable, ((currentValuation.getBitVectorValue(metaVariable) - ddMetaVariable.getLow()) &
                                                                  ~(1ull << std::get<1>(this->relevantDontCareDdVariables[index]))) +
                                                                     ddMetaVariable.getLow());
            }
        }
    }
}

template<typename ValueType>
void AddIterator<DdType::CUDD, ValueType>::treatNewCube() {
    this->relevantDontCareDdVariables.clear();

    // Now loop through the bits of all meta variables and check whether they need to be set, not set or are
    // don't cares. In the latter case, we add them to a special list, so we can iterate over their concrete
    // valuations later.
    for (auto const& metaVariable : *this->metaVariables) {
        bool metaVariableAppearsInCube = false;
        std::vector<std::tuple<storm::expressions::Variable, uint_fast64_t>> localRelevantDontCareDdVariables;
        auto const& ddMetaVariable = this->ddManager->getMetaVariable(metaVariable);
        if (ddMetaVariable.getType() == MetaVariableType::Bool) {
            if (this->cube[ddMetaVariable.getDdVariables().front().getIndex()] == 0) {
                metaVariableAppearsInCube = true;
                currentValuation.setBooleanValue(metaVariable, false);
            } else if (this->cube[ddMetaVariable.getDdVariables().front().getIndex()] == 1) {
                metaVariableAppearsInCube = true;
                currentValuation.setBooleanValue(metaVariable, true);
            } else {
                currentValuation.setBooleanValue(metaVariable, false);
                localRelevantDontCareDdVariables.push_back(std::make_tuple(metaVariable, 0));
            }
        } else {
            int_fast64_t intValue = 0;
            for (uint_fast64_t bitIndex = 0; bitIndex < ddMetaVariable.getNumberOfDdVariables(); ++bitIndex) {
                if (cube[ddMetaVariable.getDdVariables()[bitIndex].getIndex()] == 0) {
                    // Leave bit unset.
                    metaVariableAppearsInCube = true;
                } else if (cube[ddMetaVariable.getDdVariables()[bitIndex].getIndex()] == 1) {
                    intValue |= 1ull << (ddMetaVariable.getNumberOfDdVariables() - bitIndex - 1);
                    metaVariableAppearsInCube = true;
                } else {
                    // Temporarily leave bit unset so we can iterate trough the other option later.
                    // Add the bit to the relevant don't care bits.
                    localRelevantDontCareDdVariables.push_back(std::make_tuple(metaVariable, ddMetaVariable.getNumberOfDdVariables() - bitIndex - 1));
                }
            }
            if (this->enumerateDontCareMetaVariables || metaVariableAppearsInCube) {
                currentValuation.setBitVectorValue(metaVariable, intValue + ddMetaVariable.getLow());
            }
        }

        // If all meta variables are to be enumerated or the meta variable appeared in the cube, we register the
        // missing bits to later enumerate all possible valuations.
        if (this->enumerateDontCareMetaVariables || metaVariableAppearsInCube) {
            relevantDontCareDdVariables.insert(relevantDontCareDdVariables.end(), localRelevantDontCareDdVariables.begin(),
                                               localRelevantDontCareDdVariables.end());
        }
    }

    // Finally, reset the cube counter.
    this->cubeCounter = 0;
}

template<typename ValueType>
bool AddIterator<DdType::CUDD, ValueType>::operator==(AddIterator<DdType::CUDD, ValueType> const& other) const {
    if (this->isAtEnd && other.isAtEnd) {
        return true;
    } else {
        return this->ddManager == other.ddManager && this->generator == other.generator && this->cube == other.cube &&
               this->valueAsDouble == other.valueAsDouble && this->isAtEnd == other.isAtEnd && this->metaVariables == other.metaVariables &&
               this->cubeCounter == other.cubeCounter && this->relevantDontCareDdVariables == other.relevantDontCareDdVariables &&
               this->currentValuation == other.currentValuation;
    }
}

template<typename ValueType>
bool AddIterator<DdType::CUDD, ValueType>::operator!=(AddIterator<DdType::CUDD, ValueType> const& other) const {
    return !(*this == other);
}

template<typename ValueType>
std::pair<storm::expressions::SimpleValuation, ValueType> AddIterator<DdType::CUDD, ValueType>::operator*() const {
    return std::make_pair(currentValuation, storm::utility::convertNumber<ValueType>(valueAsDouble));
}

#else
template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>::AddIterator() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>::AddIterator(AddIterator<DdType::CUDD, ValueType>&& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>& AddIterator<DdType::CUDD, ValueType>::operator=(AddIterator<DdType::CUDD, ValueType>&& other) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>::~AddIterator() {}

template<typename ValueType>
AddIterator<DdType::CUDD, ValueType>& AddIterator<DdType::CUDD, ValueType>::operator++() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
bool AddIterator<DdType::CUDD, ValueType>::operator==(AddIterator<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
bool AddIterator<DdType::CUDD, ValueType>::operator!=(AddIterator<DdType::CUDD, ValueType> const& other) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
std::pair<storm::expressions::SimpleValuation, ValueType> AddIterator<DdType::CUDD, ValueType>::operator*() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}
#endif

template class AddIterator<DdType::CUDD, double>;
template class AddIterator<DdType::CUDD, uint_fast64_t>;
template class AddIterator<DdType::CUDD, storm::RationalNumber>;

}  // namespace dd
}  // namespace storm
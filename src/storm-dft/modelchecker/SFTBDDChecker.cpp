#include "storm-dft/modelchecker/SFTBDDChecker.h"

#include <memory>
#include <vector>

#include "storm-dft/transformations/SftToBddTransformator.h"
#include "storm/adapters/eigen.h"

namespace storm::dft {
namespace modelchecker {

using ValueType = SFTBDDChecker::ValueType;
#ifdef STORM_HAVE_SYLVAN
using Bdd = SFTBDDChecker::Bdd;
#endif

#ifdef STORM_HAVE_SYLVAN
namespace {
/**
 * \returns
 * The probability that the bdd is true
 * given the probabilities that the variables are true.
 *
 * \param bdd
 * The bdd for which to calculate the probability
 *
 * \param indexToProbability
 * A reference to a mapping
 * that must map every variable in the bdd to a probability
 *
 * \param bddToProbability
 * A cache for common sub Bdds.
 * Must be empty or from an earlier call with a bdd that is an
 * ancestor of the current one.
 */
ValueType recursiveProbability(Bdd const bdd, std::map<uint32_t, ValueType> const &indexToProbability, std::map<uint64_t, ValueType> &bddToProbability) {
    if (bdd.isOne()) {
        return 1;
    } else if (bdd.isZero()) {
        return 0;
    }

    auto const it{bddToProbability.find(bdd.GetBDD())};
    if (it != bddToProbability.end()) {
        return it->second;
    }

    auto const currentVar{bdd.TopVar()};
    auto const currentProbability{indexToProbability.at(currentVar)};

    auto const thenProbability{recursiveProbability(bdd.Then(), indexToProbability, bddToProbability)};
    auto const elseProbability{recursiveProbability(bdd.Else(), indexToProbability, bddToProbability)};

    // P(Ite(x, f1, f2)) = P(x) * P(f1) + P(!x) * P(f2)
    auto const probability{currentProbability * thenProbability + (1 - currentProbability) * elseProbability};
    bddToProbability[bdd.GetBDD()] = probability;
    return probability;
}

/**
 * \returns
 * The birnbaum importance factor of the given variable
 *
 * \param variableIndex
 * The index of the variable the birnbaum factor should be calculated
 *
 * \param bdd
 * The bdd for which to calculate the factor
 *
 * \param indexToProbability
 * A reference to a mapping
 * that must map every variable in the bdd to a probability
 *
 * \param bddToProbability
 * A cache for common sub Bdds.
 * Must be empty or from an earlier call with a bdd that is an
 * ancestor of the current one.
 *
 * \param bddToBirnbaumFactor
 * A cache for common sub Bdds.
 * Must be empty or from an earlier call with a bdd that is an
 * ancestor of the current one.
 */
ValueType recursiveBirnbaumFactor(uint32_t const variableIndex, Bdd const bdd, std::map<uint32_t, ValueType> const &indexToProbability,
                                  std::map<uint64_t, ValueType> &bddToProbability, std::map<uint64_t, ValueType> &bddToBirnbaumFactor) {
    if (bdd.isTerminal()) {
        return 0;
    }

    auto const it{bddToBirnbaumFactor.find(bdd.GetBDD())};
    if (it != bddToBirnbaumFactor.end()) {
        return it->second;
    }

    auto const currentVar{bdd.TopVar()};
    auto const currentProbability{indexToProbability.at(currentVar)};

    ValueType birnbaumFactor{0};

    if (currentVar > variableIndex) {
        return 0;
    } else if (currentVar == variableIndex) {
        auto const thenProbability{recursiveProbability(bdd.Then(), indexToProbability, bddToProbability)};
        auto const elseProbability{recursiveProbability(bdd.Else(), indexToProbability, bddToProbability)};
        birnbaumFactor = thenProbability - elseProbability;
    } else if (currentVar < variableIndex) {
        auto const thenBirnbaumFactor{recursiveBirnbaumFactor(variableIndex, bdd.Then(), indexToProbability, bddToProbability, bddToBirnbaumFactor)};
        auto const elseBirnbaumFactor{recursiveBirnbaumFactor(variableIndex, bdd.Else(), indexToProbability, bddToProbability, bddToBirnbaumFactor)};

        birnbaumFactor = currentProbability * thenBirnbaumFactor + (1 - currentProbability) * elseBirnbaumFactor;
    }

    bddToBirnbaumFactor[bdd.GetBDD()] = birnbaumFactor;
    return birnbaumFactor;
}

/**
 * \returns
 * The probabilities that the bdd is true
 * given the probabilities that the variables are true.
 *
 * \param chunksize
 * The width of the Eigen Arrays
 *
 * \param bdd
 * The bdd for which to calculate the probabilities
 *
 * \param indexToProbabilities
 * A reference to a mapping
 * that must map every variable in the bdd to probabilities
 *
 * \param bddToProbabilities
 * A cache for common sub Bdds.
 * Must be empty or from an earlier call with a bdd that is an
 * ancestor of the current one.
 *
 * \note
 * Great care was made that all pointers
 * are valid elements in bddToProbabilities.
 *
 */
Eigen::ArrayXd const *recursiveProbabilities(size_t const chunksize, Bdd const bdd, std::map<uint32_t, Eigen::ArrayXd> const &indexToProbabilities,
                                             std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> &bddToProbabilities) {
    auto const bddId{bdd.GetBDD()};
    auto const it{bddToProbabilities.find(bddId)};
    if (it != bddToProbabilities.end() && it->second.first) {
        return &it->second.second;
    }

    auto &bddToProbabilitiesElement{bddToProbabilities[bddId]};
    if (bdd.isOne()) {
        bddToProbabilitiesElement.first = true;
        bddToProbabilitiesElement.second = Eigen::ArrayXd::Constant(chunksize, 1);
        return &bddToProbabilitiesElement.second;
    } else if (bdd.isZero()) {
        bddToProbabilitiesElement.first = true;
        bddToProbabilitiesElement.second = Eigen::ArrayXd::Constant(chunksize, 0);
        return &bddToProbabilitiesElement.second;
    }

    auto const &thenProbabilities{*recursiveProbabilities(chunksize, bdd.Then(), indexToProbabilities, bddToProbabilities)};
    auto const &elseProbabilities{*recursiveProbabilities(chunksize, bdd.Else(), indexToProbabilities, bddToProbabilities)};

    auto const currentVar{bdd.TopVar()};
    auto const &currentProbabilities{indexToProbabilities.at(currentVar)};

    // P(Ite(x, f1, f2)) = P(x) * P(f1) + P(!x) * P(f2)
    bddToProbabilitiesElement.first = true;
    bddToProbabilitiesElement.second = currentProbabilities * thenProbabilities + (1 - currentProbabilities) * elseProbabilities;
    return &bddToProbabilitiesElement.second;
}

/**
 * \returns
 * The birnbaum importance factors of the given variable
 *
 * \param chunksize
 * The width of the Eigen Arrays
 *
 * \param bdd
 * The bdd for which to calculate the factors
 *
 * \param variableIndex
 * The index of the variable the birnbaum factors should be calculated
 *
 * \param indexToProbability
 * A reference to a mapping
 * that must map every variable in the bdd to a probabilities
 *
 * \param bddToProbability
 * A cache for common sub Bdds.
 * Must be empty or from an earlier call with a bdd that is an
 * ancestor of the current one.
 *
 * \param bddToBirnbaumFactor
 * A cache for common sub Bdds.
 * Must be empty or from an earlier call with a bdd that is an
 * ancestor of the current one.
 */
Eigen::ArrayXd const *recursiveBirnbaumFactors(size_t const chunksize, uint32_t const variableIndex, Bdd const bdd,
                                               std::map<uint32_t, Eigen::ArrayXd> const &indexToProbabilities,
                                               std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> &bddToProbabilities,
                                               std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> &bddToBirnbaumFactors) {
    auto const bddId{bdd.GetBDD()};
    auto const it{bddToBirnbaumFactors.find(bddId)};
    if (it != bddToBirnbaumFactors.end() && it->second.first) {
        return &it->second.second;
    }

    auto &bddToBirnbaumFactorsElement{bddToBirnbaumFactors[bddId]};
    if (bdd.isTerminal() || bdd.TopVar() > variableIndex) {
        // return vector 0;
        bddToBirnbaumFactorsElement.second = Eigen::ArrayXd::Constant(chunksize, 0);
        return &bddToBirnbaumFactorsElement.second;
    }

    auto const currentVar{bdd.TopVar()};
    auto const &currentProbabilities{indexToProbabilities.at(currentVar)};

    if (currentVar == variableIndex) {
        auto const &thenProbabilities{*recursiveProbabilities(chunksize, bdd.Then(), indexToProbabilities, bddToProbabilities)};
        auto const &elseProbabilities{*recursiveProbabilities(chunksize, bdd.Else(), indexToProbabilities, bddToProbabilities)};

        bddToBirnbaumFactorsElement.first = true;
        bddToBirnbaumFactorsElement.second = thenProbabilities - elseProbabilities;
        return &bddToBirnbaumFactorsElement.second;
    }

    // currentVar < variableIndex
    auto const &thenBirnbaumFactors{
        *recursiveBirnbaumFactors(chunksize, variableIndex, bdd.Then(), indexToProbabilities, bddToProbabilities, bddToBirnbaumFactors)};
    auto const &elseBirnbaumFactors{
        *recursiveBirnbaumFactors(chunksize, variableIndex, bdd.Else(), indexToProbabilities, bddToProbabilities, bddToBirnbaumFactors)};

    bddToBirnbaumFactorsElement.first = true;
    bddToBirnbaumFactorsElement.second = currentProbabilities * thenBirnbaumFactors + (1 - currentProbabilities) * elseBirnbaumFactors;
    return &bddToBirnbaumFactorsElement.second;
}
}  // namespace

SFTBDDChecker::SFTBDDChecker(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft, std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager)
    : transformator{std::make_shared<storm::dft::transformations::SftToBddTransformator<ValueType>>(dft, sylvanBddManager)} {}

SFTBDDChecker::SFTBDDChecker(std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> transformator) : transformator{transformator} {}

Bdd SFTBDDChecker::getTopLevelElementBdd() {
    return transformator->transformTopLevel();
}

std::shared_ptr<storm::dft::storage::DFT<ValueType>> SFTBDDChecker::getDFT() const {
    return transformator->getDFT();
}
std::shared_ptr<storm::dft::storage::SylvanBddManager> SFTBDDChecker::getSylvanBddManager() const {
    return transformator->getSylvanBddManager();
}

std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> SFTBDDChecker::getTransformator() const {
    return transformator;
}

std::vector<std::vector<std::string>> SFTBDDChecker::getMinimalCutSets() {
    std::vector<std::vector<uint32_t>> mcs{getMinimalCutSetsAsIndices()};

    std::vector<std::vector<std::string>> rval{};
    rval.reserve(mcs.size());
    while (!mcs.empty()) {
        std::vector<std::string> tmp{};
        tmp.reserve(mcs.back().size());
        for (auto const &be : mcs.back()) {
            tmp.push_back(getSylvanBddManager()->getName(be));
        }
        rval.push_back(std::move(tmp));
        mcs.pop_back();
    }

    return rval;
}

std::vector<std::vector<uint32_t>> SFTBDDChecker::getMinimalCutSetsAsIndices() {
    auto const bdd{getTopLevelElementBdd().Minsol()};

    std::vector<std::vector<uint32_t>> mcs{};
    std::vector<uint32_t> buffer{};
    recursiveMCS(bdd, buffer, mcs);

    return mcs;
}

template<typename FuncType>
void SFTBDDChecker::chunkCalculationTemplate(std::vector<ValueType> const &timepoints, size_t chunksize, FuncType func) const {
    if (chunksize == 0) {
        chunksize = timepoints.size();
    }

    // caches
    auto const basicElements{getDFT()->getBasicElements()};
    std::map<uint32_t, Eigen::ArrayXd> indexToProbabilities{};

    // The current timepoints we calculate with
    Eigen::ArrayXd timepointsArray{chunksize};

    for (size_t currentIndex{0}; currentIndex < timepoints.size(); currentIndex += chunksize) {
        auto const sizeLeft{timepoints.size() - currentIndex};
        if (sizeLeft < chunksize) {
            chunksize = sizeLeft;
            timepointsArray = Eigen::ArrayXd{chunksize};
        }

        // Update current timepoints
        for (size_t i{currentIndex}; i < currentIndex + chunksize; ++i) {
            timepointsArray(i - currentIndex) = timepoints[i];
        }

        // Update the probabilities of the basic elements
        for (auto const &be : basicElements) {
            auto const beIndex{getSylvanBddManager()->getIndex(be->name())};
            // Vectorize known BETypes
            // fallback to getUnreliability() otherwise
            if (be->beType() == storm::dft::storage::elements::BEType::EXPONENTIAL) {
                auto const failureRate{std::static_pointer_cast<storm::dft::storage::elements::BEExponential<ValueType> const>(be)->activeFailureRate()};

                // exponential distribution
                // p(T <= t) = 1 - exp(-lambda*t)
                indexToProbabilities[beIndex] = 1 - (-failureRate * timepointsArray).exp();
            } else {
                auto probabilities{timepointsArray};
                for (size_t i{0}; i < chunksize; ++i) {
                    probabilities(i) = be->getUnreliability(timepointsArray(i));
                }
                indexToProbabilities[beIndex] = probabilities;
            }
        }

        func(chunksize, timepointsArray, indexToProbabilities);
    }
}

ValueType SFTBDDChecker::getProbabilityAtTimebound(Bdd bdd, ValueType timebound) const {
    std::map<uint32_t, ValueType> indexToProbability{};
    for (auto const &be : getDFT()->getBasicElements()) {
        auto const currentIndex{getSylvanBddManager()->getIndex(be->name())};
        indexToProbability[currentIndex] = be->getUnreliability(timebound);
    }

    std::map<uint64_t, ValueType> bddToProbability{};
    auto const probability{recursiveProbability(bdd, indexToProbability, bddToProbability)};
    return probability;
}

std::vector<ValueType> SFTBDDChecker::getProbabilitiesAtTimepoints(Bdd bdd, std::vector<ValueType> const &timepoints, size_t chunksize) const {
    std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> bddToProbabilities{};
    std::vector<ValueType> resultProbabilities{};
    resultProbabilities.reserve(timepoints.size());

    chunkCalculationTemplate(timepoints, chunksize, [&](auto const currentChunksize, auto const &timepointsArray, auto const &indexToProbabilities) {
        // Invalidate bdd cache
        for (auto &i : bddToProbabilities) {
            i.second.first = false;
        }

        // Great care was made so that the pointer returned is always valid
        // and points to an element in bddToProbabilities
        auto const &probabilitiesArray{*recursiveProbabilities(currentChunksize, bdd, indexToProbabilities, bddToProbabilities)};

        // Update result Probabilities
        for (size_t i{0}; i < currentChunksize; ++i) {
            resultProbabilities.push_back(probabilitiesArray(i));
        }
    });

    return resultProbabilities;
}

template<typename FuncType>
ValueType SFTBDDChecker::getImportanceMeasureAtTimebound(std::string const &beName, ValueType timebound, FuncType func) {
    std::map<uint32_t, ValueType> indexToProbability{};
    for (auto const &be : getDFT()->getBasicElements()) {
        auto const currentIndex{getSylvanBddManager()->getIndex(be->name())};
        indexToProbability[currentIndex] = be->getUnreliability(timebound);
    }

    auto const bdd{getTopLevelElementBdd()};
    auto const index{getSylvanBddManager()->getIndex(beName)};
    std::map<uint64_t, ValueType> bddToProbability{};
    std::map<uint64_t, ValueType> bddToBirnbaumFactor{};
    auto const probability{recursiveProbability(bdd, indexToProbability, bddToProbability)};
    auto const birnbaumFactor{recursiveBirnbaumFactor(index, bdd, indexToProbability, bddToProbability, bddToBirnbaumFactor)};
    auto const &beProbability{indexToProbability[index]};

    return func(beProbability, probability, birnbaumFactor);
}

template<typename FuncType>
std::vector<ValueType> SFTBDDChecker::getAllImportanceMeasuresAtTimebound(ValueType timebound, FuncType func) {
    auto const bdd{getTopLevelElementBdd()};

    std::vector<ValueType> resultVector{};
    resultVector.reserve(getDFT()->getBasicElements().size());

    std::map<uint32_t, ValueType> indexToProbability{};
    for (auto const &be : getDFT()->getBasicElements()) {
        auto const currentIndex{getSylvanBddManager()->getIndex(be->name())};
        indexToProbability[currentIndex] = be->getUnreliability(timebound);
    }
    std::map<uint64_t, ValueType> bddToProbability{};

    auto const probability{recursiveProbability(bdd, indexToProbability, bddToProbability)};

    for (auto const &be : getDFT()->getBasicElements()) {
        auto const index{getSylvanBddManager()->getIndex(be->name())};
        std::map<uint64_t, ValueType> bddToBirnbaumFactor{};
        auto const birnbaumFactor{recursiveBirnbaumFactor(index, bdd, indexToProbability, bddToProbability, bddToBirnbaumFactor)};
        auto const &beProbability{indexToProbability[index]};
        resultVector.push_back(func(beProbability, probability, birnbaumFactor));
    }
    return resultVector;
}

template<typename FuncType>
std::vector<ValueType> SFTBDDChecker::getImportanceMeasuresAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize,
                                                                        FuncType func) {
    auto const bdd{getTopLevelElementBdd()};
    std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> bddToProbabilities{};
    std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> bddToBirnbaumFactors{};
    std::vector<ValueType> resultVector{};
    resultVector.reserve(timepoints.size());

    chunkCalculationTemplate(timepoints, chunksize, [&](auto const currentChunksize, auto const &timepointsArray, auto const &indexToProbabilities) {
        // Invalidate bdd caches
        for (auto &i : bddToProbabilities) {
            i.second.first = false;
        }
        for (auto &i : bddToBirnbaumFactors) {
            i.second.first = false;
        }

        // Great care was made so that the pointer returned is always valid
        auto const index{getSylvanBddManager()->getIndex(beName)};
        auto const &probabilitiesArray{*recursiveProbabilities(currentChunksize, bdd, indexToProbabilities, bddToProbabilities)};
        auto const &birnbaumFactorsArray{
            *recursiveBirnbaumFactors(currentChunksize, index, bdd, indexToProbabilities, bddToProbabilities, bddToBirnbaumFactors)};

        auto const &beProbabilitiesArray{indexToProbabilities.at(index)};
        auto const ImportanceMeasureArray{func(beProbabilitiesArray, probabilitiesArray, birnbaumFactorsArray)};

        // Update result Probabilities
        for (size_t i{0}; i < currentChunksize; ++i) {
            resultVector.push_back(ImportanceMeasureArray(i));
        }
    });

    return resultVector;
}

template<typename FuncType>
std::vector<std::vector<ValueType>> SFTBDDChecker::getAllImportanceMeasuresAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize,
                                                                                        FuncType func) {
    auto const bdd{getTopLevelElementBdd()};
    auto const basicElements{getDFT()->getBasicElements()};

    std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> bddToProbabilities{};
    std::unordered_map<uint64_t, std::pair<bool, Eigen::ArrayXd>> bddToBirnbaumFactors{};
    std::vector<std::vector<ValueType>> resultVector{};
    resultVector.resize(getDFT()->getBasicElements().size());
    for (auto &i : resultVector) {
        i.reserve(timepoints.size());
    }

    chunkCalculationTemplate(timepoints, chunksize, [&](auto const currentChunksize, auto const &timepointsArray, auto const &indexToProbabilities) {
        // Invalidate bdd cache
        for (auto &i : bddToProbabilities) {
            i.second.first = false;
        }

        auto const &probabilitiesArray{*recursiveProbabilities(currentChunksize, bdd, indexToProbabilities, bddToProbabilities)};

        for (size_t basicElementIndex{0}; basicElementIndex < basicElements.size(); ++basicElementIndex) {
            auto const &be{basicElements[basicElementIndex]};
            // Invalidate bdd cache
            for (auto &i : bddToBirnbaumFactors) {
                i.second.first = false;
            }

            // Great care was made so that the pointer returned is always
            // valid and points to an element in bddToProbabilities
            auto const index{getSylvanBddManager()->getIndex(be->name())};
            auto const &birnbaumFactorsArray{
                *recursiveBirnbaumFactors(currentChunksize, index, bdd, indexToProbabilities, bddToProbabilities, bddToBirnbaumFactors)};

            auto const &beProbabilitiesArray{indexToProbabilities.at(index)};

            auto const ImportanceMeasureArray{func(beProbabilitiesArray, probabilitiesArray, birnbaumFactorsArray)};

            // Update result Probabilities
            for (size_t i{0}; i < currentChunksize; ++i) {
                resultVector[basicElementIndex].push_back(ImportanceMeasureArray(i));
            }
        }
    });

    return resultVector;
}

namespace {

struct BirnbaumFunctor {
    template<typename T>
    constexpr auto operator()(T const &beProbability, T const &probability, T const &birnbaumFactor) const {
        return birnbaumFactor;
    }
};

struct CIFFunctor {
    template<typename T>
    constexpr T operator()(T const &beProbability, T const &probability, T const &birnbaumFactor) const {
        return (beProbability / probability) * birnbaumFactor;
    }
};

struct DIFFunctor {
    template<typename T>
    constexpr T operator()(T const &beProbability, T const &probability, T const &birnbaumFactor) const {
        return beProbability + (beProbability * (1 - beProbability) * birnbaumFactor) / probability;
    }
};

struct RAWFunctor {
    template<typename T>
    constexpr T operator()(T const &beProbability, T const &probability, T const &birnbaumFactor) const {
        return 1 + ((1 - beProbability) * birnbaumFactor) / probability;
    }
};

struct RRWFunctor {
    template<typename T>
    constexpr T operator()(T const &beProbability, T const &probability, T const &birnbaumFactor) const {
        return probability / (probability - beProbability * birnbaumFactor);
    }
};

}  // namespace

ValueType SFTBDDChecker::getBirnbaumFactorAtTimebound(std::string const &beName, ValueType timebound) {
    return getImportanceMeasureAtTimebound(beName, timebound, BirnbaumFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getAllBirnbaumFactorsAtTimebound(ValueType timebound) {
    return getAllImportanceMeasuresAtTimebound(timebound, BirnbaumFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getBirnbaumFactorsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getImportanceMeasuresAtTimepoints(beName, timepoints, chunksize, BirnbaumFunctor{});
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllBirnbaumFactorsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getAllImportanceMeasuresAtTimepoints(timepoints, chunksize, BirnbaumFunctor{});
}

ValueType SFTBDDChecker::getCIFAtTimebound(std::string const &beName, ValueType timebound) {
    return getImportanceMeasureAtTimebound(beName, timebound, CIFFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getAllCIFsAtTimebound(ValueType timebound) {
    return getAllImportanceMeasuresAtTimebound(timebound, CIFFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getCIFsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getImportanceMeasuresAtTimepoints(beName, timepoints, chunksize, CIFFunctor{});
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllCIFsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getAllImportanceMeasuresAtTimepoints(timepoints, chunksize, CIFFunctor{});
}

ValueType SFTBDDChecker::getDIFAtTimebound(std::string const &beName, ValueType timebound) {
    return getImportanceMeasureAtTimebound(beName, timebound, DIFFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getAllDIFsAtTimebound(ValueType timebound) {
    return getAllImportanceMeasuresAtTimebound(timebound, DIFFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getDIFsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getImportanceMeasuresAtTimepoints(beName, timepoints, chunksize, DIFFunctor{});
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllDIFsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getAllImportanceMeasuresAtTimepoints(timepoints, chunksize, DIFFunctor{});
}

ValueType SFTBDDChecker::getRAWAtTimebound(std::string const &beName, ValueType timebound) {
    return getImportanceMeasureAtTimebound(beName, timebound, RAWFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getAllRAWsAtTimebound(ValueType timebound) {
    return getAllImportanceMeasuresAtTimebound(timebound, RAWFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getRAWsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getImportanceMeasuresAtTimepoints(beName, timepoints, chunksize, RAWFunctor{});
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllRAWsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getAllImportanceMeasuresAtTimepoints(timepoints, chunksize, RAWFunctor{});
}

ValueType SFTBDDChecker::getRRWAtTimebound(std::string const &beName, ValueType timebound) {
    return getImportanceMeasureAtTimebound(beName, timebound, RRWFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getAllRRWsAtTimebound(ValueType timebound) {
    return getAllImportanceMeasuresAtTimebound(timebound, RRWFunctor{});
}

std::vector<ValueType> SFTBDDChecker::getRRWsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getImportanceMeasuresAtTimepoints(beName, timepoints, chunksize, RRWFunctor{});
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllRRWsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    return getAllImportanceMeasuresAtTimepoints(timepoints, chunksize, RRWFunctor{});
}

void SFTBDDChecker::recursiveMCS(Bdd const bdd, std::vector<uint32_t> &buffer, std::vector<std::vector<uint32_t>> &minimalCutSets) const {
    if (bdd.isOne()) {
        minimalCutSets.push_back(buffer);
    } else if (!bdd.isZero()) {
        auto const currentVar{bdd.TopVar()};

        buffer.push_back(currentVar);
        recursiveMCS(bdd.Then(), buffer, minimalCutSets);
        buffer.pop_back();

        recursiveMCS(bdd.Else(), buffer, minimalCutSets);
    }
}
#else
SFTBDDChecker::SFTBDDChecker(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft,
                             std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

SFTBDDChecker::SFTBDDChecker(std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> transformator) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::shared_ptr<storm::dft::storage::DFT<ValueType>> SFTBDDChecker::getDFT() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}
std::shared_ptr<storm::dft::storage::SylvanBddManager> SFTBDDChecker::getSylvanBddManager() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> SFTBDDChecker::getTransformator() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<std::vector<std::string>> SFTBDDChecker::getMinimalCutSets() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<std::vector<uint32_t>> SFTBDDChecker::getMinimalCutSetsAsIndices() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

ValueType SFTBDDChecker::getBirnbaumFactorAtTimebound(std::string const &beName, ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getAllBirnbaumFactorsAtTimebound(ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getBirnbaumFactorsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllBirnbaumFactorsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

ValueType SFTBDDChecker::getCIFAtTimebound(std::string const &beName, ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getAllCIFsAtTimebound(ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getCIFsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllCIFsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

ValueType SFTBDDChecker::getDIFAtTimebound(std::string const &beName, ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getAllDIFsAtTimebound(ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getDIFsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllDIFsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

ValueType SFTBDDChecker::getRAWAtTimebound(std::string const &beName, ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getAllRAWsAtTimebound(ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getRAWsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllRAWsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

ValueType SFTBDDChecker::getRRWAtTimebound(std::string const &beName, ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getAllRRWsAtTimebound(ValueType timebound) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<ValueType> SFTBDDChecker::getRRWsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<std::vector<ValueType>> SFTBDDChecker::getAllRRWsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

#endif

}  // namespace modelchecker
}  // namespace storm::dft

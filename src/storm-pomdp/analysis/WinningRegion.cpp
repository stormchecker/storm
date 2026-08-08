#include "storm-pomdp/analysis/WinningRegion.h"

#include <boost/algorithm/string.hpp>
#include <iostream>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"

namespace storm {
namespace pomdp {
WinningRegion::WinningRegion(std::vector<uint64_t> const& observationSizes) : observationSizes(observationSizes) {
    for (uint64_t i = 0; i < observationSizes.size(); ++i) {
        winningRegion.push_back(std::vector<storm::storage::BitVector>());
    }
}

void WinningRegion::setObservationIsWinning(uint64_t observation) {
    winningRegion[observation] = {storm::storage::BitVector(observationSizes[observation], true)};
}

void WinningRegion::addTargetStates(uint64_t observation, storm::storage::BitVector const& offsets) {
    STORM_LOG_ASSERT(!offsets.empty(), "Offsets should not be empty.");
    if (winningRegion[observation].empty()) {
        winningRegion[observation].push_back(offsets);
        return;
    }
    std::vector<storm::storage::BitVector> newWinningSupport = std::vector<storm::storage::BitVector>();

    for (auto const& support : winningRegion[observation]) {
        newWinningSupport.push_back(support | offsets);
    }
    // TODO it may be worthwhile to check whether something changed. If nothing changed, there is no need for the next routine.
    // TODO the following code is  bit naive.
    winningRegion[observation].clear();  // This prevents some overhead.
    for (auto const& newWinning : newWinningSupport) {
        update(observation, newWinning);
    }
}

bool WinningRegion::update(uint64_t observation, storm::storage::BitVector const& winning) {
    std::vector<storm::storage::BitVector> newWinningSupport = std::vector<storm::storage::BitVector>();
    bool changed = false;
    for (auto const& support : winningRegion[observation]) {
        if (winning.isSubsetOf(support)) {
            // This new winning support is already covered.
            return false;
        }
        if (support.isSubsetOf(winning)) {
            // This new winning support extends the previous support, thus the previous support is now spurious
            changed = true;
        } else {
            newWinningSupport.push_back(support);
        }
    }

    // only if changed.
    if (changed) {
        newWinningSupport.push_back(winning);
        winningRegion[observation] = newWinningSupport;
    } else {
        winningRegion[observation].push_back(winning);
    }
    return true;
}

bool WinningRegion::query(uint64_t observation, storm::storage::BitVector const& currently) const {
    for (storm::storage::BitVector winning : winningRegion[observation]) {
        if (currently.isSubsetOf(winning)) {
            return true;
        }
    }
    return false;
}

storm::expressions::Expression WinningRegion::extensionExpression(uint64_t observation, std::vector<storm::expressions::Expression>& varsForStates) const {
    std::vector<storm::expressions::Expression> expressionForEntry;

    for (auto const& winningForObservation : winningRegion[observation]) {
        if (winningForObservation.full()) {
            STORM_LOG_ASSERT(winningRegion[observation].size() == 1, "Expected single winning set per observation.");
            return varsForStates.front().getManager().boolean(false);
        }
        std::vector<storm::expressions::Expression> subexpr;
        std::vector<storm::expressions::Expression> leftHandSides;
        STORM_LOG_ASSERT(varsForStates.size() == winningForObservation.size(), "Variable count mismatch.");
        for (uint64_t i = 0; i < varsForStates.size(); ++i) {
            if (winningForObservation.get(i)) {
                leftHandSides.push_back(varsForStates[i]);
            } else {
                subexpr.push_back(varsForStates[i]);
            }
        }
        storm::expressions::Expression rightHandSide = storm::expressions::disjunction(subexpr);
        for (auto const& lhs : leftHandSides) {
            expressionForEntry.push_back(storm::expressions::implies(lhs, rightHandSide));
        }
        expressionForEntry.push_back(storm::expressions::disjunction(varsForStates));
    }
    return storm::expressions::conjunction(expressionForEntry);
}

/**
 * If we observe this observation, do we surely win?
 * @param observation
 * @return yes, if all supports for this observation are winning.
 */
bool WinningRegion::observationIsWinning(uint64_t observation) const {
    return winningRegion[observation].size() == 1 && winningRegion[observation].front().full();
}

void WinningRegion::print() const {
    uint64_t observation = 0;
    std::vector<uint64_t> winningObservations;
    std::vector<uint64_t> loosingObservations;

    for (auto const& winningSupport : winningRegion) {
        if (observationIsWinning(observation)) {
            winningObservations.push_back(observation);
        } else if (winningRegion[observation].empty()) {
            loosingObservations.push_back(observation);
        } else {
            std::cout << "***** observation" << observation << '\n';
            for (auto const& support : winningSupport) {
                std::cout << " " << support;
            }
            std::cout << '\n';
        }
        observation++;
    }
    std::cout << "and  " << winningObservations.size() << " winning observations: (";
    for (auto const& obs : winningObservations) {
        std::cout << obs << " ";
    }
    std::cout << ") and " << loosingObservations.size() << " loosing observations: (";
    for (auto const& obs : loosingObservations) {
        std::cout << obs << " ";
    }
}

/**
 * How many different observations are there?
 * @return
 */
uint64_t WinningRegion::getNumberOfObservations() const {
    STORM_LOG_ASSERT(winningRegion.size() == observationSizes.size(), "Winning region size mismatch.");
    return observationSizes.size();
}

bool WinningRegion::empty() const {
    for (auto const& ob : winningRegion) {
        if (!ob.empty()) {
            return false;
        }
    }
    return true;
}

std::vector<storm::storage::BitVector> const& WinningRegion::getWinningSetsPerObservation(uint64_t observation) const {
    STORM_LOG_ASSERT(observation < getNumberOfObservations(), "Observation index out of range.");
    return winningRegion[observation];
}

storm::RationalNumber WinningRegion::beliefSupportStates() const {
    storm::RationalNumber total = 0;
    storm::RationalNumber two = storm::utility::convertNumber<storm::RationalNumber>(2);
    for (auto const& size : observationSizes) {
        total += carl::pow(two, size) - 1;
    }
    return total;
}

std::pair<storm::RationalNumber, storm::RationalNumber> count(std::vector<storm::storage::BitVector> const& origSets,
                                                              std::vector<storm::storage::BitVector> const& intersects,
                                                              std::vector<storm::storage::BitVector> const& intersectsInfo, storm::RationalNumber val,
                                                              bool plus, uint64_t remdepth) {
    STORM_LOG_ASSERT(intersects.size() == intersectsInfo.size(), "Intersect size mismatch.");
    storm::RationalNumber newVal = val;
    storm::RationalNumber two = storm::utility::convertNumber<storm::RationalNumber>(2);
    for (uint64_t i = 0; i < intersects.size(); ++i) {
        if (plus) {
            newVal += carl::pow(two, intersects[i].getNumberOfSetBits());
        } else {
            newVal -= carl::pow(two, intersects[i].getNumberOfSetBits());
        }
    }

    storm::RationalNumber diff = val - newVal;
    storm::RationalNumber max = storm::utility::max(val, newVal);

    diff = storm::utility::abs(diff);
    if (remdepth == 0 || 20 * diff < max) {
        if (plus) {
            return std::make_pair(val, newVal);
        } else {
            return std::make_pair(newVal, val);
        }
    } else {
        storm::RationalNumber skipped = 0;
        uint64_t upperBoundElements = origSets.size() * intersects.size();
        STORM_LOG_DEBUG("Upper bound on number of elements to be considered " << upperBoundElements);
        STORM_LOG_DEBUG("Value " << val << " newVal " << newVal);
        uint64_t oom = 0;
        uint64_t critoom = 0;
        storm::RationalNumber n = 1;
        while (n < max) {
            oom += 1;
            n = n * 2;
        }
        STORM_LOG_DEBUG("Order of magnitude = " << oom);

        critoom = oom - static_cast<uint64_t>(floor(log2(upperBoundElements)));

        STORM_LOG_DEBUG("Crit Order of magnitude = " << critoom);

        uint64_t intersectSetSkip = critoom - static_cast<uint64_t>(floor(log2(origSets.size())));

        std::vector<storm::storage::BitVector> useIntersects;
        std::vector<storm::storage::BitVector> useInfo;
        for (uint64_t i = 0; i < intersects.size(); ++i) {
            if (upperBoundElements > 10000 && intersects[i].getNumberOfSetBits() < intersectSetSkip - 3) {
                skipped += (carl::pow(two, intersects[i].getNumberOfSetBits()) * origSets.size());
                STORM_LOG_DEBUG("Skipped " << skipped);
            } else {
                useIntersects.push_back(intersects[i]);
                useInfo.push_back(intersectsInfo[i]);
            }
        }

        uint64_t origSetSkip = critoom - static_cast<uint64_t>(floor(log2(useIntersects.size())));
        STORM_LOG_DEBUG("OrigSkip= " << origSetSkip);

        std::vector<storm::storage::BitVector> newIntersects;
        std::vector<storm::storage::BitVector> newInfo;

        for (uint64_t i = 0; i < origSets.size(); ++i) {
            if (upperBoundElements > 20000 && origSets[i].getNumberOfSetBits() < origSetSkip - 3) {
                skipped += (carl::pow(two, origSets[i].getNumberOfSetBits()) * useIntersects.size());
                STORM_LOG_DEBUG("Skipped " << skipped);
                continue;
            }
            for (uint64_t j = 0; j < useIntersects.size(); ++j) {
                if (useInfo[j].get(i)) {
                    continue;
                }
                storm::storage::BitVector newinf = useInfo[j];
                newinf.set(i);
                if (newinf == useInfo[j]) {
                    continue;
                }
                bool exists = false;
                for (auto const& entry : newInfo) {
                    if (entry == newinf) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    newInfo.push_back(newinf);
                    newIntersects.push_back(origSets[i] & useIntersects[j]);
                }
            }
        }

        auto res = count(origSets, newIntersects, newInfo, newVal, !plus, remdepth - 1);
        if (plus) {
            storm::RationalNumber tmp = res.first - skipped;
            return std::make_pair(storm::utility::max(tmp, val), res.second);
        } else {
            storm::RationalNumber tmp = res.second + skipped;
            return std::make_pair(res.first, storm::utility::min(tmp, val));
        }
    }
}

std::pair<storm::RationalNumber, storm::RationalNumber> WinningRegion::computeNrWinningBeliefs() const {
    storm::RationalNumber upper = 0;
    storm::RationalNumber lower = 0;
    storm::RationalNumber two = storm::utility::convertNumber<storm::RationalNumber>(2);
    for (auto const& winningSets : winningRegion) {
        storm::RationalNumber totalForObs = 0;
        storm::RationalNumber two = storm::utility::convertNumber<storm::RationalNumber>(2);

        std::vector<storm::storage::BitVector> info;  // which intersections are part of this
        for (uint64_t i = 0; i < winningSets.size(); ++i) {
            storm::storage::BitVector entry(winningSets.size());
            entry.set(i);
            info.push_back(entry);
        }
        auto res = count(winningSets, winningSets, info, totalForObs, true, 40);
        lower += res.first;
        upper += res.second;
    }
    if (lower > 0) {
        lower -= 1;
        upper -= 1;
    }
    return std::make_pair(lower, upper);
}

uint64_t WinningRegion::getStorageSize() const {
    uint64_t result = 0;
    for (uint64_t i = 0; i < getNumberOfObservations(); ++i) {
        result += winningRegion[i].size() * observationSizes[i];
    }
    return result;
}

void WinningRegion::storeToFile(std::string const& path, std::string const& preamble, bool append) const {
    std::ofstream file;
    storm::io::openFile(path, file, append);
    file << ":preamble\n";
    file << preamble << '\n';
    file << ":winningregion\n";
    bool firstLine = true;
    for (auto const& i : observationSizes) {
        if (!firstLine) {
            file << " ";
        } else {
            firstLine = false;
        }
        file << i;
    }
    file << '\n';
    for (auto const& obsWr : winningRegion) {
        for (auto const& bv : obsWr) {
            bv.store(file);
            file << ";";
        }
        file << '\n';
    }
    storm::io::closeFile(file);
}

std::pair<WinningRegion, std::string> WinningRegion::loadFromFile(std::string const& path) {
    std::ifstream file;
    std::vector<uint64_t> observationSizes;
    storm::io::openFile(path, file);
    std::string line;
    uint64_t state = 0;  // 0 = expect preamble
    uint64_t observation = 0;
    WinningRegion wr({1});
    std::stringstream preamblestream;
    while (storm::io::getline(file, line)) {
        if (boost::starts_with(line, "#")) {
            continue;
        }
        if (state == 0) {
            STORM_LOG_THROW(line == ":preamble", storm::exceptions::WrongFormatException, "Expected to see :preamble.");
            state = 1;  // state = 1: preamble
        } else if (state == 1) {
            if (line == ":winningregion") {
                state = 2;  // get sizes
            } else {
                preamblestream << line << '\n';
            }
        } else if (state == 2) {
            std::vector<std::string> entries;
            boost::split(entries, line, boost::is_space());
            std::vector<uint64_t> observationSizes;
            for (auto const& entry : entries) {
                observationSizes.push_back(std::stoul(entry));
            }
            wr = WinningRegion(observationSizes);
            state = 3;
        } else if (state == 3) {
            std::vector<std::string> entries;
            boost::split(entries, line, boost::is_any_of(";"));
            entries.pop_back();
            for (std::string const& bvString : entries) {
                wr.update(observation, storm::storage::BitVector::load(bvString));
            }
            ++observation;
        }
    }
    storm::io::closeFile(file);
    return {wr, preamblestream.str()};
}

}  // namespace pomdp
}  // namespace storm

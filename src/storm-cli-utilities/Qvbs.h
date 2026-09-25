#pragma once

#include <boost/optional.hpp>
#include <cstdint>
#include <string>
#include <vector>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace storage {

/*!
 * This class provides easy access to a benchmark of the Quantitative Verification Benchmark Set
 * http://qcomp.org/benchmarks/
 */
class QvbsBenchmark {
   public:
    /*!
     * @param modelName the (short) model name of the considered model.
     */
    QvbsBenchmark(std::string const& modelName);

    std::string const& getJaniFile(uint64_t instanceIndex = 0) const;
    std::string const& getConstantDefinition(uint64_t instanceIndex = 0) const;

    std::string getInfo(uint64_t instanceIndex = 0, boost::optional<std::vector<std::string>> propertyFilter = boost::none) const;

   private:
    std::vector<std::string> janiFiles;
    std::vector<std::string> constantDefinitions;
    std::vector<std::string> instanceInfos;

    std::string modelPath;
    storm::json<storm::RationalNumber> modelData;
};
}  // namespace storage
}  // namespace storm

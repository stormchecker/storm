#pragma once

#include <ostream>

#include "storm-dft/storage/DFT.h"

namespace storm::dft {
namespace utility {

template<typename ValueType>
class DftValidator {
   public:
    /*!
     * Check whether the DFT is well-formed.
     * The check follows the constraints of a well-formed DFT (Def. 2.15 in [Volk, 2022])
     * @see https://doi.org/10.18154/RWTH-2023-04092
     * A DFT is well-formed if it satisfies several constraints such as
     * - graph is acyclic
     * - valid threshold for VOT
     * - SEQ and PDEP are at least binary and have no parents
     * - etc.
     * @param dft DFT
     * @param stream Output stream which contains additional information if the DFT is well-formed.
     * @return True iff the DFT is well-formed.
     */
    static bool isDftWellFormed(storm::dft::storage::DFT<ValueType> const& dft, std::ostream& stream);

    /*!
     * Check whether the DFT can be analysed by translation to a Markov model, i.e. the DFT is conventional.
     * The check follows the constraints of a conventional DFT (Def. 2.19 in [Volk, 2022])
     * @see https://doi.org/10.18154/RWTH-2023-04092
     * A DFT can be analysed if it satisfies several constraints:
     * - it is well-formed
     * - all BE failure distributions are exponential distributions
     * - spare modules are independent
     * @param dft DFT
     * @param stream Output stream which contains additional information if the DFT is invalid for Markovian analysis.
     * @return True iff the DFT is valid for Markovian analysis.
     */
    static bool isDftValidForMarkovianAnalysis(storm::dft::storage::DFT<ValueType> const& dft, std::ostream& stream);

    /*!
     * Check whether the DFT has potential modeling issues.
     * While these issue do not prevent analysis, they could lead to unexpected analysis results and warnings should be issued.
     * A DFT has modeling issues if:
     * - spare modules overlap with the top module and are therefore already initially activated
     * - spare modules contains the parent SPARE gate and therefore might never be activated.
     */
    static bool hasPotentialModelingIssues(storm::dft::storage::DFT<ValueType> const& dft, std::ostream& stream);
};

}  // namespace utility
}  // namespace storm::dft

#include "storm/automata/APSet.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

#include <exception>
#include <string>

namespace storm {
namespace automata {

APSet::APSet() {
    // intentionally left blank
}

unsigned int APSet::size() const {
    return index_to_ap.size();
}

std::size_t APSet::alphabetSize() const {
    return 1L << size();
}

void APSet::add(const std::string& ap) {
    STORM_LOG_THROW(size() < MAX_APS, storm::exceptions::UnexpectedException,
                    "Set of atomic proposition size is limited to " << std::to_string(MAX_APS) << ".");

    unsigned int index = size();
    bool fresh = ap_to_index.insert(std::make_pair(ap, index)).second;
    STORM_LOG_THROW(fresh, storm::exceptions::UnexpectedException, "Duplicate atomic proposition '" << ap << "' in APSet.");

    index_to_ap.push_back(ap);
}

unsigned int APSet::getIndex(const std::string& ap) const {
    // throws out_of_range if ap is not in the set
    return ap_to_index.at(ap);
}

const std::string& APSet::getAP(unsigned int index) const {
    // throws out_of_range if index is out of range
    return index_to_ap.at(index);
}

const std::vector<std::string>& APSet::getAPs() const {
    return index_to_ap;
}

bool APSet::contains(const std::string& ap) const {
    return ap_to_index.find(ap) != ap_to_index.end();
}

APSet::alphabet_element APSet::elementAllFalse() const {
    return 0;
}

APSet::alphabet_element APSet::elementAddAP(alphabet_element element, unsigned int ap) const {
    STORM_LOG_THROW(ap < size(), storm::exceptions::UnexpectedException, "AP out of range.");

    return element | (1ul << ap);
}

}  // namespace automata
}  // namespace storm

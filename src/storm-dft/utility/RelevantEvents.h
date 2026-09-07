#pragma once

#include <boost/algorithm/string.hpp>
#include <initializer_list>
#include <memory>

#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm-dft/storage/DFT.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/logic/AtomicLabelFormula.h"
#include "storm/logic/Formula.h"
#include "storm/settings/SettingsManager.h"

namespace storm::dft {
namespace utility {

class RelevantEvents {
   public:
    /*!
     * Constructs empty RelevantEvents object
     */
    RelevantEvents() = default;

    /*!
     * Create relevant events from given event names in an initializer list.
     * If name 'all' occurs, all elements are stored as relevant.
     *
     * Allows syntactic sugar like:
     * RelevantEvents e = {};
     * and
     * RelevantEvents e{"a"};
     *
     * @param init The initializer list.
     */
    RelevantEvents(std::initializer_list<std::string> init) {
        insert(init.begin(), init.end());
    }

    /*!
     * Create relevant events from given event names in a range.
     * If name 'all' occurs, all elements are stored as relevant.
     *
     * @param first Iterator pointing to the start of a range of names.
     * @param last Iterator pointing to the end of a range of names.
     */
    template<typename ForwardIt>
    RelevantEvents(ForwardIt first, ForwardIt last) {
        insert(first, last);
    }

    bool operator==(RelevantEvents const& rhs) const {
        return this->allRelevant == rhs.allRelevant || this->names == rhs.names;
    }

    bool operator!=(RelevantEvents const& rhs) const {
        return !(*this == rhs);
    }

    /*!
     * Count the events that are relevant in the given DFT.
     * @note Can be very slow. Uses a naiive O(n^2) implementation.
     *
     * @param dft The DFT to count on.
     */
    template<typename ValueType>
    size_t count(std::shared_ptr<storm::dft::storage::DFT<ValueType>> const dft) const {
        if (this->allRelevant) {
            return dft->nrElements();
        }
        size_t rval{0};
        for (auto const& name : names) {
            if (dft->existsName(name)) {
                rval++;
            }
        }
        return rval;
    }

    /*!
     * Add relevant event names required by the labels in properties of a range.
     *
     * @param first Iterator pointing to the start of a std::shared_ptr<storm::logic::Formula const> range.
     * @param last Iterator pointing to the end of a std::shared_ptr<storm::logic::Formula const> range.
     */
    template<typename ForwardIt>
    void insertNamesFromProperties(ForwardIt first, ForwardIt last) {
        if (this->allRelevant) {
            return;
        }

        // Get necessary labels from properties
        std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> atomicLabels{};
        std::for_each(first, last, [&atomicLabels](auto const& property) { property->gatherAtomicLabelFormulas(atomicLabels); });

        // Add relevant event names from properties
        for (auto const& atomic : atomicLabels) {
            std::string label = atomic->getLabel();
            if (label == "failed" or label == "skipped") {
                // Ignore as these label will always be added if necessary
            } else {
                // Get name of event
                if (boost::ends_with(label, "_failed")) {
                    // length of "_failed" = 7
                    this->names.insert(label.substr(0, label.size() - 7));
                } else if (boost::ends_with(label, "_dc")) {
                    // length of "_dc" = 3
                    this->names.insert(label.substr(0, label.size() - 3));
                } else if (label.find("_claimed_") != std::string::npos) {
                    STORM_LOG_THROW(storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>().isAddLabelsClaiming(),
                                    storm::exceptions::InvalidArgumentException,
                                    "Claiming labels will not be exported but are required for label '" << label << "'. Try setting --labels-claiming.");
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Label '" << label << "' not known.");
                }
            }
        }
    }

    /*!
     * Add relevant event.
     * If name 'all' occurs, all elements are stored as relevant.
     *
     * @param name Name of relevant event.
     */
    void insert(std::string const& name) {
        if (name == "all") {
            setAllRelevant();
        }
        names.insert(name);
    }

    /*!
     * Add relevant event names from a range.
     * If name 'all' occurs, all elements are stored as relevant.
     *
     * @param first Iterator pointing to the start of a range of names.
     * @param last Iterator pointing to the end of a range of names.
     */
    template<typename ForwardIt>
    void insert(ForwardIt first, ForwardIt last) {
        // check if the name "all" occurs
        if (std::any_of(first, last, [](auto const& name) { return name == "all"; })) {
            setAllRelevant();
        } else {
            this->names.insert(first, last);
        }
    }

    /*!
     * Check that the relevant names correspond to existing elements in the DFT.
     *
     * @param dft DFT.
     * @return True iff the relevant names are consistent with the given DFT.
     */
    template<typename ValueType>
    bool checkRelevantNames(storm::dft::storage::DFT<ValueType> const& dft) const {
        for (std::string const& relevantName : this->names) {
            if (!dft.existsName(relevantName)) {
                return false;
            }
        }
        return true;
    }

    /*!
     * @return True iff the given name is the name of a relevant Event
     */
    bool isRelevant(std::string const& name) const {
        if (this->allRelevant) {
            return true;
        } else {
            return this->names.find(name) != this->names.end();
        }
    }

    /*!
     * Merge the given RelevantEvents with *this
     *
     * @return A reference to *this, allowing chaining i.e. e.merge(a).merge(b)
     */
    RelevantEvents& merge(RelevantEvents const& other) {
        if (!this->allRelevant) {
            if (other.allRelevant) {
                setAllRelevant();
            } else {
                this->names.insert(other.names.begin(), other.names.end());
            }
        }
        return *this;
    }

   private:
    void setAllRelevant() {
        this->allRelevant = true;
        this->names.clear();
    }

    // Names of relevant events.
    std::set<std::string> names{};

    // Whether all elements are relevant.
    bool allRelevant{false};
};

}  // namespace utility
}  // namespace storm::dft

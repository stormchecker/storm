#pragma once

#include <memory>
#include <vector>

#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/SylvanBddManager.h"
#include "storm-dft/utility/RelevantEvents.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/bitoperations.h"

namespace storm::dft {
namespace transformations {

/**
 * Transformator for DFT -> BDD.
 */
template<typename ValueType>
class SftToBddTransformator {
   public:
#ifdef STORM_HAVE_SYLVAN
    using Bdd = sylvan::Bdd;
#endif

#ifdef STORM_HAVE_SYLVAN
    SftToBddTransformator(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft, std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager,
                          storm::dft::utility::RelevantEvents relevantEvents = {})
        : dft{std::move(dft)}, sylvanBddManager{std::move(sylvanBddManager)}, relevantEvents{relevantEvents} {
        // create Variables for the BEs
        for (auto const& i : this->dft->getBasicElements()) {
            // Filter constantBeTrigger
            if (i->name() != "constantBeTrigger") {
                variables.push_back(this->sylvanBddManager->createVariable(i->name()));
            }
        }
    }

    /**
     * Transform a sft into a BDD
     * representing the function of the toplevel event.
     *
     * \exception storm::exceptions::NotSupportedException
     * The given DFT is not a SFT
     *
     * \return The translated Bdd of the toplevel event
     */
    Bdd const& transformTopLevel() {
        auto const tlName{dft->getTopLevelElement()->name()};
        if (relevantEventBdds.empty()) {
            relevantEventBdds[tlName] = translate(dft->getTopLevelElement());
        }
        // else relevantEventBdds is not empty and we maintain the invariant
        // that the toplevel event is in there
        STORM_LOG_ASSERT(relevantEventBdds.count(tlName) == 1, "Not all relevantEvents where transformed into BDDs.");

        return relevantEventBdds[tlName];
    }

    /**
     * Transform a sft into BDDs
     * representing the functions of the given Events
     * if they are reachable from the toplevel Event.
     *
     * \exception storm::exceptions::NotSupportedException
     * The given DFT is not a SFT
     *
     * \return A mapping from element name to translated Bdd
     */
    std::map<std::string, Bdd> const& transformRelevantEvents() {
        if (relevantEventBdds.empty()) {
            relevantEventBdds[dft->getTopLevelElement()->name()] = translate(dft->getTopLevelElement());
        }

        // we maintain the invariant that if relevantEventBdds is not empty
        // then we have calculated all relevant bdds
        STORM_LOG_ASSERT(relevantEventBdds.size() == relevantEvents.count(getDFT()), "Not all relevantEvents where transformed into BDDs.");

        return relevantEventBdds;
    }

    /**
     * Get Variables in the order they where inserted
     */
    std::vector<uint32_t> const& getDdVariables() const noexcept {
        return variables;
    }

    /**
     * \return The internal DFT
     */
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> getDFT() const noexcept {
        return dft;
    }

    /**
     * \return The internal sylvanBddManager
     */
    std::shared_ptr<storm::dft::storage::SylvanBddManager> getSylvanBddManager() const noexcept {
        return sylvanBddManager;
    }
#else
    SftToBddTransformator(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft, std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager,
                          storm::dft::utility::RelevantEvents relevantEvents = {}) {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }

    std::vector<uint32_t> const& getDdVariables() const noexcept {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }

    std::shared_ptr<storm::dft::storage::DFT<ValueType>> getDFT() const noexcept {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }

    std::shared_ptr<storm::dft::storage::SylvanBddManager> getSylvanBddManager() const noexcept {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }
#endif

   private:
#ifdef STORM_HAVE_SYLVAN
    std::map<std::string, Bdd> relevantEventBdds{};
    std::vector<uint32_t> variables{};
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft;
    std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager;
    storm::dft::utility::RelevantEvents relevantEvents;

    /**
     * Translate a simple DFT element into a BDD.
     *
     * \exception storm::exceptions::NotSupportedException
     * The given DFT is not a SFT
     */
    Bdd translate(std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element) {
        auto isRelevant{relevantEvents.isRelevant(element->name())};
        if (isRelevant) {
            auto const it{relevantEventBdds.find(element->name())};
            if (it != relevantEventBdds.end()) {
                return it->second;
            }
        }

        Bdd rBdd;
        if (element->isGate()) {
            rBdd = translate(std::dynamic_pointer_cast<storm::dft::storage::elements::DFTGate<ValueType> const>(element));
        } else if (element->isBasicElement()) {
            rBdd = translate(std::dynamic_pointer_cast<storm::dft::storage::elements::DFTBE<ValueType> const>(element));
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Element of type \"" << element->typestring() << "\" is not supported. Probably not a SFT.");
            rBdd = sylvanBddManager->getZero();
        }

        if (isRelevant) {
            // rBdd can't be in relevantEventBdds
            // as we would've returned
            // at the start of the function
            relevantEventBdds[element->name()] = rBdd;
        }

        return rBdd;
    }

    /**
     * Translate a simple DFT gate into a BDD.
     *
     * \exception storm::exceptions::NotSupportedException
     * The given DFT is not a SFT
     */
    Bdd translate(std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType> const> gate) {
        if (gate->type() == storm::dft::storage::elements::DFTElementType::AND) {
            // used only in conjunctions therefore neutral element -> 1
            auto tmpBdd{sylvanBddManager->getOne()};
            for (auto const& child : gate->children()) {
                tmpBdd &= translate(child);
            }
            return tmpBdd;
        } else if (gate->type() == storm::dft::storage::elements::DFTElementType::OR) {
            // used only in disjunctions therefore neutral element -> 0
            auto tmpBdd{sylvanBddManager->getZero()};
            for (auto const& child : gate->children()) {
                tmpBdd |= translate(child);
            }
            return tmpBdd;
        } else if (gate->type() == storm::dft::storage::elements::DFTElementType::VOT) {
            return translate(std::dynamic_pointer_cast<storm::dft::storage::elements::DFTVot<ValueType> const>(gate));
        }
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Gate of type \"" << gate->typestring() << "\" is not supported. Probably not a SFT.");
        return sylvanBddManager->getZero();
    }

    /**
     * Translate a DFT VOT gate into a BDD.
     *
     * \exception storm::exceptions::NotSupportedException
     * The given DFT is not a SFT
     */
    Bdd translate(std::shared_ptr<storm::dft::storage::elements::DFTVot<ValueType> const> vot) {
        std::vector<Bdd> bdds;
        bdds.reserve(vot->children().size());

        for (auto const& child : vot->children()) {
            bdds.push_back(translate(child));
        }

        auto const rval{translateVot(0, vot->threshold(), bdds)};
        return rval;
    }

    /**
     * Helper function to translate a Vot gate into a BDD
     *
     * Performs a shannon decomposition
     *
     * \param currentIndex
     * The index of the child we currently look at.
     *
     * \param threshold
     * How many children still need to be chosen.
     *
     * \param bdds
     * A reference to all children of the vot gate as Bdds.
     */
    Bdd translateVot(size_t const currentIndex, size_t const threshold, std::vector<Bdd> const& bdds) const {
        if (threshold == 0) {
            return sylvanBddManager->getOne();
        } else if (currentIndex >= bdds.size()) {
            return sylvanBddManager->getZero();
        }

        auto const notChosenBdd{translateVot(currentIndex + 1, threshold, bdds)};
        auto const chosenBdd{translateVot(currentIndex + 1, threshold - 1, bdds)};

        return bdds[currentIndex].Ite(chosenBdd, notChosenBdd);
    }

    /**
     * Translate a DFT basic element gate into a BDD.
     *
     * \note This is the recursion anchor.
     *
     */
    Bdd translate(std::shared_ptr<storm::dft::storage::elements::DFTBE<ValueType> const> const basicElement) {
        return sylvanBddManager->getPositiveLiteral(basicElement->name());
    }
#endif
};

}  // namespace transformations
}  // namespace storm::dft

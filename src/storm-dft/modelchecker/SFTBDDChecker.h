#pragma once

#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "storm-dft/storage/DFT.h"
#include "storm-dft/storage/SylvanBddManager.h"
#include "storm-dft/transformations/SftToBddTransformator.h"
#include "storm/environment/Environment.h"

namespace storm::dft {
namespace modelchecker {

/**
 * Main class for the SFTBDDChecker
 *
 */
class SFTBDDChecker {
   public:
    using ValueType = double;
#ifdef STORM_HAVE_SYLVAN
    using Bdd = sylvan::Bdd;
#endif

    SFTBDDChecker(std::shared_ptr<storm::dft::storage::DFT<ValueType>> dft, std::shared_ptr<storm::dft::storage::SylvanBddManager> sylvanBddManager);

    SFTBDDChecker(std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> transformator);

    /**
     * \return The internal DFT
     */
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> getDFT() const;

    /**
     * \return The internal sylvanBddManager
     */
    std::shared_ptr<storm::dft::storage::SylvanBddManager> getSylvanBddManager() const;

    /**
     * \return The internal SftToBddTransformator
     */
    std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> getTransformator() const;

    /**
     * Exports the Bdd that represents the top level event to a file
     * in the dot format.
     *
     * \param filename
     * The name of the file the dot graph is written to
     */
    void exportBddToDot(std::string const &filename) {
#ifdef STORM_HAVE_SYLVAN
        getSylvanBddManager()->exportBddToDot(getTopLevelElementBdd(), filename);
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

    /**
     * \return
     * A set of minimal cut sets,
     * where the basic events are identified by their name
     */
    std::vector<std::vector<std::string>> getMinimalCutSets();

    /**
     * \return
     * A set of minimal cut sets,
     * where the basic events are identified by their index
     * in the bdd manager
     */
    std::vector<std::vector<uint32_t>> getMinimalCutSetsAsIndices();

    /**
     * \return
     * The Probability that the top level event fails.
     */
    ValueType getProbabilityAtTimebound(ValueType timebound) {
#ifdef STORM_HAVE_SYLVAN
        return getProbabilityAtTimebound(getTopLevelElementBdd(), timebound);
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

#ifdef STORM_HAVE_SYLVAN
    /**
     * \return
     * The Probabilities that the given Event fails at the given timebound.
     *
     * \param bdd
     * The bdd that represents an event in the dft.
     * Must be from a call to some function of *this.
     */
    ValueType getProbabilityAtTimebound(Bdd bdd, ValueType timebound) const;
#endif

    /**
     * \return
     * The Probabilities that the top level event fails at the given timepoints.
     *
     * \param timepoints
     * Array of timebounds to calculate the failure probabilities for.
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<ValueType> getProbabilitiesAtTimepoints(std::vector<ValueType> const &timepoints, size_t const chunksize = 0) {
#ifdef STORM_HAVE_SYLVAN
        return getProbabilitiesAtTimepoints(getTopLevelElementBdd(), timepoints, chunksize);
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
#endif
    }

#ifdef STORM_HAVE_SYLVAN
    /**
     * \return
     * The Probabilities that the given Event fails at the given timepoints.
     *
     * \param bdd
     * The bdd that represents an event in the dft.
     * Must be from a call to some function of *this.
     *
     * \param timepoints
     * Array of timebounds to calculate the failure probabilities for.
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<ValueType> getProbabilitiesAtTimepoints(Bdd bdd, std::vector<ValueType> const &timepoints, size_t chunksize = 0) const;
#endif

    /**
     * \return
     * The birnbaum importance factor of the given basic event
     * at the given timebound
     */
    ValueType getBirnbaumFactorAtTimebound(std::string const &beName, ValueType timebound);

    /**
     * \return
     * The birnbaum importance factor of all basic events
     * at the given timebound
     *
     * \note
     * Sorted after the order of dft->getBasicElements.
     * Faster than looping over getBirnbaumFactorAtTimebound.
     */
    std::vector<ValueType> getAllBirnbaumFactorsAtTimebound(ValueType timebound);

    /**
     * \return
     * The birnbaum importance factors of the given basic event
     *
     * \param timepoints
     * Array of timebounds to calculate the factors for.
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<ValueType> getBirnbaumFactorsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The birnbaum importance factors of all basic events
     *
     * \param timepoints
     * Array of timebounds to calculate the factors for.
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<std::vector<ValueType>> getAllBirnbaumFactorsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The Critical importance factor of the given basic event
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     */
    ValueType getCIFAtTimebound(std::string const &beName, ValueType timebound);

    /**
     * \return
     * The Critical importance factor of all basic events
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \note
     * Sorted after the order of dft->getBasicElements.
     * Faster than looping over getCIFAtTimebound.
     */
    std::vector<ValueType> getAllCIFsAtTimebound(ValueType timebound);

    /**
     * \return
     * The Critical importance factor of the given basic event
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<ValueType> getCIFsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The Critical importance factor of all basic events
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<std::vector<ValueType>> getAllCIFsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The Diagnostic importance factor of the given basic event
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     */
    ValueType getDIFAtTimebound(std::string const &beName, ValueType timebound);

    /**
     * \return
     * The Diagnostic importance factor of all basic events
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \note
     * Sorted after the order of dft->getBasicElements.
     * Faster than looping over getDIFAtTimebound.
     */
    std::vector<ValueType> getAllDIFsAtTimebound(ValueType timebound);

    /**
     * \return
     * The Diagnostic importance factor of the given basic event
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<ValueType> getDIFsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The Diagnostic importance factor of all basic events
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<std::vector<ValueType>> getAllDIFsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The risk achievement worth of the given basic event
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     */
    ValueType getRAWAtTimebound(std::string const &beName, ValueType timebound);

    /**
     * \return
     * The risk achievement worth of all basic events
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \note
     * Sorted after the order of dft->getBasicElements.
     * Faster than looping over getRAWAtTimebound.
     */
    std::vector<ValueType> getAllRAWsAtTimebound(ValueType timebound);

    /**
     * \return
     * The risk achievement worth of the given basic event
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<ValueType> getRAWsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The risk achievement worth of all basic events
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<std::vector<ValueType>> getAllRAWsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The risk reduction worth of the given basic event
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     */
    ValueType getRRWAtTimebound(std::string const &beName, ValueType timebound);

    /**
     * \return
     * The risk reduction worth of all basic events
     * at the given timebound as defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \note
     * Sorted after the order of dft->getBasicElements.
     * Faster than looping over getRRWAtTimebound.
     */
    std::vector<ValueType> getAllRRWsAtTimebound(ValueType timebound);

    /**
     * \return
     * The risk reduction worth of the given basic event
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<ValueType> getRRWsAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize = 0);

    /**
     * \return
     * The risk reduction worth of all basic events
     * defined in
     * 10.1016/S0951-8320(01)00004-7
     *
     * \param chunksize
     * Splits the timepoints array into chunksize chunks.
     * A value of 0 represents to calculate the whole array at once.
     */
    std::vector<std::vector<ValueType>> getAllRRWsAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize = 0);

   private:
#ifdef STORM_HAVE_SYLVAN
    /**
     * Recursively traverses the given BDD and returns the minimalCutSets.
     *
     * \param bdd
     * The current bdd
     *
     * \param buffer
     * Reference to a vector that is used as a stack.
     * Temporarily stores the positive variables encountered.
     *
     * \param minimalCutSets
     * Reference to a set of minimal cut sets.
     * Will be populated by the function.
     */
    void recursiveMCS(Bdd const bdd, std::vector<uint32_t> &buffer, std::vector<std::vector<uint32_t>> &minimalCutSets) const;

    template<typename FuncType>
    void chunkCalculationTemplate(std::vector<ValueType> const &timepoints, size_t chunksize, FuncType func) const;

    template<typename FuncType>
    ValueType getImportanceMeasureAtTimebound(std::string const &beName, ValueType timebound, FuncType func);

    template<typename FuncType>
    std::vector<ValueType> getAllImportanceMeasuresAtTimebound(ValueType timebound, FuncType func);

    template<typename FuncType>
    std::vector<ValueType> getImportanceMeasuresAtTimepoints(std::string const &beName, std::vector<ValueType> const &timepoints, size_t chunksize,
                                                             FuncType func);

    template<typename FuncType>
    std::vector<std::vector<ValueType>> getAllImportanceMeasuresAtTimepoints(std::vector<ValueType> const &timepoints, size_t chunksize, FuncType func);

    /**
     * \return
     * Generated Bdd that represents the formula of the top level event
     */
    Bdd getTopLevelElementBdd();

    std::shared_ptr<storm::dft::transformations::SftToBddTransformator<ValueType>> transformator;
#endif
};

}  // namespace modelchecker
}  // namespace storm::dft

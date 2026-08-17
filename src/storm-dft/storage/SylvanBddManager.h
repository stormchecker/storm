#pragma once

#include <map>

#include "storm/adapters/sylvan.h"
#include "storm/environment/Environment.h"
#include "storm/environment/dd/DdEnvironment.h"
#include "storm/io/file.h"
#include "storm/storage/dd/sylvan/InternalSylvanDdManager.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace storage {

/**
 * Simple Manager for Sylvan
 *
 * \note
 * Not thread safe.
 *
 * Initializes Sylvan
 * which can only be initialized once per programm invocation
 */
class SylvanBddManager {
   public:
    /**
     * Initializes Sylvan
     *
     * \note
     * Internally Sylvan is initialized by a InternalSylvanDdManager.
     * This ensures compatibility.
     *
     * \param env
     * The environment providing the settings for the Sylvan manager.
     */
#ifdef STORM_HAVE_SYLVAN
    explicit SylvanBddManager(storm::Environment const &env) : internalManager{env.dd().get<storm::dd::DdType::Sylvan>()} {}
#else
    explicit SylvanBddManager(storm::Environment const &) {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }
#endif

    /*!
     * Creates a new manager that is configured according to a default environment.
     */
    static std::shared_ptr<SylvanBddManager> createWithDefaultEnvironment() {
        return std::make_shared<SylvanBddManager>(storm::Environment());
    }

    // We can only initialize Sylvan once therefore no copy semantics
    SylvanBddManager(SylvanBddManager const &) = delete;

    SylvanBddManager(SylvanBddManager &&) = default;
    SylvanBddManager &operator=(SylvanBddManager &&) = default;

    /**
     * Destroys Sylvan
     */
    ~SylvanBddManager() = default;

#ifdef STORM_HAVE_SYLVAN
    /*!
     * All code that manipulates DDs shall be called through this function.
     * This is generally needed to set-up the correct context.
     * Specifically for sylvan, this is required to make sure that DD-manipulating code is executed as a LACE task.
     * Example usage: `manager->execute([&]() { bar = foo(arg1,arg2); }`
     *
     * @param f the function that is executed
     */
    void execute(std::function<void()> const &f) const {
        internalManager.execute(f);
    }

    /**
     * Creates a variable with a unique name
     *
     * \return Index of the variable
     */
    uint32_t createVariable(std::string const name) {
        nameToIndex[name] = nextFreeVariableIndex;
        indexToName[nextFreeVariableIndex] = name;
        sylvan::Bdd::bddVar(nextFreeVariableIndex);
        return nextFreeVariableIndex++;
    }

    /**
     * \return
     * The bdd representation of the variable.
     * Therefore ITE(x, 1, 0).
     */
    sylvan::Bdd getPositiveLiteral(std::string const name) const {
        return sylvan::Bdd::bddVar(nameToIndex.at(name));
    }

    /**
     * \note equivalent with !getPositiveLiteral(name)
     *
     * \return
     * The negative bdd representation of the variable.
     * Therefore ITE(x, 0, 1).
     */
    sylvan::Bdd getNegativeLiteral(std::string const name) const {
        return !sylvan::Bdd::bddVar(nameToIndex.at(name));
    }

    /**
     * \return
     * The bdd representation of the variable.
     * Therefore ITE(x, 1, 0).
     */
    sylvan::Bdd getPositiveLiteral(uint32_t const index) const {
        return sylvan::Bdd::bddVar(index);
    }

    /**
     * \note equivalent with !getPositiveLiteral(index)
     *
     * \return
     * The negative bdd representation of the variable.
     * Therefore ITE(x, 0, 1).
     */
    sylvan::Bdd getNegativeLiteral(uint32_t const index) const {
        return !sylvan::Bdd::bddVar(index);
    }

    /**
     * \return
     * The bdd representation of the constant function 1.
     */
    sylvan::Bdd getOne() {
        return sylvan::Bdd::bddOne();
    }

    /**
     * \return
     * The bdd representation of the constant function 0.
     */
    sylvan::Bdd getZero() {
        return sylvan::Bdd::bddZero();
    }

    /**
     * \return
     * The index of the variable with the given name.
     */
    uint32_t getIndex(std::string const name) const {
        return nameToIndex.at(name);
    }

    /**
     * \return
     * The name of the variable with the given index.
     */
    std::string getName(uint32_t const index) const {
        return indexToName.at(index);
    }

    /**
     * Exports the given Bdd to a file
     * in the dot format.
     *
     * \param bdd
     * The Bdd.
     * \param filename
     * The name of the file the dot graph is written to
     */
    void exportBddToDot(sylvan::Bdd const &bdd, std::string const &filename) const {
        FILE *filePointer = fopen(filename.c_str(), "w+");

        // fopen returns a nullptr on failure
        if (filePointer == nullptr) {
            STORM_LOG_ERROR("Failure to open file: " << filename);
        } else {
            bdd.PrintDot(filePointer);
            fclose(filePointer);
            std::ofstream filestream;
            storm::io::openFile(filename.c_str(), filestream, true);
            filestream << "// Mapping from BDD nodes to DFT BEs as follows: \n";
            for (auto const &[index, name] : indexToName) {
                filestream << "// " << index << " -> " << name << '\n';
            }
            storm::io::closeFile(filestream);
        }
    }
#else
    void execute(std::function<void()> const &f) const {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }

    uint32_t createVariable(std::string const name) {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }

    uint32_t getIndex(std::string const name) const {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }

    std::string getName(uint32_t const index) const {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                        "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                        "version of Storm with Sylvan support.");
    }
#endif

   private:
#ifdef STORM_HAVE_SYLVAN
    storm::dd::InternalDdManager<storm::dd::DdType::Sylvan> internalManager;
    uint32_t nextFreeVariableIndex{0};

    std::map<std::string, uint32_t> nameToIndex{};
    std::map<uint32_t, std::string> indexToName{};
#endif
};

}  // namespace storage
}  // namespace storm::dft

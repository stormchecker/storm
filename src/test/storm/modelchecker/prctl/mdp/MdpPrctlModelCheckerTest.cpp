#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/MultiplierEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/HybridMdpPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SymbolicMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/storage/jani/Property.h"

namespace {

enum class MdpEngine { PrismSparse, JaniSparse, Hybrid, PrismDd, JaniDd };

class SparseDoubleValueIterationViOpGaussSeidelMultEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        env.solver().minMax().setMultiplicationStyle(storm::solver::MultiplicationStyle::GaussSeidel);
        env.solver().multiplier().setType(storm::solver::MultiplierType::ViOperator);
        return env;
    }
};

class SparseDoubleValueIterationViOpRegularMultEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        env.solver().minMax().setMultiplicationStyle(storm::solver::MultiplicationStyle::Regular);
        env.solver().multiplier().setType(storm::solver::MultiplierType::ViOperator);
        return env;
    }
};

class SparseDoubleValueIterationNativeGaussSeidelMultEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        env.solver().minMax().setMultiplicationStyle(storm::solver::MultiplicationStyle::GaussSeidel);
        env.solver().multiplier().setType(storm::solver::MultiplierType::Native);
        return env;
    }
};

class SparseDoubleValueIterationNativeRegularMultEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        env.solver().minMax().setMultiplicationStyle(storm::solver::MultiplicationStyle::Regular);
        env.solver().multiplier().setType(storm::solver::MultiplierType::Native);
        return env;
    }
};

class JaniSparseDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::JaniSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};

class SparseDoubleIntervalIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::IntervalIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};

class SparseDoubleSoundValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};

class SparseDoubleOptimisticValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::OptimisticValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};

class SparseDoubleGuessingValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::GuessingValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};

class SparseDoubleTopologicalValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::Topological);
        env.solver().topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};

class SparseDoubleTopologicalSoundValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::Topological);
        env.solver().topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};

class SparseDoubleTopologicalGuessingValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::Topological);
        env.solver().topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::GuessingValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};

class SparseDoubleLPEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::LinearProgramming);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class SparseDoubleViToLPEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ViToLp);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class SparseRationalPolicyIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        return env;
    }
};

class SparseRationalViToPiEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ViToPi);
        return env;
    }
};

class SparseRationalRationalSearchEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // Unused for sparse models
    static const MdpEngine engine = MdpEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Mdp<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::RationalSearch);
        return env;
    }
};
class HybridCuddDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const MdpEngine engine = MdpEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class HybridSylvanDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const MdpEngine engine = MdpEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class HybridCuddDoubleSoundValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const MdpEngine engine = MdpEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::SoundValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};
class HybridCuddDoubleOptimisticValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const MdpEngine engine = MdpEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::OptimisticValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};
class HybridCuddDoubleGuessingValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const MdpEngine engine = MdpEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::GuessingValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().minMax().setRelativeTerminationCriterion(false);
        return env;
    }
};
class HybridSylvanRationalPolicyIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const MdpEngine engine = MdpEngine::Hybrid;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        return env;
    }
};
class DdCuddDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const MdpEngine engine = MdpEngine::PrismDd;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class JaniDdCuddDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const MdpEngine engine = MdpEngine::JaniDd;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class DdSylvanDoubleValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const MdpEngine engine = MdpEngine::PrismDd;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class DdCuddDoublePolicyIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const MdpEngine engine = MdpEngine::PrismDd;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-10));
        return env;
    }
};
class DdSylvanRationalRationalSearchEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const MdpEngine engine = MdpEngine::PrismDd;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::symbolic::Mdp<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::RationalSearch);
        return env;
    }
};

template<typename TestType>
class MdpPrctlModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::Mdp<ValueType> SparseModelType;
    typedef typename storm::models::symbolic::Mdp<TestType::ddType, ValueType> SymbolicModelType;

    MdpPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        if constexpr (TestType::engine == MdpEngine::Hybrid || TestType::engine == MdpEngine::PrismDd || TestType::engine == MdpEngine::JaniDd) {
            TestType::checkLibraryAvailable();
        }
    }

    storm::Environment const& env() const {
        return _environment;
    }
    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }
    ValueType precision() const {
        return TestType::isExact ? parseNumber("0") : parseNumber("1e-6");
    }
    bool isSparseModel() const {
        return std::is_same<typename TestType::ModelType, SparseModelType>::value;
    }
    bool isSymbolicModel() const {
        return std::is_same<typename TestType::ModelType, SymbolicModelType>::value;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value,
                            std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>>>::type
    buildModelFormulas(std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
        program = storm::utility::prism::preprocess(program, constantDefinitionString);
        if (TestType::engine == MdpEngine::PrismSparse) {
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<MT>();
        } else if (TestType::engine == MdpEngine::JaniSparse) {
            auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            janiData.first.substituteFunctions();
            result.second = storm::api::extractFormulasFromProperties(janiData.second);
            result.first = storm::api::buildSparseModel<ValueType>(janiData.first, result.second)->template as<MT>();
        }
        return result;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value,
                            std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>>>::type
    buildModelFormulas(std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<MT>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
        program = storm::utility::prism::preprocess(program, constantDefinitionString);
        if (TestType::engine == MdpEngine::Hybrid || TestType::engine == MdpEngine::PrismDd) {
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(program, result.second)->template as<MT>();
        } else if (TestType::engine == MdpEngine::JaniDd) {
            auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            janiData.first.substituteFunctions();
            result.second = storm::api::extractFormulasFromProperties(janiData.second);
            result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(janiData.first, result.second)->template as<MT>();
        }
        return result;
    }

    std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> getTasks(
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) const {
        std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> result;
        for (auto const& f : formulas) {
            result.emplace_back(*f);
        }
        return result;
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type createModelChecker(
        std::shared_ptr<MT> const& model) const {
        if (TestType::engine == MdpEngine::PrismSparse || TestType::engine == MdpEngine::JaniSparse) {
            return std::make_shared<storm::modelchecker::SparseMdpPrctlModelChecker<SparseModelType>>(*model);
        }
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type
    createModelChecker(std::shared_ptr<MT> const& model) const {
        if (TestType::engine == MdpEngine::Hybrid) {
            return std::make_shared<storm::modelchecker::HybridMdpPrctlModelChecker<SymbolicModelType>>(*model);
        } else if (TestType::engine == MdpEngine::PrismDd || TestType::engine == MdpEngine::JaniDd) {
            return std::make_shared<storm::modelchecker::SymbolicMdpPrctlModelChecker<SymbolicModelType>>(*model);
        }
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SparseModelType>::value, void>::type execute(std::shared_ptr<MT> const& model,
                                                                                          std::function<void()> const& f) const {
        f();
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, void>::type execute(std::shared_ptr<MT> const& model,
                                                                                            std::function<void()> const& f) const {
        model->getManager().execute(f);
    }

    bool getQualitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                            std::unique_ptr<storm::modelchecker::CheckResult>& result) {
        auto filter = getInitialStateFilter(model);
        result->filter(*filter);
        return result->asQualitativeCheckResult().forallTrue();
    }

    ValueType getQuantitativeResultAtInitialState(std::shared_ptr<storm::models::Model<ValueType>> const& model,
                                                  std::unique_ptr<storm::modelchecker::CheckResult>& result) {
        auto filter = getInitialStateFilter(model);
        result->filter(*filter);
        return result->asQuantitativeCheckResult<ValueType>().getMin();
    }

   private:
    storm::Environment _environment;

    std::unique_ptr<storm::modelchecker::QualitativeCheckResult> getInitialStateFilter(std::shared_ptr<storm::models::Model<ValueType>> const& model) const {
        if (isSparseModel()) {
            return std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<ValueType>>(model->template as<SparseModelType>()->getInitialStates());
        } else {
            return std::make_unique<storm::modelchecker::SymbolicQualitativeCheckResult<TestType::ddType>>(
                model->template as<SymbolicModelType>()->getReachableStates(), model->template as<SymbolicModelType>()->getInitialStates());
        }
    }
};

typedef ::testing::Types<SparseDoubleValueIterationViOpGaussSeidelMultEnvironment, SparseDoubleValueIterationViOpRegularMultEnvironment,
                         SparseDoubleValueIterationNativeGaussSeidelMultEnvironment, SparseDoubleValueIterationNativeRegularMultEnvironment,
                         JaniSparseDoubleValueIterationEnvironment, SparseDoubleIntervalIterationEnvironment, SparseDoubleSoundValueIterationEnvironment,
                         SparseDoubleOptimisticValueIterationEnvironment, SparseDoubleGuessingValueIterationEnvironment,
                         SparseDoubleTopologicalValueIterationEnvironment, SparseDoubleTopologicalSoundValueIterationEnvironment,
                         SparseDoubleTopologicalGuessingValueIterationEnvironment, SparseDoubleLPEnvironment, SparseDoubleViToLPEnvironment,
                         SparseRationalPolicyIterationEnvironment, SparseRationalViToPiEnvironment, SparseRationalRationalSearchEnvironment,
                         HybridCuddDoubleValueIterationEnvironment, HybridSylvanDoubleValueIterationEnvironment, HybridCuddDoubleSoundValueIterationEnvironment,
                         HybridCuddDoubleOptimisticValueIterationEnvironment, HybridCuddDoubleGuessingValueIterationEnvironment,
                         HybridSylvanRationalPolicyIterationEnvironment, DdCuddDoubleValueIterationEnvironment, JaniDdCuddDoubleValueIterationEnvironment,
                         DdSylvanDoubleValueIterationEnvironment, DdCuddDoublePolicyIterationEnvironment, DdSylvanRationalRationalSearchEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(MdpPrctlModelCheckerTest, TestingTypes, );

TYPED_TEST(MdpPrctlModelCheckerTest, Dice) {
    std::string formulasString = "Pmin=? [F \"two\"]";
    formulasString += "; Pmax=? [F \"two\"]";
    formulasString += "; Pmin=? [F \"three\"]";
    formulasString += "; Pmax=? [F \"three\"]";
    formulasString += "; Pmin=? [F \"four\"]";
    formulasString += "; Pmax=? [F \"four\"]";
    formulasString += "; Rmin=? [F \"done\"]";
    formulasString += "; Rmax=? [F \"done\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(169ul, model->getNumberOfStates());
        EXPECT_EQ(436ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);

        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("1/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("2/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("2/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[4]);
        EXPECT_NEAR(this->parseNumber("3/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[5]);
        EXPECT_NEAR(this->parseNumber("3/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[6]);
        EXPECT_NEAR(this->parseNumber("22/3"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[7]);
        EXPECT_NEAR(this->parseNumber("22/3"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(MdpPrctlModelCheckerTest, AsynchronousLeader) {
    std::string formulasString = "Pmin=? [F \"elected\"]";
    formulasString += "; Pmax=? [F \"elected\"]";
    formulasString += "; Pmin=? [F<=25 \"elected\"]";
    formulasString += "; Pmax=? [F<=25 \"elected\"]";
    formulasString += "; Rmin=? [F \"elected\"]";
    formulasString += "; Rmax=? [F \"elected\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/leader4.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(3172ul, model->getNumberOfStates());
        EXPECT_EQ(7144ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("1/16"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("1/16"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[4]);
        EXPECT_NEAR(this->parseNumber("30/7"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[5]);
        EXPECT_NEAR(this->parseNumber("30/7"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(MdpPrctlModelCheckerTest, consensus) {
    std::string formulasString = "Pmax=? [F \"finished\"]";
    formulasString += "; Pmax=? [F \"all_coins_equal_1\"]";
    formulasString += "; P<0.8 [F \"all_coins_equal_1\"]";
    formulasString += "; P<0.9 [F \"all_coins_equal_1\"]";
    formulasString += "; Rmax=? [F \"all_coins_equal_1\"]";
    formulasString += "; Rmin=? [F \"all_coins_equal_1\"]";
    formulasString += "; Rmax=? [F \"finished\"]";
    formulasString += "; Rmin=? [F \"finished\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(272ul, model->getNumberOfStates());
        EXPECT_EQ(492ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("57/64"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_FALSE(this->getQualitativeResultAtInitialState(model, result));

        result = checker->check(this->env(), tasks[3]);
        EXPECT_TRUE(this->getQualitativeResultAtInitialState(model, result));

        result = checker->check(this->env(), tasks[4]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));

        result = checker->check(this->env(), tasks[5]);
        EXPECT_TRUE(storm::utility::isInfinity(this->getQuantitativeResultAtInitialState(model, result)));

        result = checker->check(this->env(), tasks[6]);
        EXPECT_NEAR(this->parseNumber("75"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[7]);
        EXPECT_NEAR(this->parseNumber("48"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(MdpPrctlModelCheckerTest, TinyRewards) {
    std::string formulasString = "Rmin=? [F \"target\"]";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(3ul, model->getNumberOfStates());
        EXPECT_EQ(4ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // This example considers a zero-reward end component that does not reach the target
        // For some methods this requires end-component elimination which is (currently) not supported in the Dd engine

        if (TypeParam::engine == MdpEngine::PrismDd && this->env().solver().minMax().getMethod() == storm::solver::MinMaxMethod::RationalSearch) {
            STORM_SILENT_EXPECT_THROW(checker->check(this->env(), tasks[0]), storm::exceptions::UncheckedRequirementException);
        } else {
            result = checker->check(this->env(), tasks[0]);
            EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        }
    });
}

TYPED_TEST(MdpPrctlModelCheckerTest, Team) {
    std::string formulasString = "R{\"w_1_total\"}max=? [ C ]";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/multiobj_team3.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(12475ul, model->getNumberOfStates());
        EXPECT_EQ(15228ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // This example considers an expected total reward formula, which is not supported in all engines

        if (TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse) {
            result = checker->check(this->env(), tasks[0]);
            EXPECT_NEAR(this->parseNumber("114/49"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
}

TYPED_TEST(MdpPrctlModelCheckerTest, LtlProbabilitiesCoin) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "Pmin=? [!(GF \"all_coins_equal_1\")]";
    formulasString += "; Pmax=? [F \"all_coins_equal_1\" U \"finished\"]";
    formulasString += "; P>0.4 [!(GF \"all_coins_equal_1\")]";
    formulasString += "; P<0.6 [F \"all_coins_equal_1\" U \"finished\"]";
    // The following example results in an automaton with acceptance condition not in DNF (Streett, using Spot)
    formulasString += "; Pmax=?[ (GF \"all_coins_equal_1\") & ((GF \"all_coins_equal_0\") | (FG \"finished\"))]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(272ul, model->getNumberOfStates());
        EXPECT_EQ(492ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // LTL not supported in all engines (Hybrid,  PrismDd, JaniDd)
        if (TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse) {
            result = checker->check(this->env(), tasks[0]);
            EXPECT_NEAR(this->parseNumber("4/9"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[1]);
            EXPECT_NEAR(this->parseNumber("5/9"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[2]);
            EXPECT_TRUE(this->getQualitativeResultAtInitialState(model, result));

            result = checker->check(this->env(), tasks[3]);
            EXPECT_TRUE(this->getQualitativeResultAtInitialState(model, result));

            result = checker->check(this->env(), tasks[4]);
            EXPECT_NEAR(this->parseNumber("5/9"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(MdpPrctlModelCheckerTest, LtlDice) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "Pmax=? [  X (((s1=1) U (s1=3)) U (s1=7))]";
    formulasString += "; Pmax=? [ (F (X (s1=6 & (XX s1=5)))) & (F G (d1!=5))]";
    formulasString += "; Pmax=? [ F s1=3 U (\"three\")]";
    formulasString += "; Pmin=? [! F (s2=6) & X \"done\"]";
    // Acceptance condition not in DNF (Streett, using Spot)
    formulasString += "; Pmax=? [ ( (G F !(\"two\")) | F G (\"three\") ) & ( (G F !(\"five\") ) | F G (\"seven\") )]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(169ul, model->getNumberOfStates());
        EXPECT_EQ(436ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // LTL not supported in all engines (Hybrid,  PrismDd, JaniDd)
        if (TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse) {
            result = checker->check(this->env(), tasks[0]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[1]);
            EXPECT_NEAR(this->parseNumber("1/24"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[2]);
            EXPECT_NEAR(this->parseNumber("1/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[3]);
            EXPECT_NEAR(this->parseNumber("5/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[4]);
            EXPECT_NEAR(this->parseNumber("31/36"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(MdpPrctlModelCheckerTest, LtlCoinFlips) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "Pmax=? [  G (true U (heads | \"done\")) ]";
    formulasString += "; Pmin=? [  G (true U (heads | \"done\")) ]";
    formulasString += "; Pmax=? [  G (true U<=10 (heads | \"done\")) ]";
    formulasString += "; Pmin=? [  G (true U<=10 (heads | \"done\")) ]";
    formulasString += "; Pmax=? [ X G (true U<=0 (heads | \"done\")) ]";
    formulasString += "; Pmax=? [ true U[10,12] heads ]";
    formulasString += "; Pmax=? [ X (true U[0,0] heads) ]";
    formulasString += "; Pmax=? [  G (!(P>=1 [\"done\"]) U<=10 (heads | P>=1 [ \"done\"])) ]";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/coin_flips.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(61ul, model->getNumberOfStates());
        EXPECT_EQ(177ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // LTL not supported in all engines (Hybrid,  PrismDd, JaniDd)
        if (TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse) {
            result = checker->check(this->env(), tasks[0]);
            EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[1]);
            EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[2]);
            EXPECT_NEAR(this->parseNumber("1021/1024"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[3]);
            EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[4]);
            EXPECT_NEAR(this->parseNumber("1/524288"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[5]);
            EXPECT_NEAR(this->parseNumber("7/8"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[6]);
            EXPECT_NEAR(this->parseNumber("1/2"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(this->env(), tasks[7]);
            EXPECT_NEAR(this->parseNumber("1021/1024"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(MdpPrctlModelCheckerTest, HOADice) {
    // "P=? [ F "three" & (X s=1)]"
    std::string formulasString = "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> \"three\", \"p1\" -> s2=1 }]";
    // "P=? [!(s2=6) U "done"]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> !(s2=6), \"p1\" -> \"done\" }]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(169ul, model->getNumberOfStates());
        EXPECT_EQ(436ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // Not supported in all engines (Hybrid,  PrismDd, JaniDd)
        if (TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse) {
            result = checker->check(tasks[0]);
            EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[1]);
            EXPECT_NEAR(this->parseNumber("3/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
}

TYPED_TEST(MdpPrctlModelCheckerTest, SmallDiscount) {
    if (TypeParam::isExact || std::is_same<TypeParam, SparseDoubleLPEnvironment>::value || std::is_same<TypeParam, SparseDoubleViToLPEnvironment>::value) {
        GTEST_SKIP() << "Exact computations for discounted properties are not supported.";
    }
    std::string formulasString = "Rmax=? [ C ]";
    formulasString += "; Rmax=? [ Cdiscount=9/10 ]";
    formulasString += "; Rmax=? [ Cdiscount=15/16 ]";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/small_discount.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(4ul, model->getNumberOfStates());
    EXPECT_EQ(6ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    auto checker = this->createModelChecker(model);

    if (TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse) {
        std::unique_ptr<storm::modelchecker::CheckResult> result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("5"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("18/5"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("15/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
        EXPECT_FALSE(checker->canHandle(tasks[1]));
        EXPECT_FALSE(checker->canHandle(tasks[2]));
    }
}

TYPED_TEST(MdpPrctlModelCheckerTest, OneStateDiscounting) {
    if (TypeParam::isExact || std::is_same<TypeParam, SparseDoubleLPEnvironment>::value || std::is_same<TypeParam, SparseDoubleViToLPEnvironment>::value) {
        GTEST_SKIP() << "Exact computations for discounted properties are not supported.";
    }
    std::string formulasString = "Rmax=? [ Cdiscount=9/10 ]";
    formulasString += "; Rmax=? [ C<=5discount=9/10 ]";
    formulasString += "; Rmax=? [ C<=10discount=9/10 ]";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/one_state.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(1ul, model->getNumberOfStates());
    EXPECT_EQ(2ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    auto checker = this->createModelChecker(model);

    if ((TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse)) {
        std::unique_ptr<storm::modelchecker::CheckResult> result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("40951/100"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("6513215599/10000000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
        EXPECT_FALSE(checker->canHandle(tasks[1]));
        EXPECT_FALSE(checker->canHandle(tasks[2]));
    }
}

TYPED_TEST(MdpPrctlModelCheckerTest, StepBoundedReachability) {
    std::string formulasString = "Pmax=? [ F<=0 x=5];";  // 0
    formulasString += "Pmax=? [ F<=1 x=5];";             // 1/2
    formulasString += "Pmax=? [ F<=2 x=5];";             // 3/4
    formulasString += "Pmax=? [ F[2,2] x=5];";           // 1/4
    formulasString += "Pmax=? [ F[2,3] x=5];";           // 3/8
    formulasString += "Pmin=? [ F[2,3] x=5];";           // 0
    formulasString += "Pmax=? [ x<2 U[2,2] x=5];";       // 1/4
    formulasString += "Pmax=? [ x<2 U[2,3] x=5];";       // 1/4
    formulasString += "Pmax=? [ x<3 U[2,2] x=5];";       // 1/4
    formulasString += "Pmax=? [ x<3 U<=4 x=5];";         // 7/8
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/chain.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(7ul, model->getNumberOfStates());
    EXPECT_EQ(16ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Mdp);
    auto checker = this->createModelChecker(model);

    if (TypeParam::engine == MdpEngine::PrismSparse || TypeParam::engine == MdpEngine::JaniSparse) {
        std::unique_ptr<storm::modelchecker::CheckResult> result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("1/2"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("3/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("1/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[4]);
        EXPECT_NEAR(this->parseNumber("3/8"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[5]);
        EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[6]);
        EXPECT_NEAR(this->parseNumber("1/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[7]);
        EXPECT_NEAR(this->parseNumber("1/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[8]);
        EXPECT_NEAR(this->parseNumber("1/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[9]);
        EXPECT_NEAR(this->parseNumber("7/8"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    }
}
}  // namespace
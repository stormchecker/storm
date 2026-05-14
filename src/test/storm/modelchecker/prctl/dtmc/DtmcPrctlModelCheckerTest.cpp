#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/DirectEncodingParser.h"
#include "storm-parsers/parser/FormulaParser.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/GmmxxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/environment/solver/TopologicalSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/HybridDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SymbolicDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/helper/SparseDtmcPrctlHelper.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/QualitativeCheckResult.h"
#include "storm/modelchecker/results/QuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/EigenLinearEquationSolver.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace {

enum class DtmcEngine { PrismSparse, JaniSparse, Hybrid, PrismDd, JaniDd };

#ifdef STORM_HAVE_GMM
class SparseGmmxxGmresIluEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class JaniSparseGmmxxGmresIluEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::JaniSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class SparseGmmxxGmresDiagEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Diagonal);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class SparseGmmxxBicgstabIluEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Bicgstab);
        env.solver().gmmxx().setPreconditioner(storm::solver::GmmxxLinearEquationSolverPreconditioner::Ilu);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};
#endif

class SparseEigenDGmresEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::DGmres);
        env.solver().eigen().setPreconditioner(storm::solver::EigenLinearEquationSolverPreconditioner::Ilu);
        env.solver().eigen().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class SparseEigenDoubleLUEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class SparseEigenRationalLUEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

class SparseRationalEliminationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Elimination);
        return env;
    }
};

class SparseNativeJacobiEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class SparseNativeWalkerChaeEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::WalkerChae);
        env.solver().native().setMaximalNumberOfIterations(1000000);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-7));
        return env;
    }
};

class SparseNativeSorEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SOR);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class SparseNativePowerEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class SparseNativeSoundValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::SoundValueIteration);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class SparseNativeOptimisticValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::OptimisticValueIteration);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class SparseNativeGuessingValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::GuessingValueIteration);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class SparseNativeIntervalIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::IntervalIteration);
        env.solver().native().setRelativeTerminationCriterion(false);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class SparseNativeRationalSearchEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
        return env;
    }
};

class SparseTopologicalEigenLUEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;  // unused for sparse models
    static const DtmcEngine engine = DtmcEngine::PrismSparse;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::sparse::Dtmc<ValueType> ModelType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Topological);
        env.solver().topological().setUnderlyingEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU);
        return env;
    }
};

#ifdef STORM_HAVE_GMM
class HybridSylvanGmmxxGmresEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const DtmcEngine engine = DtmcEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Gmmxx);
        env.solver().gmmxx().setMethod(storm::solver::GmmxxLinearEquationSolverMethod::Gmres);
        env.solver().gmmxx().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};
#endif

class HybridCuddNativeJacobiEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const DtmcEngine engine = DtmcEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Jacobi);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class HybridCuddNativeSoundValueIterationEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const DtmcEngine engine = DtmcEngine::Hybrid;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        return env;
    }
};

class HybridSylvanNativeRationalSearchEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const DtmcEngine engine = DtmcEngine::Hybrid;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
        return env;
    }
};

class DdSylvanNativePowerEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const DtmcEngine engine = DtmcEngine::PrismDd;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class JaniDdSylvanNativePowerEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const DtmcEngine engine = DtmcEngine::JaniDd;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class DdCuddNativeJacobiEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::CUDD;
    static const DtmcEngine engine = DtmcEngine::PrismDd;
    static const bool isExact = false;
    typedef double ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::Power);
        env.solver().native().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        return env;
    }
};

class DdSylvanRationalSearchEnvironment {
   public:
    static const storm::dd::DdType ddType = storm::dd::DdType::Sylvan;
    static const DtmcEngine engine = DtmcEngine::PrismDd;
    static const bool isExact = true;
    typedef storm::RationalNumber ValueType;
    typedef storm::models::symbolic::Dtmc<ddType, ValueType> ModelType;

    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Native);
        env.solver().native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch);
        return env;
    }
};

template<typename TestType>
class DtmcPrctlModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    typedef typename storm::models::sparse::Dtmc<ValueType> SparseModelType;
    typedef typename storm::models::symbolic::Dtmc<TestType::ddType, ValueType> SymbolicModelType;

    DtmcPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        if constexpr (TestType::engine == DtmcEngine::Hybrid || TestType::engine == DtmcEngine::PrismDd || TestType::engine == DtmcEngine::JaniDd) {
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
        if (TestType::engine == DtmcEngine::PrismSparse) {
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<MT>();
        } else if (TestType::engine == DtmcEngine::JaniSparse) {
            auto janiData = storm::api::convertPrismToJani(program, storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
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
        if (TestType::engine == DtmcEngine::Hybrid || TestType::engine == DtmcEngine::PrismDd) {
            result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
            result.first = storm::api::buildSymbolicModel<TestType::ddType, ValueType>(program, result.second)->template as<MT>();
        } else if (TestType::engine == DtmcEngine::JaniDd) {
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
        if (TestType::engine == DtmcEngine::PrismSparse || TestType::engine == DtmcEngine::JaniSparse) {
            return std::make_shared<storm::modelchecker::SparseDtmcPrctlModelChecker<SparseModelType>>(*model);
        }
    }

    template<typename MT = typename TestType::ModelType>
    typename std::enable_if<std::is_same<MT, SymbolicModelType>::value, std::shared_ptr<storm::modelchecker::AbstractModelChecker<MT>>>::type
    createModelChecker(std::shared_ptr<MT> const& model) const {
        if (TestType::engine == DtmcEngine::Hybrid) {
            return std::make_shared<storm::modelchecker::HybridDtmcPrctlModelChecker<SymbolicModelType>>(*model);
        } else if (TestType::engine == DtmcEngine::PrismDd || TestType::engine == DtmcEngine::JaniDd) {
            return std::make_shared<storm::modelchecker::SymbolicDtmcPrctlModelChecker<SymbolicModelType>>(*model);
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

typedef ::testing::Types<
#ifdef STORM_HAVE_GMM
    SparseGmmxxGmresIluEnvironment, JaniSparseGmmxxGmresIluEnvironment, SparseGmmxxGmresDiagEnvironment, SparseGmmxxBicgstabIluEnvironment,
    HybridSylvanGmmxxGmresEnvironment,
#endif
    SparseEigenDGmresEnvironment, SparseEigenDoubleLUEnvironment, SparseEigenRationalLUEnvironment, SparseRationalEliminationEnvironment,
    SparseNativeJacobiEnvironment, SparseNativeWalkerChaeEnvironment, SparseNativeSorEnvironment, SparseNativePowerEnvironment,
    SparseNativeSoundValueIterationEnvironment, SparseNativeOptimisticValueIterationEnvironment, SparseNativeGuessingValueIterationEnvironment,
    SparseNativeIntervalIterationEnvironment, SparseNativeRationalSearchEnvironment, SparseTopologicalEigenLUEnvironment, HybridCuddNativeJacobiEnvironment,
    HybridCuddNativeSoundValueIterationEnvironment, HybridSylvanNativeRationalSearchEnvironment, DdSylvanNativePowerEnvironment,
    JaniDdSylvanNativePowerEnvironment, DdCuddNativeJacobiEnvironment, DdSylvanRationalSearchEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(DtmcPrctlModelCheckerTest, TestingTypes, );

TYPED_TEST(DtmcPrctlModelCheckerTest, Die) {
    std::string formulasString = "P=? [F \"one\"]";
    formulasString += "; P=? [F \"two\"]";
    formulasString += "; P=? [F \"three\"]";
    formulasString += "; R=? [F \"done\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(13ul, model->getNumberOfStates());
        EXPECT_EQ(20ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("11/3"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(DtmcPrctlModelCheckerTest, Crowds) {
    std::string formulasString = "P=? [F observe0>1]";
    formulasString += "; P=? [F \"observeIGreater1\"]";
    formulasString += "; P=? [F observe1>1]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-4-3.pm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(726ul, model->getNumberOfStates());
        EXPECT_EQ(1146ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("78686542099694893/1268858272000000000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("40300855878315123/1268858272000000000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("13433618626105041/1268858272000000000"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TYPED_TEST(DtmcPrctlModelCheckerTest, SynchronousLeader) {
    std::string formulasString = "P=? [F \"elected\"]";
    formulasString += "; P=? [F<=5 \"elected\"]";
    formulasString += "; R=? [F \"elected\"]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(273ul, model->getNumberOfStates());
        EXPECT_EQ(397ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("1"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("24/25"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("25/24"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    });
}

TEST(DtmcPrctlModelCheckerTest, BoundedReachability) {
    std::string formulasString = "P=? [F<=1 \"a\"]";
    formulasString += "; P=? [F[1,1] \"a\"]";
    formulasString += "; P=? [F[0,2] \"a\"]";

    std::shared_ptr<storm::models::sparse::Model<double>> modelPtr =
        storm::parser::parseDirectEncodingModel<double>(STORM_TEST_RESOURCES_DIR "/dtmc/tiny-01.drn");
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(storm::api::parseProperties(formulasString));
    storm::Environment env;

    std::shared_ptr<storm::models::sparse::Dtmc<double>> dtmc = modelPtr->as<storm::models::sparse::Dtmc<double>>();
    ASSERT_EQ(storm::models::ModelType::Dtmc, modelPtr->getType());
    auto task = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[0]);

    auto checker = storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>>(*dtmc);
    auto result = checker.check(env, task);
    auto filter = std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<double>>(dtmc->getInitialStates());
    result->filter(*filter);
    EXPECT_NEAR(0.2, result->asQuantitativeCheckResult<double>().getMin(), 0.0001);

    task = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[1]);

    result = checker.check(env, task);
    filter = std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<double>>(dtmc->getInitialStates());
    result->filter(*filter);
    EXPECT_NEAR(0.2, result->asQuantitativeCheckResult<double>().getMin(), 0.0001);

    task = storm::modelchecker::CheckTask<storm::logic::Formula, double>(*formulas[2]);

    result = checker.check(env, task);
    filter = std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<double>>(dtmc->getInitialStates());
    result->filter(*filter);
    EXPECT_NEAR(0.366, result->asQuantitativeCheckResult<double>().getMin(), 0.0001);
}

TEST(DtmcPrctlModelCheckerTest, AllUntilProbabilities) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    std::string formulasString = "P=? [F \"one\"]";
    formulasString += "; P=? [F \"two\"]";
    formulasString += "; P=? [F \"three\"]";

    storm::prism::Program program = storm::api::parseProgram(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, double>> tasks;
    for (auto const& f : formulas) {
        tasks.emplace_back(*f);
    }
    auto model = storm::api::buildSparseModel<double>(program, formulas)->template as<storm::models::sparse::Dtmc<double>>();
    EXPECT_EQ(13ul, model->getNumberOfStates());
    EXPECT_EQ(20ul, model->getNumberOfTransitions());

    storm::storage::SparseMatrix<double> matrix = model->getTransitionMatrix();
    storm::storage::BitVector initialStates(13);
    initialStates.set(0);
    storm::storage::BitVector phiStates(13);
    storm::storage::BitVector psiStates(13);
    psiStates.set(7);
    psiStates.set(8);
    psiStates.set(9);
    psiStates.set(10);
    psiStates.set(11);
    psiStates.set(12);
    storm::Environment env;
    storm::solver::SolveGoal<double> goal(*model, tasks[0]);
    std::vector<double> result = storm::modelchecker::helper::SparseDtmcPrctlHelper<double>::computeAllUntilProbabilities(env, std::move(goal), matrix,
                                                                                                                          initialStates, phiStates, psiStates);

    EXPECT_NEAR(1.0 / 6, result[7], 1e-6);
    EXPECT_NEAR(1.0 / 6, result[8], 1e-6);
    EXPECT_NEAR(1.0 / 6, result[9], 1e-6);
    EXPECT_NEAR(1.0 / 6, result[10], 1e-6);
    EXPECT_NEAR(1.0 / 6, result[11], 1e-6);
    EXPECT_NEAR(1.0 / 6, result[12], 1e-6);

    phiStates.set(6);
    psiStates.set(1);
    result = storm::modelchecker::helper::SparseDtmcPrctlHelper<double>::computeAllUntilProbabilities(env, std::move(goal), matrix, initialStates, phiStates,
                                                                                                      psiStates);

    EXPECT_NEAR(1, result[0], 1e-6);
    EXPECT_NEAR(0.5, result[1], 1e-6);
    EXPECT_NEAR(0.5, result[2], 1e-6);
    EXPECT_NEAR(0.25, result[5], 1e-6);
    EXPECT_NEAR(0, result[7], 1e-6);
    EXPECT_NEAR(0, result[8], 1e-6);
    EXPECT_NEAR(0, result[9], 1e-6);
    EXPECT_NEAR(0.125, result[10], 1e-6);
    EXPECT_NEAR(0.125, result[11], 1e-6);
    EXPECT_NEAR(0, result[12], 1e-6);
}

TYPED_TEST(DtmcPrctlModelCheckerTest, LtlProbabilitiesDie) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "P=? [(X s>0) U (s=7 & d=2)]";
    formulasString += "; P=? [  X (((s=1) U (s=3)) U (s=7))]";
    formulasString += "; P=? [ (F (X (s=6 & (XX s=5)))) & (F G (d!=5))]";
    formulasString += "; P=? [ F (s=3 U (\"three\"))]";
    formulasString += "; P=? [ F s=3 U (\"three\")]";
    formulasString += "; P=? [ F (s=6) & X \"done\"]";
    formulasString += "; P=? [ (F s=6) & (X \"done\")]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(13ul, model->getNumberOfStates());
        EXPECT_EQ(20ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // LTL not supported in all engines (Hybrid,  PrismDd, JaniDd)
        if (TypeParam::engine == DtmcEngine::PrismSparse || TypeParam::engine == DtmcEngine::JaniSparse) {
            result = checker->check(tasks[0]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[1]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[2]);
            EXPECT_NEAR(this->parseNumber("1/24"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[3]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[4]);
            EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[5]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[6]);
            EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(DtmcPrctlModelCheckerTest, LtlProbabilitiesSynchronousLeader) {
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "P=? [X (u1=true U \"elected\")]";
    formulasString += "; P=? [X !(u1=true U \"elected\")]";
    formulasString += "; P=? [X v1=2 & X v1=1]";
    formulasString += "; P=? [(X v1=2) & (X v1=1)]";
    formulasString += "; P=? [(!X v1=2) & (X v1=1)]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(273ul, model->getNumberOfStates());
        EXPECT_EQ(397ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // LTL not supported in all engines (Hybrid,  PrismDd, JaniDd)
        if (TypeParam::engine == DtmcEngine::PrismSparse || TypeParam::engine == DtmcEngine::JaniSparse) {
            result = checker->check(tasks[0]);
            EXPECT_NEAR(this->parseNumber("16/25"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[1]);
            EXPECT_NEAR(this->parseNumber("9/25"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[2]);
            EXPECT_NEAR(this->parseNumber("1/25"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[3]);
            EXPECT_NEAR(this->parseNumber("0"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[4]);
            EXPECT_NEAR(this->parseNumber("1/5"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(DtmcPrctlModelCheckerTest, HOAProbabilitiesDie) {
    // "P=? [(X s>0) U (s=7 & d=2)]"
    std::string formulasString = "P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> (s>0), \"p1\" -> (s=7 & d=2) }]";
    // "P=? [(X s>0) U (d=4 | d=2)]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> (s>0), \"p1\" -> (d=4 | d=2) }]";
    // "P=? [ (F s=4) & (X \"three\")]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> (s=4), \"p1\" -> \"three\" }]";
    // "P=? [ (F s=6) & (X \"done\")]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> (s=6), \"p1\" -> \"done\" }]";
    // "P=? [ (F s=6) & (X !\"done\")]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> (s=6), \"p1\" -> !\"done\" }]";
    // "P=? [ (F (s=4 | s=5)) & (X (\"three\" | \"five\"))]"
    formulasString += "; P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> (s=4 | s=5), \"p1\" -> s=7 & (d=3 | d=5) }]";
    // "P=? [ (F (s=4 | s=5)) & (X (\"three\" | \"five\"))]"
    formulasString += "; P>0.3[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> (s=4 | s=5), \"p1\" -> s=7 & (d=3 | d=5) }]";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    this->execute(model, [&]() {
        EXPECT_EQ(13ul, model->getNumberOfStates());
        EXPECT_EQ(20ul, model->getNumberOfTransitions());
        ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
        auto checker = this->createModelChecker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> result;

        // Not supported in all engines (Hybrid,  PrismDd, JaniDd)
        if (TypeParam::engine == DtmcEngine::PrismSparse || TypeParam::engine == DtmcEngine::JaniSparse) {
            result = checker->check(tasks[0]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[1]);
            EXPECT_NEAR(this->parseNumber("1/3"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[2]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[3]);
            EXPECT_NEAR(this->parseNumber("1/6"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[4]);
            EXPECT_NEAR(this->parseNumber("1/8"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[5]);
            EXPECT_NEAR(this->parseNumber("1/3"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

            result = checker->check(tasks[6]);
            EXPECT_TRUE(this->getQualitativeResultAtInitialState(model, result));
        } else {
            EXPECT_FALSE(checker->canHandle(tasks[0]));
        }
    });
}

TYPED_TEST(DtmcPrctlModelCheckerTest, SmallDiscount) {
    if (TypeParam::isExact) {
        GTEST_SKIP() << "Exact computations for discounted properties are not supported.";
    }
    std::string formulasString = "R=? [ C ]";
    formulasString += "; R=? [ Cdiscount=9/10 ]";
    formulasString += "; R=? [ Cdiscount=15/16 ]";
    formulasString += "; R=? [ C<5discount=9/10 ]";
    formulasString += "; R=? [ C<5discount=15/16 ]";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/dtmc/small_discount.nm", formulasString);
    auto model = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(3ul, model->getNumberOfStates());
    EXPECT_EQ(4ul, model->getNumberOfTransitions());
    ASSERT_EQ(model->getType(), storm::models::ModelType::Dtmc);
    auto checker = this->createModelChecker(model);

    if (TypeParam::engine == DtmcEngine::PrismSparse || TypeParam::engine == DtmcEngine::JaniSparse) {
        std::unique_ptr<storm::modelchecker::CheckResult> result = checker->check(this->env(), tasks[0]);
        EXPECT_NEAR(this->parseNumber("5"), this->getQuantitativeResultAtInitialState(model, result), this->precision());

        result = checker->check(this->env(), tasks[1]);
        EXPECT_NEAR(this->parseNumber("45/14"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[2]);
        EXPECT_NEAR(this->parseNumber("15/4"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[3]);
        EXPECT_NEAR(this->parseNumber("12591/6250"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
        result = checker->check(this->env(), tasks[4]);
        EXPECT_NEAR(this->parseNumber("555/256"), this->getQuantitativeResultAtInitialState(model, result), this->precision());
    } else {
        EXPECT_FALSE(checker->canHandle(tasks[0]));
        EXPECT_FALSE(checker->canHandle(tasks[1]));
        EXPECT_FALSE(checker->canHandle(tasks[2]));
        EXPECT_FALSE(checker->canHandle(tasks[3]));
        EXPECT_FALSE(checker->canHandle(tasks[4]));
    }
}

}  // namespace

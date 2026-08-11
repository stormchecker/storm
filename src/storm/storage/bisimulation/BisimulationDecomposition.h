#pragma once

#include "storm/logic/Formulas.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/Decomposition.h"
#include "storm/storage/StateBlock.h"
#include "storm/storage/bisimulation/BisimulationType.h"
#include "storm/storage/bisimulation/Partition.h"
#include "storm/utility/ConstantsComparator.h"

namespace storm {
namespace logic {
class Formula;
}

namespace storage {

/*!
 * This class is the superclass of all decompositions of a sparse model into its bisimulation quotient.
 */
template<typename ModelType, typename BlockDataType>
class BisimulationDecomposition : public Decomposition<StateBlock> {
   public:
    typedef typename ModelType::ValueType ValueType;
    typedef typename ModelType::RewardModelType RewardModelType;

    // A class that offers the possibility to customize the bisimulation.
    struct Options {
        /*!
         * Creates an object representing the options necessary to obtain the quotient while still preserving
         * the given formula.
         *
         * @param model The model for which the quotient model shall be computed. This needs to be given in order to
         * derive a suitable initial partition.
         * @param formula The formula that is to be preserved.
         * @param tolerance The tolerance used for comparing constants (irrelevant if ValueType is exact).
         */
        Options(ModelType const& model, storm::logic::Formula const& formula, ValueType const& tolerance);

        /*!
         * Creates an object representing the options necessary to obtain the smallest quotient while still
         * preserving the given formulas.
         *
         * @param model The model for which the quotient model shall be computed. This needs to be given in order to
         * derive a suitable initial partition.
         * @param formulas The formulas that need to be preserved.
         * @param tolerance The tolerance used for comparing constants (irrelevant if ValueType is exact).
         */
        Options(ModelType const& model, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas, ValueType const& tolerance);

        /*!
         * Creates an object representing the options necessary to obtain the quotient that respects all atomic
         * propositions of the model (rather than those relevant to some formula).
         *
         * @param tolerance The tolerance used for comparing constants (irrelevant if ValueType is exact).
         */
        static Options preservingAllLabels(ValueType const& tolerance);

        /*!
         * Changes the options in a way that the given formula is preserved.
         *
         * @param formula The only formula to check.
         */
        void preserveFormula(storm::logic::Formula const& formula);

        /**
         * Sets the bisimulation type. If the bisimulation type is set to weak,
         * we also change the bounded flag (as bounded properties are not preserved under
         * weak bisimulation).
         */
        void setType(BisimulationType t) {
            if (t == BisimulationType::Weak) {
                STORM_LOG_WARN_COND(!bounded, "Weak bisimulation does not preserve bounded properties.");
                STORM_LOG_WARN_COND(!discounted, "Weak bisimulation does not preserve discounted properties.");
                bounded = false;
                discounted = false;
            }
            type = t;
        }

        BisimulationType getType() const {
            return this->type;
        }

        bool getBounded() const {
            return this->bounded;
        }

        bool getDiscounted() const {
            return this->discounted;
        }

        bool getKeepRewards() const {
            return this->keepRewards;
        }

        void setKeepRewards(bool keepRewards) {
            this->keepRewards = keepRewards;
        }

        bool isOptimizationDirectionSet() const {
            return static_cast<bool>(optimalityType);
        }

        ValueType getTolerance() const {
            return storm::NumberTraits<ValueType>::IsExact ? storm::utility::zero<ValueType>() : tolerance;
        }

        void setTolerance(ValueType value) {
            tolerance = value;
        }

        OptimizationDirection getOptimizationDirection() const {
            STORM_LOG_ASSERT(optimalityType, "Optimality type not set.");
            return optimalityType.value();
        }

        // A flag that indicates whether a measure driven initial partition is to be used. If this flag is set
        // to true, the two optional pairs phiStatesAndLabel and psiStatesAndLabel must be set. Then, the
        // measure driven initial partition wrt. to the states phi and psi is taken.
        bool measureDrivenInitialPartition = false;
        std::optional<storm::storage::BitVector> phiStates;
        std::optional<storm::storage::BitVector> psiStates;

        /// An optional set of strings that indicate which of the atomic propositions of the model are to be
        /// respected and which may be ignored. If not given, all atomic propositions of the model are respected.
        std::optional<std::set<std::string>> respectedAtomicPropositions;

        /// A flag that governs whether the quotient model is actually built or only the decomposition is computed.
        bool buildQuotient = true;

       private:
        std::optional<OptimizationDirection> optimalityType;

        /// A flag that indicates whether the state-rewards of the model are to be respected (and should
        /// be kept in the quotient model, if one is built).
        bool keepRewards = false;

        /// A flag that indicates whether a strong or a weak bisimulation is to be computed.
        BisimulationType type = BisimulationType::Strong;

        /// A flag that indicates whether step-bounded properties are to be preserved. This may only be set to tru
        /// when computing strong bisimulation equivalence.
        bool bounded = false;

        /// A flag that indicates whether discounted properties are to be preserved. This may only be set to true
        /// when computing strong bisimulation equivalence.
        bool discounted = false;

        /// The tolerance used for comparing constants (irrelevant if ValueType is exact).
        ValueType tolerance;

        /*!
         * Creates an object representing the default values for all options except the tolerance, which must
         * always be supplied deliberately by the caller.
         */
        explicit Options(ValueType const& tolerance);

        /*!
         * Sets the options under the assumption that the given formula is the only one that is to be checked.
         *
         * @param model The model for which to preserve the formula.
         * @param formula The only formula to check.
         */
        void preserveSingleFormula(ModelType const& model, storm::logic::Formula const& formula);

        /*!
         * Adds the given expressions and labels to the set of respected atomic propositions.
         *
         * @param expressions The expressions to respect.
         * @param labels The labels to respect.
         */
        void addToRespectedAtomicPropositions(std::vector<std::shared_ptr<storm::logic::AtomicExpressionFormula const>> const& expressions,
                                              std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> const& labels);

        /*
         * Checks whether a measure driven partition is possible with the given formula and sets the necessary
         * data if this is the case.
         *
         * @param model The model for which to derive the data.
         * @param formula The formula for which to derive the data for the measure driven initial partition (if
         * applicable).
         */
        void checkAndSetMeasureDrivenInitialPartition(ModelType const& model, storm::logic::Formula const& formula);
    };

    /*!
     * Decomposes the given model into equivalence classes of a bisimulation.
     *
     * @param model The model to decompose.
     * @param options The options to use during for the decomposition.
     */
    BisimulationDecomposition(ModelType const& model, Options const& options);

    virtual ~BisimulationDecomposition() = default;

    /*!
     * Retrieves the quotient of the model under the computed bisimulation.
     *
     * @return The quotient model.
     */
    std::shared_ptr<ModelType> getQuotient() const;

    /*!
     * Computes the decomposition of the model into bisimulation equivalence classes. If requested, a quotient
     * model is built.
     */
    void computeBisimulationDecomposition();

   protected:
    /*!
     * Decomposes the given model into equivalence classes of a bisimulation.
     *
     * @param model The model to decompose.
     * @param backwardTransitions The backward transitions of the model.
     * @param options The options to use during for the decomposition.
     */
    BisimulationDecomposition(ModelType const& model, storm::storage::SparseMatrix<ValueType> const& backwardTransitions, Options const& options);

    /*!
     * Performs the partition refinement on the model and thereby computes the equivalence classes under strong
     * bisimulation equivalence. If required, the quotient model is built and may be retrieved using
     * getQuotient().
     */
    void performPartitionRefinement();

    /*!
     * Refines the partition by considering the given splitter. All blocks that become potential splitters
     * because of this refinement, are marked as splitters and inserted into the splitter vector.
     *
     * @param splitter The splitter to use.
     * @param splitterVector The vector into which to insert the newly discovered potential splitters.
     */
    virtual void refinePartitionBasedOnSplitter(bisimulation::Block<BlockDataType>& splitter,
                                                std::vector<bisimulation::Block<BlockDataType>*>& splitterVector) = 0;

    /*!
     * Builds the quotient model based on the previously computed equivalence classes (stored in the blocks of the decomposition).
     */
    virtual void buildQuotient() = 0;

    /*!
     * Initializes the initial partition based on all respected labels.
     */
    virtual void initializeLabelBasedPartition();

    /*!
     * Creates the measure-driven initial partition for reaching psi states from phi states.
     */
    virtual void initializeMeasureDrivenPartition();

    /*!
     * A function that can initialize auxiliary data structures. It is called after initializing the initial partition.
     */
    virtual void initialize();

    /*!
     * Computes the set of states with probability 0/1 for satisfying phi until psi. This is used for the measure
     * driven initial partition.
     *
     * @return The states with probability 0 and 1.
     */
    virtual std::pair<storm::storage::BitVector, storm::storage::BitVector> getStatesWithProbability01() = 0;

    /*!
     * Splits the initial partition based on the (unique) reward model of the current model.
     */
    virtual void splitInitialPartitionBasedOnRewards();

    /*!
     * Splits the initial partition based on the given reward vector.
     */
    virtual void splitInitialPartitionBasedOnRewards(std::vector<ValueType> const& rewardVector);

    /*!
     * Splits the initial partition based on the given vector of action rewards.
     */
    virtual void splitInitialPartitionBasedOnActionRewards(std::vector<std::set<ValueType>> const& rewardVector);

    /*!
     * Constructs the blocks of the decomposition object based on the current partition.
     */
    void extractDecompositionBlocks();

    // The model to decompose.
    ModelType const& model;

    // The backward transitions of the model.
    storm::storage::SparseMatrix<ValueType> backwardTransitions;

    // The options used during construction.
    Options options;

    // The current partition (used by partition refinement).
    storm::storage::bisimulation::Partition<BlockDataType> partition;

    // A comparator used for comparing the distances of constants.
    storm::utility::ConstantsComparator<ValueType> comparator;

    // The quotient, if it was build. Otherwise a null pointer.
    std::shared_ptr<ModelType> quotient;
};
}  // namespace storage
}  // namespace storm

// #pragma once
// #include <string>
//
// #include "storm/models/sparse/MarkovAutomaton.h"
// #include "storm/models/sparse/StandardRewardModel.h"
// #include "storm/storage/BitVector.h"
// #include "storm/storage/BitVectorHashMap.h"
// #include "storm/storage/gspn/GSPN.h"
// #include "storm/storage/gspn/ImmediateTransition.h"
// #include "storm/storage/gspn/TimedTransition.h"
//
//  namespace storm {
//     namespace builder {
//
//         /*!
//          * This class provides the facilities for building an markov automaton.
//          */
//         template<typename ValueType=double>
//         class ExplicitGspnModelBuilder {
//         public:
//
//             /*!
//              * Builds an MarkovAutomaton from the given GSPN.
//              *
//              * @param gspn The gspn whose semantic is covered by the MarkovAutomaton
//              * @return The resulting MarkovAutomaton
//              */
//             storm::models::sparse::MarkovAutomaton<ValueType> translateGspn(storm::gspn::GSPN const& gspn, std::string const& formula);
//         private:
//
//             /*!
//              * Add for each partition a new row in the probability matrix.
//              *
//              * @param partitions The partitioned immediate transitions.
//              * @param currentMarking The current marking which is considered at the moment.
//              */
//             void addRowForPartitions(std::vector<std::vector<std::shared_ptr<storm::gspn::ImmediateTransition<double>>>> const& partitions,
//             storm::gspn::Marking const& currentMarking);
//
//
//             /*!
//              * Add row for a markovian state.
//              *
//              * @param enabledTimedTransitions List of enabled timed transitions.
//              * @param currentMarking The current marking which is considered at the moment.
//              * @param accRate The sum of all rates of the enabled timed transisitons
//              */
//             void addRowForTimedTransitions(std::vector<std::shared_ptr<storm::gspn::TimedTransition<double>>> const& enabledTimedTransitions,
//             storm::gspn::Marking const& currentMarking, double const& accRate);
//
//             /*!
//              * Struct for comparing unsigned integer for maps
//              */
//             struct cmpByIndex {
//                 bool operator()(const uint_fast64_t &a, const uint_fast64_t &b) const {
//                     return a < b;
//                 }
//             };
//
//             /*!
//              * This method insert the values from a map into the matrix
//              *
//              * @param values The values which are inserted
//              */
//             void addValuesToBuilder(std::map<uint_fast64_t , double, storm::builder::ExplicitGspnModelBuilder<ValueType>::cmpByIndex> const& values);
//
//
//             /*!
//              * This method partitions all enabled immediate transitions w.r.t. a marking.
//              * All enabled weighted immediate transitions are in one vector. Between these transitions
//              * is chosen probabilistically by the weights.
//              *
//              * All other enabled non-weighted immediate transitions are in an singleton vector.
//              * Between different vectors is chosen non-deterministically.
//              *
//              * @param marking The current marking which is considered.
//              * @param enabledImmediateTransistions A vector of enabled immediate transitions.
//              * @return The vector of vectors which is described above.
//              */
//             std::vector<std::vector<std::shared_ptr<storm::gspn::ImmediateTransition<double>>>> partitonEnabledImmediateTransitions(storm::gspn::Marking
//             const& marking, std::vector<std::shared_ptr<storm::gspn::ImmediateTransition<double>>> const& enabledImmediateTransitions);
//
//             /*!
//              * Computes the accumulated weight of immediate transisions which are stored in a list.
//              *
//              * @param vector List of immediate transisitions.
//              */
//             double getAccumulatedWeight(std::vector<std::shared_ptr<storm::gspn::ImmediateTransition<double>>> const& vector) const;
//
//             /*!
//              * Compute the upper bound for the number of tokens in each place.
//              * Also computes the number of bits which are used to store a marking.
//              *
//              * @param gspn The corresponding gspn.
//              */
//             void computeCapacities(storm::gspn::GSPN const& gspn);
//
//             /*!
//              * Returns the vector of enabled timed transition with the highest priority.
//              *
//              *
//              * @param marking The current marking which is considered.
//              * @return The vector of enabled timed transitions.
//              */
//             std::vector<std::shared_ptr<storm::gspn::TimedTransition<double>>> getEnabledTimedTransition(
//                     storm::gspn::Marking const& marking);
//
//             /*!
//              * Returns the vector of active immediate transition with the highest priority.
//              *
//              * @param marking The current marking which is considered.
//              * @return The vector of enabled immediate transitions.
//              */
//             std::vector<std::shared_ptr<storm::gspn::ImmediateTransition<double>>> getEnabledImmediateTransition(
//                     storm::gspn::Marking const& marking);
//
//             /*!
//              * Computes the accumulated rate of a vector of timed transitions.
//              *
//              * @param vector The vector of timed transitions.
//              * @return The accumulated rate.
//              */
//             double getAccumulatedRate(std::vector<std::shared_ptr<storm::gspn::TimedTransition<double>>> const& vector);
//
//             // is only needed for debugging purposes
//             // since markings is injective, we can determine the bitvector
//             storm::storage::BitVector getBitvector(uint_fast64_t const& index);
//
//             /*!
//              * Adds the bitvector to the marking-map.
//              * If the bitvector corresponds to a new marking the bitvector is added to the todo list.
//              *
//              * @return The rowGroup index for the bitvector.
//              */
//             uint_fast64_t findOrAddBitvectorToMarkings(storm::storage::BitVector const& bitvector);
//
//             /*!
//              * Computes the state labeling and returns it.
//              * Every state is labeled with its name.
//              *
//              * @return The computed state labeling.
//              */
//             storm::models::sparse::StateLabeling getStateLabeling() const;
//
//
//             // contains the number of bits which are used the store the number of tokens at each place
//             std::map<uint_fast64_t, uint_fast64_t> numberOfBits;
//
//             // contains the number of bits used to create markings
//             uint_fast64_t numberOfTotalBits;
//
//             // maps bitvectors (markings w.r.t. the capacity) to their rowgroup
//             storm::storage::BitVectorHashMap<uint_fast64_t> markings = storm::storage::BitVectorHashMap<uint_fast64_t>(64, 1);
//
//             // a list of markings for which the outgoing edges need to be computed
//             std::deque<std::shared_ptr<storm::storage::BitVector>> todo;
//
//             //the gspn which is transformed
//             storm::gspn::GSPN gspn;
//
//             // the next index for a new rowgroup
//             uint_fast64_t nextRowGroupIndex = 0;
//
//             // builder object which is used to create the probability matrix
//             storm::storage::SparseMatrixBuilder<double> builder;
//
//             // contains the current row index during the computation
//             uint_fast64_t currentRowIndex;
//         };
//     }
// }
//

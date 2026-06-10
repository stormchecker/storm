#pragma once
#include <sys/types.h>
#include <cstdint>
#include <utility>
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"

namespace storm::generator {

/*
 * Based on the paper "Learning Verified Monitors for Hidden Markov Models" ATVA25 (https://doi.org/10.1007/978-3-032-08707-2_9)
 */

/*
 * Result of GenerateMonitorVerifier.
 * It contains the product POMDP, where the states are pairs of states of the monitor and the HMM, and the observations are pairs of step number and accepting
 * label. It also contains a mapping from the observation pairs to the observation number and a mapping from the observation number to the default action for
 * that observation.
 */
template<typename ValueType>
class MonitorVerifier {
   public:
    MonitorVerifier(const storm::models::sparse::Pomdp<ValueType>& product, const std::map<std::pair<uint32_t, bool>, uint32_t>& observationMap,
                    std::map<uint32_t, std::string> observationDefaultAction);

    std::map<std::pair<uint32_t, bool>, uint32_t> const& getObservationMap();
    const std::map<uint32_t, std::string>& getObservationDefaultAction();
    const storm::models::sparse::Pomdp<ValueType>& getProduct();

   private:
    storm::models::sparse::Pomdp<ValueType> product;
    std::map<std::pair<uint32_t, bool>, uint32_t> observationMap;
    std::map<uint32_t, std::string> observationDefaultAction;
};

/*
 * This class generates a product of a HMM and a monitor. The HMM is represented as a DTMC where observations are encoded as labels. Risk values must be
 * provided via setRisk() before calling createProduct(). The monitor is represented as an MDP where the actions correspond to the observations of the HMM and
 * the states are labeled with the acceptingLabel. The monitor is unrolled and the steps are labeled with the stepPrefix and then a number. The states at the
 * horizon after unrolling are labeled with the horizonLabel. Restart semantics decides if states which failed the condition are directed to the initial state
 * or to a sink state.
 *
 */
template<typename ValueType>
class GenerateMonitorVerifier {
   public:
    struct Options {
        std::string acceptingLabel = "accepting";
        std::string stepPrefix = "step";
        std::string horizonLabel = "horizon";
        bool useRestartSemantics = true;
    };
    GenerateMonitorVerifier(storm::models::sparse::Dtmc<ValueType> const& mc, storm::models::sparse::Mdp<ValueType> const& monitor,
                            std::shared_ptr<storm::expressions::ExpressionManager>& exprManager, Options const& options);
    std::shared_ptr<MonitorVerifier<ValueType>> createProduct();
    void setRisk(std::vector<ValueType> const& risk);

   private:
    const storm::models::sparse::Dtmc<ValueType>& mc;
    const storm::models::sparse::Mdp<ValueType>& monitor;
    std::vector<ValueType> risk;
    std::shared_ptr<storm::expressions::ExpressionManager>& exprManager;
    storm::expressions::Variable monvar;
    storm::expressions::Variable mcvar;
    Options options;
};

}  // namespace storm::generator
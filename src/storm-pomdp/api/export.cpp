#include "storm-pomdp/api/export.h"

#include <fstream>

#include "storm-pomdp/beliefs/policy/ObservationBasedFiniteStateController.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/io/file.h"

namespace storm::pomdp::api {

template<typename ValueType>
void exportPolicyAsJson(storm::pomdp::policy::ObservationBasedFiniteStateController<ValueType> const& policy, std::string const& filename) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    policy.writeJsonToStream(stream);
    storm::io::closeFile(stream);
}

template<typename ValueType>
void exportPolicyAsDot(storm::pomdp::policy::ObservationBasedFiniteStateController<ValueType> const& policy, std::string const& filename) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    policy.writeDotToStream(stream);
    storm::io::closeFile(stream);
}

template void exportPolicyAsJson(storm::pomdp::policy::ObservationBasedFiniteStateController<double> const&, std::string const&);
template void exportPolicyAsJson(storm::pomdp::policy::ObservationBasedFiniteStateController<storm::RationalNumber> const&, std::string const&);
template void exportPolicyAsDot(storm::pomdp::policy::ObservationBasedFiniteStateController<double> const&, std::string const&);
template void exportPolicyAsDot(storm::pomdp::policy::ObservationBasedFiniteStateController<storm::RationalNumber> const&, std::string const&);

}  // namespace storm::pomdp::api

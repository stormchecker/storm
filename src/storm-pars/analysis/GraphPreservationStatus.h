#pragma once

#include <ostream>

namespace storm::pars::analysis {

/*!
 * Describes to what extent a set of regions is known to be well-defined and to preserve the graph structure of
 * the parametric model.
 */
enum class GraphPreservationStatus {
    /*!
     * No guarantee. The graph structure of the model may change within a region. Only engines that can handle
     * non-graph-preserving regions (currently robust parameter lifting) may be used in this case.
     */
    None,
    /*!
     * The regions are known to be well-defined, i.e. all probabilities lie in [0, 1] and all denominators are
     * non-zero, but it is not known whether they preserve the graph structure of the model.
     */
    WellDefined,
    /*!
     * The regions are known (or assumed) to preserve the graph structure of the model. This implies that they are
     * well-defined, as a model that is not well-formed cannot be graph-preserving.
     */
    GraphPreserving,
    /*!
     * It could not be established whether the regions are well-defined or preserve the graph structure of the
     * model.
     */
    Unknown
};

/*!
 * Returns whether the given status guarantees that the graph structure of the model is preserved.
 */
inline bool isGraphPreserving(GraphPreservationStatus status) {
    return status == GraphPreservationStatus::GraphPreserving;
}

inline std::ostream& operator<<(std::ostream& stream, GraphPreservationStatus status) {
    switch (status) {
        case GraphPreservationStatus::None:
            stream << "none";
            break;
        case GraphPreservationStatus::WellDefined:
            stream << "well-defined";
            break;
        case GraphPreservationStatus::GraphPreserving:
            stream << "graph-preserving";
            break;
        case GraphPreservationStatus::Unknown:
            stream << "unknown";
            break;
    }
    return stream;
}

}  // namespace storm::pars::analysis

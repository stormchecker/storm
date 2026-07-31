
#include "storm-gspn/parser/PnmlParser.h"
#ifdef STORM_HAVE_XERCES
#include <iostream>

#include "storm-gspn/adapters/XercesAdapter.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace {
bool isOnlyWhitespace(std::string const& in) {
    return std::all_of(in.begin(), in.end(), [](char c) { return std::isspace(static_cast<unsigned char>(c)); });
}
}  // namespace
namespace storm {
namespace parser {

storm::gspn::GSPN* PnmlParser::parse(xercesc::DOMElement const* elementRoot) {
    if (storm::adapters::getName(elementRoot) == "pnml") {
        traversePnmlElement(elementRoot);
    } else {
        // If the top-level node is not a "pnml" or "" node, then throw an exception.
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Failed to identify the root element.");
    }
    return builder.buildGspn();
}

void PnmlParser::traversePnmlElement(xercesc::DOMElement const* const element) {
    // traverse attributes
    for (uint_fast64_t i = 0; i < element->getAttributes()->getLength(); ++i) {
        auto attr = element->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        // Found node or attribute which is at the moment nod handled by this parser.
        // Notify the user and continue the parsing.
        STORM_PRINT_AND_LOG("unknown attribute (node=pnml): " + name + "\n");
    }

    // traverse children
    for (uint_fast64_t i = 0; i < element->getChildNodes()->getLength(); ++i) {
        auto child = element->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name == "net") {
            traverseNetOrPage(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=pnml): " + name + "\n");
        }
    }
}

void PnmlParser::traverseNetOrPage(xercesc::DOMNode const* const node) {
    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name == "id") {
            builder.setGspnName(storm::adapters::XMLtoString(attr->getNodeValue()));
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name == "place") {
            traversePlace(child);
        } else if (name == "transition") {
            traverseTransition(child);
        } else if (name == "arc") {
            traverseArc(child);
        } else if (name == "page") {
            // Some pnml files have a child named page.
            // The page node has the same children like the net node (e.g., place, transition, arc)
            traverseNetOrPage(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

void PnmlParser::traversePlace(xercesc::DOMNode const* const node) {
    std::string placeName;
    // the first entry is false if the corresponding information was not found in the pnml file
    std::pair<bool, uint_fast64_t> numberOfInitialTokens(false, defaultNumberOfInitialTokens);
    std::pair<bool, boost::optional<uint64_t>> capacity(false, boost::none);

    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name == "id") {
            placeName = storm::adapters::XMLtoString(attr->getNodeValue());
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown attribute (node=place): " + name + "\n");
        }
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name == "initialMarking") {
            numberOfInitialTokens.first = true;
            numberOfInitialTokens.second = traverseInitialMarking(child);
        } else if (name == "capacity") {
            capacity.first = true;
            capacity.second = traverseCapacity(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else if (name == "name" || name == "graphics") {
            // ignore these tags
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=place): " + name + "\n");
        }
    }

    if (!numberOfInitialTokens.first) {
        // no information about the number of initial tokens is found
        // use the default number of initial tokens
        STORM_PRINT_AND_LOG("unknown numberOfInitialTokens (place=" + placeName + ")\n");
    }
    if (!capacity.first) {
        // no information about the capacity is found
        STORM_PRINT_AND_LOG("unknown capacity (place=" + placeName + ")\n");
    }
    builder.addPlace(capacity.second, numberOfInitialTokens.first ? numberOfInitialTokens.second : 0, placeName);
}

void PnmlParser::traverseTransition(xercesc::DOMNode const* const node) {
    // the first entry is false if the corresponding information was not found in the pnml file
    std::pair<bool, bool> timed(false, defaultTransitionType);
    std::pair<bool, std::string> value(false, "");
    std::string id;
    uint_fast64_t priority = defaultPriority;

    // parse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name == "id") {
            id = storm::adapters::XMLtoString(attr->getNodeValue());
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown attribute (node=transition): " + name + "\n");
        }
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name == "rate") {
            value.first = true;
            value.second = traverseTransitionValue(child);
        } else if (name == "timed") {
            timed.first = true;
            timed.second = traverseTransitionType(child);
        } else if (name == "priority") {
            priority = traversePriority(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else if (name == "graphics" || name == "name" || name == "orientation") {
            // ignore these tags
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=transition): " + name + "\n");
        }
    }

    // build transition and add it to the GSPN
    STORM_LOG_THROW(timed.first, storm::exceptions::UnexpectedException, "Unknown transition type (transition=" + id + ").");

    if (timed.second) {
        // no information about the rate is found -> abort the parsing
        STORM_LOG_THROW(value.first, storm::exceptions::UnexpectedException, "Unknown transition rate (transition=" + id + ").");
        builder.addTimedTransition(priority, std::stod(value.second), id);
    } else {
        if (!value.first) {
            // no information about the weight is found -> continue with the default weight
            STORM_PRINT_AND_LOG("unknown transition weight (transition=" + id + ")\n");
        }
        builder.addImmediateTransition(priority, std::stod(value.second), id);
    }
}

void PnmlParser::traverseArc(xercesc::DOMNode const* const node) {
    // the first entry is false if the corresponding information was not found in the pnml file
    std::pair<bool, std::string> source(false, "");
    std::pair<bool, std::string> target(false, "");
    std::pair<bool, std::string> type(false, defaultArcType);
    std::pair<bool, uint_fast64_t> multiplicity(false, defaultMultiplicity);
    std::string id;

    // parse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name == "source") {
            source.first = true;
            source.second = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name == "target") {
            target.first = true;
            target.second = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name == "id") {
            id = storm::adapters::XMLtoString(attr->getNodeValue());
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown attribute (node=arc): " + name + "\n");
        }
    }

    // parse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name == "type") {
            type.first = true;
            type.second = traverseArcType(child);
        } else if (name == "inscription") {
            multiplicity.first = true;
            multiplicity.second = traverseMultiplicity(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else if (name == "graphics" || name == "arcpath" || name == "tagged") {
            // ignore these tags
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=arc): " + name + "\n");
        }
    }

    // check if all necessary information where stored in the pnml file
    // could not find start of the arc -> abort parsing
    STORM_LOG_THROW(source.first, storm::exceptions::UnexpectedException, "Unknown arc source (arc=" + id + ").");
    // could not find the target of the arc -> abort parsing
    STORM_LOG_THROW(target.first, storm::exceptions::UnexpectedException, "Unknown arc target (arc=" + id + ").");
    if (!multiplicity.first) {
        // no information about the multiplicity of the arc
        // continue and use the default multiplicity
        STORM_PRINT_AND_LOG("unknown multiplicity (node=arc): " + id + "\n");
    }

    if (type.second == "normal") {
        builder.addNormalArc(source.second, target.second, multiplicity.second);
    } else if (type.second == "inhibition") {
        builder.addInhibitionArc(source.second, target.second, multiplicity.second);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Arc type '" << type.second << "' in arc '" << id << "' is unknown.");
    }
}

uint_fast64_t PnmlParser::traverseInitialMarking(xercesc::DOMNode const* const node) {
    uint_fast64_t result = defaultNumberOfInitialTokens;
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name == "text") {
            result = std::stoull(storm::adapters::getName(child->getFirstChild()));
        } else if (name == "value") {
            auto value = storm::adapters::getName(child->getFirstChild());
            value = value.substr(std::string("Default,").length());
            result = std::stoull(value);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else if (name == "graphics") {
            // ignore these tags
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=initialMarking): " + name + "\n");
        }
    }
    return result;
}

int_fast64_t PnmlParser::traverseCapacity(xercesc::DOMNode const* const node) {
    int_fast64_t result = defaultCapacity;
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name == "value") {
            auto value = storm::adapters::getName(child->getFirstChild());
            if (value.find("Default,") == 0) {
                value = value.substr(std::string("Default,").length());
            }
            result = std::stoull(value);
        } else if (name == "graphics") {
            // ignore these nodes
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=capacity): " + name + "\n");
        }
    }
    return result;
}

uint_fast64_t PnmlParser::traverseMultiplicity(xercesc::DOMNode const* const node) {
    uint_fast64_t result = defaultMultiplicity;
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name == "value") {
            auto value = storm::adapters::getName(child->getFirstChild());
            if (value.find("Default,") == 0) {
                value = value.substr(std::string("Default,").length());
            }
            result = std::stoull(value);
        } else if (name == "graphics") {
            // ignore these nodes
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=inscription): " + name + "\n");
        }
    }
    return result;
}

std::string PnmlParser::traverseTransitionValue(xercesc::DOMNode const* const node) {
    std::string result;
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name == "value") {
            result = storm::adapters::getName(child->getFirstChild());
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=rate): " + name + "\n");
        }
    }
    return result;
}

bool PnmlParser::traverseTransitionType(xercesc::DOMNode const* const node) {
    bool result;
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name == "value") {
            result = storm::adapters::getName(child->getFirstChild()) == "true";
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=timed): " + name + "\n");
        }
    }
    return result;
}

std::string PnmlParser::traverseArcType(xercesc::DOMNode const* const node) {
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);
        if (name == "value") {
            return storm::adapters::XMLtoString(attr->getNodeValue());
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=type): " + name + "\n");
        }
    }
    return defaultArcType;
}

uint_fast64_t PnmlParser::traversePriority(xercesc::DOMNode const* const node) {
    uint_fast64_t result = defaultPriority;
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name == "text") {
            result = std::stoull(storm::adapters::getName(child->getFirstChild()));
        } else if (name == "value") {
            auto value = storm::adapters::getName(child->getFirstChild());
            value = value.substr(std::string("Default,").length());
            result = std::stoull(value);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else if (name == "graphics") {
            // ignore these tags
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_PRINT_AND_LOG("unknown child (node=priority): " + name + "\n");
        }
    }
    return result;
}
}  // namespace parser
}  // namespace storm

#endif

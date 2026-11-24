#include "GreatSpnEditorProjectParser.h"
#include <boost/algorithm/string.hpp>

#ifdef STORM_HAVE_XERCES

#include <algorithm>
#include <iostream>

#include "storm-gspn/adapters/XercesAdapter.h"

#include "storm/exceptions/NotSupportedException.h"
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

GreatSpnEditorProjectParser::GreatSpnEditorProjectParser(std::string const& constantDefinitionString)
    : manager(std::make_shared<storm::expressions::ExpressionManager>()) {
    if (constantDefinitionString != "") {
        std::vector<std::string> constDefs;
        boost::split(constDefs, constantDefinitionString, boost::is_any_of(","));
        for (auto const& pair : constDefs) {
            std::vector<std::string> keyvaluepair;
            boost::split(keyvaluepair, pair, boost::is_any_of("="));
            STORM_LOG_THROW(keyvaluepair.size() == 2, storm::exceptions::WrongFormatException,
                            "Expected a constant definition of the form N=42 but got " << pair << ".");
            constantDefinitions.emplace(keyvaluepair.at(0), keyvaluepair.at(1));
        }
    }
}

storm::gspn::GSPN* GreatSpnEditorProjectParser::parse(xercesc::DOMElement const* elementRoot) {
    if (storm::adapters::XMLtoString(elementRoot->getTagName()) == "project") {
        traverseProjectElement(elementRoot);
        return builder.buildGspn(manager, constantsSubstitution);
    } else {
        // If the top-level node is not a "pnml" or "" node, then throw an exception.
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Failed to identify the root element.\n");
    }
}

void GreatSpnEditorProjectParser::traverseProjectElement(xercesc::DOMNode const* const node) {
    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name.compare("version") == 0 || name.compare("name") == 0) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment not handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name.compare("gspn") == 0) {
            traverseGspnElement(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

void GreatSpnEditorProjectParser::traverseGspnElement(xercesc::DOMNode const* const node) {
    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name.compare("name") == 0) {
            builder.setGspnName(storm::adapters::XMLtoString(attr->getNodeValue()));
        } else if (name.compare("show-color-cmd") == 0 || name.compare("show-fluid-cmd") == 0 || name.compare("zoom") == 0) {
            // ignore node
        } else {
            // Found node or attribute which is at the moment not handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name.compare("nodes") == 0) {
            traverseNodesElement(child);
        } else if (name.compare("edges") == 0) {
            traverseEdgesElement(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

void GreatSpnEditorProjectParser::traverseNodesElement(xercesc::DOMNode const* const node) {
    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        // Found node or attribute which is at the moment not handled by this parser.
        // Notify the user and continue the parsing.
        STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
    }

    // traverse children
    // First pass: find constant definitions
    constantsSubstitution.clear();
    expressionParser = std::make_shared<storm::parser::ExpressionParser>(*manager);
    std::unordered_map<std::string, storm::expressions::Expression> identifierMapping;
    expressionParser->setIdentifierMapping(identifierMapping);
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);
        if (name.compare("constant") == 0 || name.compare("template") == 0) {
            traverseConstantOrTemplateElement(child);
        }
    }
    // Update the expression parser to make the newly created variables known to it
    expressionParser = std::make_shared<storm::parser::ExpressionParser>(*manager);
    for (auto const& var : manager->getVariables()) {
        identifierMapping.emplace(var.getName(), var.getExpression());
    }
    expressionParser->setIdentifierMapping(identifierMapping);
    // Second pass: traverse other children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name.compare("place") == 0) {
            traversePlaceElement(child);
        } else if (name.compare("transition") == 0) {
            traverseTransitionElement(child);
        } else if (name.compare("constant") == 0 || name.compare("template") == 0) {
            // Ignore constant def in second pass
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

void GreatSpnEditorProjectParser::traverseConstantOrTemplateElement(xercesc::DOMNode const* const node) {
    std::string identifier;
    storm::expressions::Type type;
    std::string valueStr = "";

    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name.compare("name") == 0) {
            identifier = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name.compare("consttype") == 0 || name.compare("type") == 0) {
            if (storm::adapters::XMLtoString(attr->getNodeValue()).compare("REAL") == 0) {
                type = manager->getRationalType();
            } else if (storm::adapters::XMLtoString(attr->getNodeValue()).compare("INTEGER") == 0) {
                type = manager->getIntegerType();
            } else {
                STORM_LOG_WARN("Unknown consttype: " << storm::adapters::XMLtoString(attr->getNodeValue()) << '\n');
            }
        } else if (name.compare("value") == 0) {
            valueStr = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if ((name == "x") || (name == "y")) {
            // Ignore
        } else {
            // Found node or attribute which is at the moment not handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }

    STORM_LOG_THROW(!manager->hasVariable(identifier), storm::exceptions::NotSupportedException,
                    "Multiple definitions of constant '" << identifier << "' were found.");
    if (valueStr == "") {
        auto constDef = constantDefinitions.find(identifier);
        STORM_LOG_THROW(constDef != constantDefinitions.end(), storm::exceptions::NotSupportedException,
                        "Constant '" << identifier << "' has no value defined.");
        valueStr = constDef->second;
    }
    storm::expressions::Variable var;
    if (type.isRationalType()) {
        var = manager->declareRationalVariable(identifier);
        expressionParser->setAcceptDoubleLiterals(true);
    } else if (type.isIntegerType()) {
        var = manager->declareIntegerVariable(identifier);
        expressionParser->setAcceptDoubleLiterals(false);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unknown type of constant" << type << ".");
    }
    constantsSubstitution.emplace(var, expressionParser->parseFromString(valueStr));

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

void GreatSpnEditorProjectParser::traverseEdgesElement(xercesc::DOMNode const* const node) {
    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        // Found node or attribute which is at the moment not handled by this parser.
        // Notify the user and continue the parsing.
        STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (name.compare("arc") == 0) {
            traverseArcElement(child);
        } else if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

bool ignorePlaceAttribute(std::string const& name) {
    // TODO we should probably not ignore x-servers but check that it is 0.5.
    if ((name == "label-x") || (name == "label-y") || (name == "x") || (name == "y")) {
        return true;
    }
    return false;
}

void GreatSpnEditorProjectParser::traversePlaceElement(xercesc::DOMNode const* const node) {
    // traverse attributes
    std::string placeName;
    uint64_t initialTokens = 0;

    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name.compare("name") == 0) {
            placeName = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name.compare("marking") == 0) {
            initialTokens = parseInt(storm::adapters::XMLtoString(attr->getNodeValue()));
        } else if (ignorePlaceAttribute(name)) {
            // ignore node
        } else {
            // Found node or attribute which is at the moment not handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
    builder.addPlace(boost::none, initialTokens, placeName);
}

bool ignoreTransitionAttribute(std::string const& name) {
    // TODO we should probably not ignore x-servers but check that it is 0.5.
    if ((name == "label-x") || (name == "label-y") || (name == "x") || (name == "y") || (name == "nservers-x") || (name == "delay-x") || (name == "delay-y") ||
        (name == "rotation")) {
        return true;
    }
    return false;
}

void GreatSpnEditorProjectParser::traverseTransitionElement(xercesc::DOMNode const* const node) {
    std::string transitionName;
    bool immediateTransition;
    double rate = 1.0;      // The default rate in GreatSPN.
    double weight = 1.0;    // The default weight in GreatSpn
    uint64_t priority = 1;  // The default priority in GreatSpn
    boost::optional<uint64_t> numServers;

    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name.compare("name") == 0) {
            transitionName = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name.compare("type") == 0) {
            if (storm::adapters::XMLtoString(attr->getNodeValue()).compare("EXP") == 0) {
                immediateTransition = false;
            } else if (storm::adapters::XMLtoString(attr->getNodeValue()).compare("IMM") == 0) {
                immediateTransition = true;
            } else {
                STORM_LOG_WARN("unknown transition type: " << storm::adapters::XMLtoString(attr->getNodeValue()));
            }
        } else if (name.compare("delay") == 0) {
            rate = parseReal(storm::adapters::XMLtoString(attr->getNodeValue()));
        } else if (name.compare("weight") == 0) {
            weight = parseReal(storm::adapters::XMLtoString(attr->getNodeValue()));
        } else if (name.compare("priority") == 0) {
            priority = parseInt(storm::adapters::XMLtoString(attr->getNodeValue()));
        } else if (name.compare("nservers") == 0) {
            std::string nservers = storm::adapters::XMLtoString(attr->getNodeValue());
            if (nservers == "Single") {
                numServers = 1;
            } else if (nservers == "Infinite") {
                // Ignore this case as we assume infinite by default (similar to GreatSpn)
            } else {
                numServers = parseInt(nservers);
            }
        } else if (ignoreTransitionAttribute(name)) {
            // ignore node
        } else {
            // Found node or attribute which is at the moment not handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }

    if (immediateTransition) {
        builder.addImmediateTransition(priority, weight, transitionName);
    } else {
        builder.addTimedTransition(0, rate, numServers, transitionName);
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

bool ignoreArcAttribute(std::string const& name) {
    if ((name == "mult-x") || (name == "mult-y") || (name == "mult-k")) {
        return true;
    }
    return false;
}

bool ignoreArcChild(std::string const& name) {
    if (name == "point") {
        return true;
    }
    return false;
}

void GreatSpnEditorProjectParser::traverseArcElement(xercesc::DOMNode const* const node) {
    std::string head = "____NOT_SET____";
    std::string tail = "____NOT_SET____";
    std::string kind = "____NOT_SET____";
    uint_fast64_t mult = 1;

    // traverse attributes
    for (uint_fast64_t i = 0; i < node->getAttributes()->getLength(); ++i) {
        auto attr = node->getAttributes()->item(i);
        auto name = storm::adapters::getName(attr);

        if (name.compare("head") == 0) {
            head = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name.compare("tail") == 0) {
            tail = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name.compare("kind") == 0 || name.compare("type") == 0) {
            kind = storm::adapters::XMLtoString(attr->getNodeValue());
        } else if (name.compare("mult") == 0) {
            mult = parseInt(storm::adapters::XMLtoString(attr->getNodeValue()));
        } else if (ignoreArcAttribute(name)) {
            // ignore node
        } else {
            // Found node or attribute which is at the moment not handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown attribute (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }

    STORM_LOG_THROW(head.compare("____NOT_SET____") != 0, storm::exceptions::WrongFormatException, "Arc must have a head.");
    STORM_LOG_THROW(tail.compare("____NOT_SET____") != 0, storm::exceptions::WrongFormatException, "Arc must have a tail.");
    STORM_LOG_THROW(kind.compare("____NOT_SET____") != 0, storm::exceptions::WrongFormatException, "Arc must have a kind.");

    if (kind.compare("INPUT") == 0) {
        builder.addInputArc(tail, head, mult);
    } else if (kind.compare("INHIBITOR") == 0) {
        builder.addInhibitionArc(tail, head, mult);
    } else if (kind.compare("OUTPUT") == 0) {
        builder.addOutputArc(tail, head, mult);
    } else {
        // TODO error!
    }

    // traverse children
    for (uint_fast64_t i = 0; i < node->getChildNodes()->getLength(); ++i) {
        auto child = node->getChildNodes()->item(i);
        auto name = storm::adapters::getName(child);

        if (isOnlyWhitespace(name)) {
            // ignore node (contains only whitespace)
        } else if (ignoreArcChild(name)) {
            // ignore
        } else {
            // Found node or attribute which is at the moment nod handled by this parser.
            // Notify the user and continue the parsing.
            STORM_LOG_WARN("unknown child (node=" + storm::adapters::XMLtoString(node->getNodeName()) + "): " + name + "\n");
        }
    }
}

int64_t GreatSpnEditorProjectParser::parseInt(std::string str) {
    expressionParser->setAcceptDoubleLiterals(false);
    auto expr = expressionParser->parseFromString(str).substitute(constantsSubstitution);
    STORM_LOG_ASSERT(!expr.containsVariables(), "Can not evaluate expression " << str << " as it contains undefined variables.");
    return expr.evaluateAsInt();
}

double GreatSpnEditorProjectParser::parseReal(std::string str) {
    expressionParser->setAcceptDoubleLiterals(true);
    auto expr = expressionParser->parseFromString(str).substitute(constantsSubstitution);
    STORM_LOG_ASSERT(!expr.containsVariables(), "Can not evaluate expression " << str << " as it contains undefined variables.");
    return expr.evaluateAsDouble();
}

}  // namespace parser
}  // namespace storm
#endif

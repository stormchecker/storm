#include "GspnParser.h"
#include "storm-config.h"
#include "storm-gspn/adapters/XercesAdapter.h"

#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

#include "GreatSpnEditorProjectParser.h"
#include "PnmlParser.h"

namespace storm {
namespace parser {

storm::gspn::GSPN* GspnParser::parse(std::string const& filename, std::string const& constantDefinitions) {
#ifdef STORM_HAVE_XERCES
    // initialize xercesc
    try {
        xercesc::XMLPlatformUtils::Initialize();
    } catch (xercesc::XMLException const&) {
        // Error occurred during the initialization process. Abort parsing since it is not possible.
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Failed to initialize xercesc.");
    }

    auto parser = new xercesc::XercesDOMParser();
    parser->setValidationScheme(xercesc::XercesDOMParser::Val_Always);
    parser->setDoNamespaces(false);
    parser->setDoSchema(false);
    parser->setLoadExternalDTD(false);
    parser->setIncludeIgnorableWhitespace(false);

    auto errHandler = (xercesc::ErrorHandler*)new xercesc::HandlerBase();
    parser->setErrorHandler(errHandler);

    // parse file
    try {
        parser->parse(filename.c_str());
    } catch (xercesc::XMLException const& toCatch) {
        auto message = xercesc::XMLString::transcode(toCatch.getMessage());
        // Error occurred while parsing the file. Abort constructing the gspn since the input file is not valid
        // or the parser run into a problem.
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, message);
        xercesc::XMLString::release(&message);
    } catch (const xercesc::DOMException& toCatch) {
        auto message = xercesc::XMLString::transcode(toCatch.msg);
        // Error occurred while parsing the file. Abort constructing the gspn since the input file is not valid
        // or the parser run into a problem.
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, message);
        xercesc::XMLString::release(&message);
    } catch (...) {
        // Error occurred while parsing the file. Abort constructing the gspn since the input file is not valid
        // or the parser run into a problem.
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Failed to parse pnml file.\n");
    }

    // build gspn by traversing the DOM object
    parser->getDocument()->normalizeDocument();
    xercesc::DOMElement* elementRoot = parser->getDocument()->getDocumentElement();

    if (storm::adapters::XMLtoString(elementRoot->getTagName()) == "pnml") {
        STORM_LOG_WARN_COND(constantDefinitions == "", "Constant definitions for pnml files are currently not supported.");
        PnmlParser p;
        return p.parse(elementRoot);
    } else if (storm::adapters::XMLtoString(elementRoot->getTagName()) == "project") {
        GreatSpnEditorProjectParser p(constantDefinitions);
        return p.parse(elementRoot);
    } else {
        // If the top-level node is not a "pnml" or "" node, then throw an exception.
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Failed to identify the root element.\n");
    }

    // clean up
    delete parser;
    delete errHandler;
    xercesc::XMLPlatformUtils::Terminate();
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is not compiled with XML support: " << filename << " can not be parsed.");
#endif
}
}  // namespace parser
}  // namespace storm

#include "storm-dft/parser/BEOrderParser.h"

#include <boost/algorithm/string.hpp>

#include "storm-dft/parser/DFTGalileoParser.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/io/file.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace parser {

template<typename ValueType>
std::vector<size_t> BEOrderParser<ValueType>::parseBEOrder(std::string const& filename, storm::dft::storage::DFT<ValueType> const& dft) {
    std::ifstream file;
    storm::io::openFile(filename, file);

    std::string line;
    size_t lineNo = 0;
    std::vector<size_t> beOrder;
    try {
        while (storm::io::getline(file, line)) {
            ++lineNo;
            boost::trim(line);
            if (line.empty()) {
                // Empty line
                continue;
            }

            // Split line into tokens w.r.t. whitespace
            boost::trim(line);
            std::vector<std::string> tokens;
            boost::split(tokens, line, boost::is_any_of(" \t"), boost::token_compress_on);

            for (auto const& token : tokens) {
                std::string name = DFTGalileoParser<ValueType>::parseName(token);
                STORM_LOG_THROW(dft.existsName(name), storm::exceptions::InvalidArgumentException, "No DFT element with name '" << name << "' exists.");
                auto element = dft.getElement(dft.getIndex(name));
                STORM_LOG_THROW(element->isBasicElement(), storm::exceptions::InvalidArgumentException, "Element '" << name << "' is not a BE.");
                beOrder.push_back(element->id());
            }
        }
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_THROW(false, storm::exceptions::FileIoException, "A parsing exception occurred in line " << lineNo << ": " << exception.what() << ".");
    }
    storm::io::closeFile(file);
    STORM_LOG_THROW(beOrder.size() == dft.nrBasicElements(), storm::exceptions::InvalidArgumentException,
                    "DFT has " << dft.nrBasicElements() << " BEs but " << beOrder.size() << " BE names where given.");
    return beOrder;
}

// Explicitly instantiate the class.
template class BEOrderParser<double>;
template class BEOrderParser<RationalFunction>;

}  // namespace parser
}  // namespace storm::dft

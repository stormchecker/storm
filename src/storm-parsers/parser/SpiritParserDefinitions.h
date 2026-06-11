#ifndef STORM_PARSER_SPIRITPARSERDEFINITIONS_H_
#define STORM_PARSER_SPIRITPARSERDEFINITIONS_H_

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-W#pragma-messages"

#if defined(__clang__) && defined(__apple_build_version__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// Include boost spirit.
#define BOOST_SPIRIT_USE_PHOENIX_V3
#define BOOST_SPIRIT_UNICODE
#include <boost/phoenix.hpp>
#include <boost/spirit/home/classic/iterator/position_iterator.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/support_line_pos_iterator.hpp>
#include <boost/typeof/typeof.hpp>

#pragma clang diagnostic pop

namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;

typedef std::string::const_iterator BaseIteratorType;
typedef boost::spirit::line_pos_iterator<BaseIteratorType> PositionIteratorType;
typedef PositionIteratorType Iterator;

namespace storm {
namespace spirit_encoding = boost::spirit::unicode;
}

typedef BOOST_TYPEOF(storm::spirit_encoding::space_type() | qi::lit("//") >> *(qi::char_ - (qi::eol | qi::eoi)) >> (qi::eol | qi::eoi)) Skipper;

#if defined(__clang__) && defined(__apple_build_version__)
#pragma clang diagnostic pop
#endif

#endif /* STORM_PARSER_SPIRITPARSERDEFINITIONS_H_ */

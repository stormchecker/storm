//==============================================================================
//
//  Copyright (c) 2015-
//  Authors:
//  * Joachim Klein <klein@tcs.inf.tu-dresden.de>
//  * David Mueller <david.mueller@tcs.inf.tu-dresden.de>
//
//------------------------------------------------------------------------------
//
//  This file is part of the cpphoafparser library,
//      http://automata.tools/hoa/cpphoafparser/
//
//  The cpphoafparser library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  The cpphoafparser library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
//
//==============================================================================

#include <queue>
#include <iostream>
#include <fstream>

#include "cpphoafparser/consumer/hoa_consumer_print.hh"
#include "cpphoafparser/consumer/hoa_consumer_null.hh"
#include "cpphoafparser/consumer/hoa_intermediate_trace.hh"
#include "cpphoafparser/consumer/hoa_intermediate_resolve_aliases.hh"
#include "cpphoafparser/parser/hoa_parser.hh"

using namespace cpphoafparser;

/** The version */
static const char* version = "0.99.2";

/** Print version to out, verbose? */
static void printVersion(std::ostream& out, bool verbose) {
  out << "cpphoafparser library - command line tool (version " << version << ")" << std::endl;
  out << " (C) Copyright 2015- Joachim Klein, David Mueller" << std::endl;
  out << " http://automata.tools/hoa/cpphoafparser/" << std::endl;
  out <<  std::endl;

  if (verbose) {
    out << "The cpphoafparser library is free software; you can redistribute it and/or" << std::endl;
    out << "modify it under the terms of the GNU Lesser General Public" << std::endl;
    out << "License as published by the Free Software Foundation; either" << std::endl;
    out << "version 2.1 of the License, or (at your option) any later version." << std::endl;
    out << std::endl;
    out << "The cpphoafparser library is distributed in the hope that it will be useful," << std::endl;
    out << "but WITHOUT ANY WARRANTY; without even the implied warranty of" << std::endl;
    out << "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU" << std::endl;
    out << "Lesser General Public License for more details." << std::endl;
  }
}

/** Print usage information, and optionally error message */
static unsigned int usage(const std::string& error = "") {

  printVersion(std::cerr, false);

  if (error != "") {
    std::cerr << "Command-line arguments error: " << error << std::endl;
    std::cerr << "Use argument 'help' to get usage information." << std::endl;
    return 2;
  }

  std::cerr << "Arguments:" << std::endl;
  std::cerr << "  command [flags]* file" << std::endl << std::endl;

  std::cerr << " Valid commands:" << std::endl;
  std::cerr << "  parse               Parse the HOAF file and check for errors" << std::endl;
  std::cerr << "  print               Parse the HOAF file, perform any specified transformations" << std::endl;
  std::cerr << "                        and print the parsed automata to standard out" << std::endl;
  std::cerr << "  version             Print the version and exit" << std::endl;
  std::cerr << "  help                Print this help screen and exit" << std::endl;
  std::cerr << std::endl;
  std::cerr << " file can be a filename or - to request reading from standard input" << std::endl;
  std::cerr << std::endl;
  std::cerr << " Valid flags:" << std::endl;
  std::cerr << "  --resolve-aliases           Resolve aliases" << std::endl;
  std::cerr << "  --no-validate               Do not perform semantic validation" << std::endl;
  std::cerr << "  --trace                     Debugging: Trace the function calls to HOAConsumer" << std::endl;
// std::cerr << "  --verbose                        Increase verbosity" << std::endl;

  return (error != "" ? 2 : 0);
}



int main(int argc, char* argv[]) {
  bool verbose = false;
  bool resolve_aliases = false;
  bool trace = false;
  bool validate = true;

  std::queue<std::string> arguments;
  for (int i=1; i<argc; i++) {
    arguments.push(argv[i]);
  }

  if (arguments.size() == 0) {
    return usage();
  }

  std::string command = arguments.front();
  arguments.pop();
  if (command == "help" || command == "--help") {
    return usage();
  }
  if (command == "version") {
    printVersion(std::cout, true);
    return 0;
  }

  while (!arguments.empty()) {
    const std::string& arg = arguments.front();
    if (arg.compare(0, 2, "--") == 0) {
      // is an argument
      arguments.pop();
      if (arg == "--resolve-aliases") {
        resolve_aliases = true;
      } else if (arg == "--trace") {
        trace = true;
      } else if (arg == "--no-validate") {
        validate = false;
      } else if (arg == "--") {
        // end of arguments
        break;
      } else {
        return usage("Unknown argument "+arg);
      }
    }
    // not an argument
    break;
  }


  if (arguments.empty()) {
    return usage("Missing filename (or - for standard input)");
  }

  if (arguments.size() > 1) {
    return usage("More than one filename, currently only supports single file");
  }

  std::string filename = arguments.front();
  arguments.pop();
  std::shared_ptr<std::ifstream> f_in;
  if (filename != "-") {
    f_in.reset(new std::ifstream(filename.c_str()));
    if (!*f_in) {
      std::cerr << "Error opening file " + filename << std::endl;
      return 1;
    }
  }
  std::istream& in = (filename == "-" ? std::cin : *f_in);

  if (verbose) {
    std::cerr << "Reading from " + (filename != "-" ? "file "+filename : "standard input") << std::endl;
  }

  HOAConsumer::ptr consumer;
  if (command == "print") {
    consumer.reset(new HOAConsumerPrint(std::cout));
  } else if (command == "parse") {
    consumer.reset(new HOAConsumerNull());
  } else {
    return usage("Unknown command: "+command);
  }

  if (resolve_aliases) {
    consumer.reset(new HOAIntermediateResolveAliases(consumer));
  }

  if (trace) {
    consumer.reset(new HOAIntermediateTrace(consumer));
  }

  try {
    HOAParser::parse(in, consumer, validate);

    if (command == "parse") {
      std::cout << "Parsing OK" << std::endl;
    }

    return 0;
  } catch (HOAParserException& e) {
    std::cerr << e.what() << std::endl;
  } catch (HOAConsumerException& e) {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
  return 1;
}



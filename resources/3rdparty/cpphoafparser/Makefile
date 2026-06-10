
# CXXFLAGS for debugging:
# CXXFLAGS=-g -O1 -Wall

# CXXFLAGS for production
CXXFLAGS=-O3 -Wall

cpphoaf : src/cpphoaf.cc include/cpphoafparser/*/*.hh
	$(CXX) $(CXXFLAGS) -I include --std=c++11 -o $@ $<


# The example parsers
basic_parser_1 : src/basic_parser_1.cc include/cpphoafparser/*/*.hh
	$(CXX) $(CXXFLAGS) -I include --std=c++11 -o $@ $<

basic_parser_2 : src/basic_parser_2.cc include/cpphoafparser/*/*.hh
	$(CXX) $(CXXFLAGS) -I include --std=c++11 -o $@ $<

.PHONY: all
all: cpphoaf basic_parser_1 basic_parser_2

#include "cpphoafparser/consumer/hoa_consumer_print.hh"
#include "cpphoafparser/parser/hoa_parser.hh"

using namespace cpphoafparser;

/** The most basic HOA parser: Read an automaton from input and print it to the output. */
int main(int argc, const char* argv[]) {
  HOAConsumer::ptr consumer(new HOAConsumerPrint(std::cout));

  try {

    HOAParser::parse(std::cin, consumer);

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}

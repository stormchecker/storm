#include "cpphoafparser/consumer/hoa_intermediate.hh"
#include "cpphoafparser/consumer/hoa_consumer_null.hh"
#include "cpphoafparser/parser/hoa_parser.hh"

using namespace cpphoafparser;

/* An HOAIntermediate that counts invocations of addState */
class CountStates : public HOAIntermediate {
public:
  typedef std::shared_ptr<CountStates> ptr;
  unsigned int count = 0;

  CountStates(HOAConsumer::ptr next) : HOAIntermediate(next) {
  }

  virtual void addState(unsigned int id,
                        std::shared_ptr<std::string> info,
                        label_expr::ptr labelExpr,
                        std::shared_ptr<int_list> accSignature) override {
    count++;
    next->addState(id, info, labelExpr, accSignature);
  }
};

/** Demonstrating the use of HOAIntermediates */
int main(int argc, const char* argv[]) {
  HOAConsumer::ptr hoaNull(new HOAConsumerNull());
  CountStates::ptr counter(new CountStates(hoaNull));

  try {

    HOAParser::parse(std::cin, counter);
    std::cout << "Number of state definitions = " << counter->count << std::endl;

  } catch (std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}

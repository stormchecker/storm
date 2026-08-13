#include "MarkovAutomatonSparseTransitionParser.h"

#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/util/cstring.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

using namespace storm::utility::cstring;

template<typename ValueType>
typename MarkovAutomatonSparseTransitionParser<ValueType>::FirstPassResult MarkovAutomatonSparseTransitionParser<ValueType>::firstPass(
    char const* buf, ExplicitModelParserOptions const& options) {
    MarkovAutomatonSparseTransitionParser::FirstPassResult result;

    bool fixDeadlocks = options.fixDeadlocks;

    // Skip the format hint if it is there.
    buf = trimWhitespaces(buf);
    if (buf[0] < '0' || buf[0] > '9') {
        buf = forwardToLineEnd(buf);
        buf = trimWhitespaces(buf);
    }

    // Now read the transitions.
    uint_fast64_t source, target = 0;
    uint_fast64_t lastsource = 0;
    bool encounteredEOF = false;
    bool stateHasMarkovianChoice = false;
    bool stateHasProbabilisticChoice = false;
    while (buf[0] != '\0' && !encounteredEOF) {
        // At the current point, the next thing to read is the source state of the next choice to come.
        source = checked_strtol(buf, &buf);

        // Check if we encountered a state index that is bigger than all previously seen ones and record it if necessary.
        if (source > result.highestStateIndex) {
            result.highestStateIndex = source;
        }

        // If we have skipped some states, we need to reserve the space for the self-loop insertion in the second pass.
        if (source > lastsource + 1) {
            if (fixDeadlocks) {
                result.numberOfNonzeroEntries += source - lastsource - 1;
                result.numberOfChoices += source - lastsource - 1;
            } else {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                "Found deadlock states (e.g. " << lastsource + 1 << ") during parsing. Please fix them or set the appropriate flag.");
            }
        } else if (source < lastsource) {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Illegal state choice order. A choice of state " << source << " appears at an illegal position.");
        }

        ++result.numberOfChoices;

        // If we have moved to the next state, we need to clear the flag that stores whether or not the source has a Markovian or probabilistic choice.
        if (source != lastsource) {
            stateHasMarkovianChoice = false;
            stateHasProbabilisticChoice = false;
        }

        // Record that the current source was the last source.
        lastsource = source;

        buf = trimWhitespaces(buf);

        // Depending on the action name, the choice is either a probabilitic one or a markovian one.
        bool isMarkovianChoice = false;
        if (buf[0] == '!' && skipWord(buf) - buf == 1) {
            isMarkovianChoice = true;
        } else {
            isMarkovianChoice = false;
        }
        buf = skipWord(buf);

        if (isMarkovianChoice) {
            STORM_LOG_THROW(!stateHasMarkovianChoice, storm::exceptions::WrongFormatException, "The state " << source << " has multiple Markovian choices.");
            if (stateHasProbabilisticChoice) {
                STORM_LOG_THROW(
                    false, storm::exceptions::WrongFormatException,
                    "The state " << source
                                 << " has a probabilistic choice preceding a Markovian choice. The Markovian choice must be the first choice listed.");
            }
            stateHasMarkovianChoice = true;
        } else {
            stateHasProbabilisticChoice = true;
        }

        // Go to the next line where the transitions start.
        buf = forwardToNextLine(buf);

        // Now that we have the source state and the information whether or not the current choice is probabilistic or Markovian, we need to read the list of
        // successors and the probabilities/rates.
        bool hasSuccessorState = false;
        bool encounteredNewDistribution = false;
        uint_fast64_t lastSuccessorState = 0;

        // At this point, we need to check whether there is an additional successor or we have reached the next choice for the same or a different state.
        do {
            buf = trimWhitespaces(buf);
            // If the end of the file was reached, we need to abort and check whether we are in a legal state.
            if (buf[0] == '\0') {
                if (!hasSuccessorState) {
                    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                    "Premature end-of-file. Expected at least one successor state for state " << source << ".");
                } else {
                    // If there was at least one successor for the current choice, this is legal and we need to move on.
                    encounteredEOF = true;
                }
            } else if (buf[0] == '*') {
                // As we have encountered a "*", we know that there is an additional successor state for the current choice.
                buf = skipWord(buf);

                // Now we need to read the successor state and check if we already saw a higher state index.
                target = checked_strtol(buf, &buf);
                if (target > result.highestStateIndex) {
                    result.highestStateIndex = target;
                }
                STORM_LOG_THROW(!hasSuccessorState || target > lastSuccessorState, storm::exceptions::WrongFormatException,
                                "Illegal transition order for source state " << source << ".");

                // And the corresponding probability/rate.
                double val = checked_strtod(buf, &buf);
                STORM_LOG_THROW(val >= 0.0, storm::exceptions::WrongFormatException,
                                "Illegal negative probability/rate value for transition from " << source << " to " << target << ": " << val << ".");
                STORM_LOG_THROW(isMarkovianChoice || val <= 1.0, storm::exceptions::WrongFormatException,
                                "Illegal probability value for transition from " << source << " to " << target << ": " << val << ".");

                // We need to record that we found at least one successor state for the current choice.
                hasSuccessorState = true;
                lastSuccessorState = target;

                // As we found a new successor, we need to increase the number of nonzero entries.
                ++result.numberOfNonzeroEntries;

                buf = forwardToNextLine(buf);
            } else {
                // If it was not a "*", we have to assume that we encountered the beginning of a new choice definition. In this case, we don't move the pointer
                // to the buffer, because we still need to read the new source state.
                encounteredNewDistribution = true;
            }
        } while (!encounteredEOF && !encounteredNewDistribution);
    }

    // If there are some states with indices that are behind the last source for which no transition was specified,
    // we need to reserve some space for introducing self-loops later.
    if (fixDeadlocks) {
        result.numberOfNonzeroEntries += result.highestStateIndex - lastsource;
        result.numberOfChoices += result.highestStateIndex - lastsource;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                        "Found deadlock states (e.g. " << lastsource + 1 << ") during parsing. Please fix them or set the appropriate flag.");
    }

    return result;
}

template<typename ValueType>
typename MarkovAutomatonSparseTransitionParser<ValueType>::Result MarkovAutomatonSparseTransitionParser<ValueType>::secondPass(
    char const* buf, FirstPassResult const& firstPassResult, ExplicitModelParserOptions const& options) {
    Result result(firstPassResult);

    bool fixDeadlocks = options.fixDeadlocks;

    // Skip the format hint if it is there.
    buf = trimWhitespaces(buf);
    if (buf[0] < '0' || buf[0] > '9') {
        buf = forwardToLineEnd(buf);
        buf = trimWhitespaces(buf);
    }

    // Now read the transitions.
    uint_fast64_t source, target = 0;
    uint_fast64_t lastsource = 0;
    bool encounteredEOF = false;
    uint_fast64_t currentChoice = 0;

    // The first choice of the first state already starts a new row group of the matrix.
    result.transitionMatrixBuilder.newRowGroup(0);

    while (buf[0] != '\0' && !encounteredEOF) {
        // At the current point, the next thing to read is the source state of the next choice to come.
        source = checked_strtol(buf, &buf);

        // If we have skipped some states, we need to insert self-loops if requested.
        if (source > lastsource + 1) {
            if (fixDeadlocks) {
                for (uint_fast64_t index = lastsource + 1; index < source; ++index) {
                    result.transitionMatrixBuilder.newRowGroup(currentChoice);
                    result.transitionMatrixBuilder.addNextValue(currentChoice, index, 1);
                    ++currentChoice;
                }
            } else {
                STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                                "Found deadlock states (e.g. " << lastsource + 1 << ") during parsing. Please fix them or set the appropriate flag.");
            }
        }

        if (source != lastsource) {
            // If we skipped to a new state we need to create a new row group for the choices of the new state.
            result.transitionMatrixBuilder.newRowGroup(currentChoice);
        }

        // Record that the current source was the last source.
        lastsource = source;

        buf = trimWhitespaces(buf);

        // Depending on the action name, the choice is either a probabilitic one or a markovian one.
        bool isMarkovianChoice = false;
        if (buf[0] == '!' && skipWord(buf) - buf == 1) {
            isMarkovianChoice = true;

            // Mark the current state as a Markovian one.
            result.markovianStates.set(source, true);
        } else {
            isMarkovianChoice = false;
        }

        // Go to the next line where the transitions start.
        buf = forwardToNextLine(buf);

        // Now that we have the source state and the information whether or not the current choice is probabilistic or Markovian, we need to read the list of
        // successors and the probabilities/rates.
        bool encounteredNewDistribution = false;

        // At this point, we need to check whether there is an additional successor or we have reached the next choice for the same or a different state.
        do {
            buf = trimWhitespaces(buf);

            // If the end of the file was reached, we need to abort and check whether we are in a legal state.
            if (buf[0] == '\0') {
                // Under the assumption that the currently open choice has at least one successor (which is given after the first run)
                // we may legally stop reading here.
                encounteredEOF = true;
            } else if (buf[0] == '*') {
                // As we have encountered a "*", we know that there is an additional successor state for the current choice.
                buf = skipWord(buf);

                // Now we need to read the successor state and check if we already saw a higher state index.
                target = checked_strtol(buf, &buf);

                // And the corresponding probability/rate.
                double val = checked_strtod(buf, &buf);

                // Record the value as well as the exit rate in case of a Markovian choice.
                result.transitionMatrixBuilder.addNextValue(currentChoice, target, val);
                if (isMarkovianChoice) {
                    result.exitRates[source] += val;
                }

                buf = forwardToNextLine(buf);
            } else {
                // If it was not a "*", we have to assume that we encountered the beginning of a new choice definition. In this case, we don't move the pointer
                // to the buffer, because we still need to read the new source state.
                encounteredNewDistribution = true;
            }
        } while (!encounteredEOF && !encounteredNewDistribution);

        ++currentChoice;
    }

    // If there are some states with indices that are behind the last source for which no transition was specified,
    // we need to insert the self-loops now. Note that we assume all these states to be Markovian.
    if (fixDeadlocks) {
        for (uint_fast64_t index = lastsource + 1; index <= firstPassResult.highestStateIndex; ++index) {
            result.markovianStates.set(index, true);
            result.exitRates[index] = storm::utility::one<ValueType>();
            result.transitionMatrixBuilder.newRowGroup(currentChoice);
            result.transitionMatrixBuilder.addNextValue(currentChoice, index, storm::utility::one<ValueType>());
            ++currentChoice;
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                        "Found deadlock states (e.g. " << lastsource + 1 << ") during parsing. Please fix them or set the appropriate flag.");
    }

    return result;
}

template<typename ValueType>
typename MarkovAutomatonSparseTransitionParser<ValueType>::Result MarkovAutomatonSparseTransitionParser<ValueType>::parseMarkovAutomatonTransitions(
    std::string const& filename, ExplicitModelParserOptions const& options) {
    // Set the locale to correctly recognize floating point numbers.
    setlocale(LC_NUMERIC, "C");

    // Open file and prepare pointer to buffer.
    MappedFile file(filename.c_str());
    char const* buf = file.getData();

    return secondPass(buf, firstPass(buf, options), options);
}

template class MarkovAutomatonSparseTransitionParser<double>;

}  // namespace parser
}  // namespace storm

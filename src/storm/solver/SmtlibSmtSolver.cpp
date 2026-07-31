#include "storm/solver/SmtlibSmtSolver.h"

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <boost/algorithm/string.hpp>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/io/file.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/Smt2SmtSolverSettings.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {

SmtlibSmtSolver::SmtlibModelReference::SmtlibModelReference(storm::expressions::ExpressionManager const& manager) : ModelReference(manager) {
    // Intentionally left empty.
}

bool SmtlibSmtSolver::SmtlibModelReference::getBooleanValue(storm::expressions::Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

int_fast64_t SmtlibSmtSolver::SmtlibModelReference::getIntegerValue(storm::expressions::Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

double SmtlibSmtSolver::SmtlibModelReference::getRationalValue(storm::expressions::Variable const&) const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

std::string SmtlibSmtSolver::SmtlibModelReference::toString() const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

SmtlibSmtSolver::SmtlibSmtSolver(storm::expressions::ExpressionManager& manager, bool useCarlExpressions)
    : SmtSolver(manager), isCommandFileOpen(false), expressionAdapter(nullptr), useCarlExpressions(useCarlExpressions) {
    processIdOfSolver = 0;
    this->expressionAdapter =
        std::unique_ptr<storm::adapters::Smt2ExpressionAdapter>(new storm::adapters::Smt2ExpressionAdapter(this->getManager(), this->useReadableVarNames));
    init();
}

SmtlibSmtSolver::~SmtlibSmtSolver() {
    writeCommand("( exit )",
                 false);  // do not wait for success because it does not matter at this point and may cause problems if the solver is not running properly

    if (processIdOfSolver != 0) {
        // Since the process has been opened successfully, it means that we have to close our fds
        close(fromSolver);
        close(toSolver);
        kill(processIdOfSolver, SIGTERM);
        waitpid(processIdOfSolver, nullptr, 0);  // make sure the process has exited
    }
}

void SmtlibSmtSolver::push() {
    expressionAdapter->increaseScope();
    writeCommand("( push 1 ) ", true);
}

void SmtlibSmtSolver::pop() {
    expressionAdapter->decreaseScope();
    writeCommand("( pop 1 ) ", true);
}

void SmtlibSmtSolver::pop(uint_fast64_t n) {
    expressionAdapter->decreaseScope(n);
    writeCommand("( pop " + std::to_string(n) + " ) ", true);
}

void SmtlibSmtSolver::reset() {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

void SmtlibSmtSolver::add(storm::expressions::Expression const&) {
    STORM_LOG_THROW(!useCarlExpressions, storm::exceptions::IllegalFunctionCallException, "This solver was initialized without allowing carl expressions.");
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

void SmtlibSmtSolver::add(storm::RationalFunction const& leftHandSide, storm::CompareRelation const& relation, storm::RationalFunction const& rightHandSide) {
    STORM_LOG_THROW(useCarlExpressions, storm::exceptions::IllegalFunctionCallException, "This solver was initialized without allowing carl expressions.");
    // if some of the occurring variables are not declared yet, we will have to.
    std::set<storm::RationalFunctionVariable> variables;
    leftHandSide.gatherVariables(variables);
    rightHandSide.gatherVariables(variables);
    std::vector<std::string> const varDeclarations = expressionAdapter->checkForUndeclaredVariables(variables);
    for (auto declaration : varDeclarations) {
        writeCommand(declaration, true);
    }
    writeCommand("( assert " + expressionAdapter->translateExpression(leftHandSide, relation, rightHandSide) + " )", true);
}

void SmtlibSmtSolver::add(const storm::RationalFunctionVariable& variable, bool value) {
    STORM_LOG_THROW((variable.type() == carl::VariableType::VT_BOOL), storm::exceptions::IllegalArgumentException,
                    "Tried to add a constraint that consists of a non-boolean variable.");
    std::set<storm::RationalFunctionVariable> variableSet;
    variableSet.insert(variable);
    std::vector<std::string> const varDeclarations = expressionAdapter->checkForUndeclaredVariables(variableSet);
    for (auto declaration : varDeclarations) {
        writeCommand(declaration, true);
    }
    std::string varName = carl::VariablePool::getInstance().getName(variable, this->useReadableVarNames);
    if (value) {
        writeCommand("( assert " + varName + " )", true);
    } else {
        writeCommand("( assert (not " + varName + ") )", true);
    }
}

SmtSolver::CheckResult SmtlibSmtSolver::check() {
    writeCommand("( check-sat )", false);

    if (processIdOfSolver != 0) {
        auto solverOutput = readSolverOutput();
        STORM_LOG_THROW(
            solverOutput.size() == 1, storm::exceptions::UnexpectedException,
            "Expected a single line of output after smt2 command ( check-sat ). Got " + std::to_string(solverOutput.size()) + " lines of output instead.");
        solverOutput[0].erase(std::remove_if(solverOutput[0].begin(), solverOutput[0].end(), ::isspace), solverOutput[0].end());  // remove spaces
        if (solverOutput[0] == "sat")
            return SmtSolver::CheckResult::Sat;
        if (solverOutput[0] == "unsat")
            return SmtSolver::CheckResult::Unsat;
        if (solverOutput[0] == "unknown")
            return SmtSolver::CheckResult::Unknown;
        // if we reach this point, something unexpected happened. Lets return unknown and print some debug output
        STORM_LOG_DEBUG("unexpected solver output: " << solverOutput[0] << ". Returning result 'unknown'");
        return SmtSolver::CheckResult::Unknown;
    } else {
        STORM_LOG_WARN("No SMT-LIBv2 Solver Command specified, which means that no actual SMT solving is done... Assume that the result is \"unknown\"");
        return SmtSolver::CheckResult::Unknown;
    }
}

SmtSolver::CheckResult SmtlibSmtSolver::checkWithAssumptions(std::set<storm::expressions::Expression> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

SmtSolver::CheckResult SmtlibSmtSolver::checkWithAssumptions(std::initializer_list<storm::expressions::Expression> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Functionality not (yet) implemented.");
}

void SmtlibSmtSolver::init() {
    if (storm::settings::getModule<storm::settings::modules::Smt2SmtSolverSettings>().isSolverCommandSet()) {
        signal(SIGPIPE, SIG_IGN);
        this->needsRestart = false;

        // for executing the solver we will need the path to the executable file as well as the arguments as a char* array
        // where the first argument is the path to the executable file and the last is NULL
        const std::string cmdString = storm::settings::getModule<storm::settings::modules::Smt2SmtSolverSettings>().getSolverCommand();
        std::vector<std::string> solverCommandVec;
        boost::split(solverCommandVec, cmdString, boost::is_any_of("\t "));
        char** solverArgs = new char*[solverCommandVec.size() + 1];
        std::string solverPath = solverCommandVec[0].substr(0, cmdString.rfind('/') + 1);
        solverArgs[0] = const_cast<char*>(solverPath.c_str());
        for (uint_fast64_t argumentIndex = 1; argumentIndex < solverCommandVec.size(); ++argumentIndex) {
            solverArgs[argumentIndex] = const_cast<char*>(solverCommandVec[argumentIndex].c_str());
        }
        solverArgs[solverCommandVec.size()] = NULL;

        // get the pipes started
        int pipeIn[2];
        int pipeOut[2];
        const int READ = 0;
        const int WRITE = 1;
        STORM_LOG_THROW(pipe(pipeIn) == 0 && pipe(pipeOut) == 0, storm::exceptions::UnexpectedException, "Could not open pipe to new process.");

        // now start the child process, i.e., the solver
        pid_t pid = fork();
        STORM_LOG_THROW(pid >= 0, storm::exceptions::UnexpectedException, "Could not start new process for the smt solver.");
        if (pid == 0) {
            // Child process
            // duplicate the fd so that standard input and output will be send to our pipes
            dup2(pipeIn[READ], STDIN_FILENO);
            dup2(pipeOut[WRITE], STDOUT_FILENO);
            dup2(pipeOut[WRITE], STDERR_FILENO);
            // we can now close everything since our child process will use std in and out to address the pipes
            close(pipeIn[READ]);
            close(pipeIn[WRITE]);
            close(pipeOut[READ]);
            close(pipeOut[WRITE]);

            execv(solverCommandVec[0].c_str(), solverArgs);  //"-smt2 -in"
            // if we reach this point, execl was not successful
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Could not execute the solver correctly.");
        }
        // Parent Process
        toSolver = pipeIn[WRITE];
        fromSolver = pipeOut[READ];
        close(pipeOut[WRITE]);
        close(pipeIn[READ]);
        processIdOfSolver = pid;

    } else {
        STORM_LOG_WARN("No SMT-LIBv2 Solver Command specified, which means that no actual SMT solving can be done");
    }

    if (storm::settings::getModule<storm::settings::modules::Smt2SmtSolverSettings>().isExportSmtLibScriptSet()) {
        STORM_LOG_DEBUG("The SMT-LIBv2 commands are exportet to the given file");
        storm::io::openFile(storm::settings::getModule<storm::settings::modules::Smt2SmtSolverSettings>().getExportSmtLibScriptPath(), commandFile);
        isCommandFileOpen = true;
        // TODO also close file
    }

    // some initial commands
    writeCommand("( set-option :print-success true )", true);
    writeCommand("( set-logic QF_NRA )", true);
    // writeCommand("( get-info :name )");
}

bool SmtlibSmtSolver::isNeedsRestart() const {
    return this->needsRestart;
}

void SmtlibSmtSolver::writeCommand(std::string smt2Command, bool expectSuccess) {
    if (isCommandFileOpen) {
        commandFile << smt2Command << '\n';
    }

    if (processIdOfSolver != 0) {
        if (write(toSolver, (smt2Command + "\n").c_str(), smt2Command.length() + 1) < 0) {
            STORM_LOG_DEBUG("Was not able to write " << smt2Command << "to the solver.");
        }
        if (expectSuccess) {
            auto output = readSolverOutput();
            STORM_LOG_THROW(output.size() == 1, storm::exceptions::UnexpectedException,
                            "Expected a single success response after smt2 command " + smt2Command + ". Got " + std::to_string(output.size()) +
                                " lines of output instead.");
            output[0].erase(std::remove_if(output[0].begin(), output[0].end(), ::isspace), output[0].end());
            STORM_LOG_THROW(output[0] == "success", storm::exceptions::UnexpectedException,
                            "Expected <<success>> response after smt2 command " + smt2Command + ". Got <<" + output[0] + ">> instead.");
        }
    }
}

std::vector<std::string> SmtlibSmtSolver::readSolverOutput(bool waitForOutput) {
    if (processIdOfSolver == 0) {
        STORM_LOG_DEBUG("failed to read solver output as the solver is not running");
        return std::vector<std::string>();
    }
    int bytesReadable;
    if (waitForOutput) {
        bytesReadable = 1;                                         // just assume that there are bytes readable
    } else if (ioctl(fromSolver, FIONREAD, &bytesReadable) < 0) {  // actually obtain the readable bytes
        STORM_LOG_ERROR("Could not check if the solver has output");
        return std::vector<std::string>();
    }
    std::string solverOutput = "";
    const ssize_t MAX_CHUNK_SIZE = 256;
    char chunk[MAX_CHUNK_SIZE];
    while (bytesReadable > 0) {
        ssize_t chunkSize = read(fromSolver, chunk, MAX_CHUNK_SIZE);
        STORM_LOG_THROW(chunkSize >= 0, storm::exceptions::UnexpectedException, "Failed to read solver output.");
        solverOutput += std::string(chunk, chunkSize);
        if (ioctl(fromSolver, FIONREAD, &bytesReadable) < 0) {  // obtain the new amount of readable bytes
            STORM_LOG_ERROR("Could not check if the solver has output");
            return std::vector<std::string>();
        }
        if (bytesReadable == 0 && solverOutput.back() != '\n') {
            STORM_LOG_DEBUG(
                "Solver Output '"
                << solverOutput
                << "' did not end with newline symbol (\\n). Since we assume that this should be the case, we will wait for more output from the solver");
            bytesReadable = 1;  // we expect more output!
        }
        if (bytesReadable > 0) {
            pid_t w = waitpid(processIdOfSolver, nullptr, WNOHANG);
            if (w != 0) {
                STORM_LOG_WARN_COND(w > 0, "Error when checking whether the solver is still running. Will assume that solver has terminated.");
                STORM_LOG_WARN("The solver exited unexpectedly when reading output: " << solverOutput);
                solverOutput += "terminated";
                this->needsRestart = true;
                this->processIdOfSolver = 0;
                bytesReadable = 0;
            }
        }
    }
    checkForErrorMessage(solverOutput);
    std::vector<std::string> solverOutputAsVector;
    if (solverOutput.length() > 0) {
        solverOutput = solverOutput.substr(0, solverOutput.length() - 1);
        boost::split(solverOutputAsVector, solverOutput, boost::is_any_of("\n"));
        // note: this is a little bit unsafe as \n can be contained within a solver response
    }
    return solverOutputAsVector;
}

void SmtlibSmtSolver::checkForErrorMessage(const std::string message) {
    size_t errorOccurrance = message.find("error");
    // do not throw an exception for timeout or memout errors
    if (message.find("timeout") != std::string::npos) {
        STORM_LOG_INFO("SMT solver answered: '" << message << "' and I am interpreting this as timeout ");
        this->needsRestart = true;
        this->processIdOfSolver = 0;
    } else if (message.find("memory") != std::string::npos) {
        STORM_LOG_INFO("SMT solver answered: '" << message << "' and I am interpreting this as out of memory ");
        this->needsRestart = true;
        this->processIdOfSolver = 0;
    } else if (errorOccurrance != std::string::npos) {
        this->needsRestart = true;
        std::string errorMsg = "An error was detected while checking the solver output. ";
        STORM_LOG_DEBUG("Detected an error message in the solver response:\n" + message);
        size_t firstQuoteSign = message.find('\"', errorOccurrance);
        if (firstQuoteSign != std::string::npos && message.find("\\\"", firstQuoteSign - 1) != firstQuoteSign - 1) {
            size_t secondQuoteSign = message.find('\"', firstQuoteSign + 1);
            while (secondQuoteSign != std::string::npos && message.find("\\\"", secondQuoteSign - 1) == secondQuoteSign - 1) {
                secondQuoteSign = message.find('\"', secondQuoteSign + 1);
            }
            if (secondQuoteSign != std::string::npos) {
                errorMsg += "The error message was: <<" + message.substr(errorOccurrance, secondQuoteSign - errorOccurrance + 1) + ">>.";
                STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, errorMsg);
                return;
            }
        }
        errorMsg += "The error message could not be parsed correctly. Snippet:\n" + message.substr(errorOccurrance, 200);
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, errorMsg);
    }
}
}  // namespace solver
}  // namespace storm

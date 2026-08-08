#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace solver {
std::string toString(MinMaxMethod m) {
    switch (m) {
        case MinMaxMethod::PolicyIteration:
            return "policy";
        case MinMaxMethod::ValueIteration:
            return "value";
        case MinMaxMethod::LinearProgramming:
            return "linearprogramming";
        case MinMaxMethod::Topological:
            return "topological";
        case MinMaxMethod::RationalSearch:
            return "ratsearch";
        case MinMaxMethod::IntervalIteration:
            return "intervaliteration";
        case MinMaxMethod::SoundValueIteration:
            return "soundvalueiteration";
        case MinMaxMethod::OptimisticValueIteration:
            return "optimisticvalueiteration";
        case MinMaxMethod::GuessingValueIteration:
            return "guessingvalueiteration";
        case MinMaxMethod::ViToPi:
            return "vi-to-pi";
        case MinMaxMethod::ViToLp:
            return "vi-to-lp";
        case MinMaxMethod::Acyclic:
            return "vi-to-pi";
    }
    return "invalid";
}

std::string toString(MultiplierType t) {
    switch (t) {
        case MultiplierType::Native:
            return "Native";
        case MultiplierType::ViOperator:
            return "ViOperator";
    }
    return "invalid";
}

std::string toString(GameMethod m) {
    switch (m) {
        case GameMethod::ValueIteration:
            return "valueiteration";
        case GameMethod::PolicyIteration:
            return "PolicyIteration";
    }
    return "invalid";
}

std::string toString(LraMethod m) {
    switch (m) {
        case LraMethod::LinearProgramming:
            return "linear-programming";
        case LraMethod::ValueIteration:
            return "value-iteration";
        case LraMethod::LraDistributionEquations:
            return "lra-distribution-equations";
        case LraMethod::GainBiasEquations:
            return "gain-bias-equations";
    }
    return "invalid";
}

std::string toString(MaBoundedReachabilityMethod m) {
    switch (m) {
        case MaBoundedReachabilityMethod::Imca:
            return "imca";
        case MaBoundedReachabilityMethod::UnifPlus:
            return "unifplus";
    }
    return "invalid";
}

std::string toString(LpSolverType t) {
    switch (t) {
        case LpSolverType::Gurobi:
            return "Gurobi";
        case LpSolverType::Glpk:
            return "Glpk";
        case LpSolverType::Z3:
            return "Z3";
        case LpSolverType::Soplex:
            return "Soplex";
        case LpSolverType::Highs:
            return "Highs";
    }
    return "invalid";
}

std::string toString(EquationSolverType t) {
    switch (t) {
        case EquationSolverType::Native:
            return "Native";
        case EquationSolverType::Gmmxx:
            return "Gmmxx";
        case EquationSolverType::Eigen:
            return "Eigen";
        case EquationSolverType::Elimination:
            return "Elimination";
        case EquationSolverType::Topological:
            return "Topological";
        case EquationSolverType::Acyclic:
            return "Acyclic";
    }
    return "invalid";
}

std::string toString(SmtSolverType t) {
    switch (t) {
        case SmtSolverType::Z3:
            return "Z3";
        case SmtSolverType::Mathsat:
            return "Mathsat";
    }
    return "invalid";
}

std::string toString(NativeLinearEquationSolverMethod t) {
    switch (t) {
        case NativeLinearEquationSolverMethod::Jacobi:
            return "Jacobi";
        case NativeLinearEquationSolverMethod::GaussSeidel:
            return "GaussSeidel";
        case NativeLinearEquationSolverMethod::SOR:
            return "SOR";
        case NativeLinearEquationSolverMethod::WalkerChae:
            return "WalkerChae";
        case NativeLinearEquationSolverMethod::Power:
            return "Power";
        case NativeLinearEquationSolverMethod::SoundValueIteration:
            return "SoundValueIteration";
        case NativeLinearEquationSolverMethod::OptimisticValueIteration:
            return "optimisticvalueiteration";
        case NativeLinearEquationSolverMethod::GuessingValueIteration:
            return "guessingvalueiteration";
        case NativeLinearEquationSolverMethod::IntervalIteration:
            return "IntervalIteration";
        case NativeLinearEquationSolverMethod::RationalSearch:
            return "RationalSearch";
    }
    return "invalid";
}

std::string toString(GmmxxLinearEquationSolverMethod t) {
    switch (t) {
        case GmmxxLinearEquationSolverMethod::Bicgstab:
            return "BiCGSTAB";
        case GmmxxLinearEquationSolverMethod::Qmr:
            return "QMR";
        case GmmxxLinearEquationSolverMethod::Gmres:
            return "GMRES";
    }
    return "invalid";
}

std::string toString(GmmxxLinearEquationSolverPreconditioner t) {
    switch (t) {
        case GmmxxLinearEquationSolverPreconditioner::Diagonal:
            return "diagonal";
        case GmmxxLinearEquationSolverPreconditioner::Ilu:
            return "ilu";
        case GmmxxLinearEquationSolverPreconditioner::None:
            return "none";
    }
    return "invalid";
}

std::string toString(EigenLinearEquationSolverMethod t) {
    switch (t) {
        case EigenLinearEquationSolverMethod::SparseLU:
            return "SparseLU";
        case EigenLinearEquationSolverMethod::Bicgstab:
            return "BiCGSTAB";
        case EigenLinearEquationSolverMethod::DGmres:
            return "DGMRES";
        case EigenLinearEquationSolverMethod::Gmres:
            return "GMRES";
    }
    return "invalid";
}

std::string toString(EigenLinearEquationSolverPreconditioner t) {
    switch (t) {
        case EigenLinearEquationSolverPreconditioner::Diagonal:
            return "diagonal";
        case EigenLinearEquationSolverPreconditioner::Ilu:
            return "ilu";
        case EigenLinearEquationSolverPreconditioner::None:
            return "none";
    }
    return "invalid";
}
}  // namespace solver
}  // namespace storm

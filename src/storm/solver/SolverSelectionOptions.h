#pragma once

#include "storm/utility/ExtendSettingEnumWithSelectionField.h"

namespace storm {
namespace solver {
ExtendEnumsWithSelectionField(MinMaxMethod, ValueIteration, PolicyIteration, LinearProgramming, Topological, RationalSearch, IntervalIteration,
                              SoundValueIteration, OptimisticValueIteration, GuessingValueIteration, ViToPi, ViToLp, Acyclic)
    ExtendEnumsWithSelectionField(MultiplierType, Native, ViOperator) ExtendEnumsWithSelectionField(GameMethod, PolicyIteration, ValueIteration)
        ExtendEnumsWithSelectionField(LraMethod, LinearProgramming, ValueIteration, GainBiasEquations, LraDistributionEquations)
            ExtendEnumsWithSelectionField(MaBoundedReachabilityMethod, Imca, UnifPlus)

                ExtendEnumsWithSelectionField(LpSolverType, Gurobi, Glpk, Z3, Soplex, Highs)
                    ExtendEnumsWithSelectionField(EquationSolverType, Native, Gmmxx, Eigen, Elimination, Topological, Acyclic)
                        ExtendEnumsWithSelectionField(SmtSolverType, Z3, Mathsat)

                            ExtendEnumsWithSelectionField(NativeLinearEquationSolverMethod, Jacobi, GaussSeidel, SOR, WalkerChae, Power, SoundValueIteration,
                                                          OptimisticValueIteration, GuessingValueIteration, IntervalIteration, RationalSearch)
                                ExtendEnumsWithSelectionField(GmmxxLinearEquationSolverMethod, Bicgstab, Qmr, Gmres)
                                    ExtendEnumsWithSelectionField(GmmxxLinearEquationSolverPreconditioner, Ilu, Diagonal, None)
                                        ExtendEnumsWithSelectionField(EigenLinearEquationSolverMethod, SparseLU, Bicgstab, DGmres, Gmres)
                                            ExtendEnumsWithSelectionField(EigenLinearEquationSolverPreconditioner, Ilu, Diagonal, None)
}
}  // namespace storm

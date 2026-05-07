Changelog
==============

This changelog lists only the most important changes. Smaller (bug)fixes as well as non-mature features are not part of the changelog.
The releases of major and minor versions contain an overview of changes since the last major/minor update.


Version 1.13.x
--------------

### Version 1.13.0 (2026/05)
- Moved repository to stormchecker/storm.
- Import/export of explicit models in (binary) [UMB](https://pmc-tools.github.io/umb/spec) format.
- Support for exact intervals and bounded model checking of intervals.
- Support for monitor verification.
- Extended support for conditional model checking.
- Added scheduler to `ExplicitQualitativeCheckResult`.
- Adaption to changes of CMake options in carl-storm.
- Revised installation paths.
- Updated dependencies: carl-storm, ExprTk, Spot.
- Renamed directories: storm-cudd and cpphoafparser.
- Bug fixes:
    * in Unif+ for Markov automata with non-Markovian initial states
    * dangling reference to tmp object
    * assert in observation trace unfolder
    * warning in `ConditionalHelper`
    * error for IDTMC checking from CLI
    * in includes
    * compiler warnings
- Developer: Added support for creating Debian packages in CMake and via CI workflow.
- Developer: minor fixes in CI workflows.


Version 1.12.x
--------------

### Version 1.12.0 (2026/03)
- Support for verifying IDTMCs.
- Extended DRN file format:
    * Use `--io:digits` option to control precision of floats.
    * Reading from and writing to compressed DRN files (use `.drn.gz` / `.drn.xz` file extensions and/or the new `--compression` option) (requires libarchive).
    * Added `@value_type` to determine the type of the parsed values.
    * Support for parsing of DRN interval models.
- Multi-objective model checking:
    * Multi-objective model checking in `--sound`mode handles approximation errors correctly.
    * Added `--multiobjective:approxtradeoff` setting to control Pareto curve approximation heuristic.
    * Extended support for multi-objective properties: distinguish between trade-off and lexicographic properties, introduce `multilex(..)` syntax for Prism-style properties.
- `storm-pars`:
    * Added BigStep transformation.
    * Added generalized parameter lifting.
    * Support for not graph-preserving regions and for not well-defined regions.
    * Region verification API uses `RefinementOptions`.
- Support build without DD support.
- Added state-valuation transformer.
- Preparation for using probability matrix in CTMC bisimulation.
- Revised check for probabilistic transition matrix.
- ConstantsComparator no longer depends on global state.
- New (optional) dependency: libarchive which supports directly reading/writing from an archive.
- Updated dependencies: carl-storm, Eigen, ExprTk, gmm, googletest, Gurobi, parallel-hashmap, Spot.
- Bug fixes in returning rate parameters and requiring MultiObjectiveSettings.
- Developer: modernized CTMC helpers.
- Developer: general code clean up such as revising includes, assertions and CMake defines.
- Developer: CI treats compiler warnings as errors.


Version 1.11.x
--------------

### Version 1.11.1 (2025/10)
- Bug fixes in conditional probabilities computation with the (recent) bisection method.
- Sound value iteration diagnostics in debug mode are more robust.
- Improved support for ARM.
- Support for musl libc.
- Code quality fixes (reduced warnings)

### Version 1.11.0 (2025/09)
- Completely restructured CMake build process:
    * Storm can now be properly installed.
    * Carl-storm is now automatically fetched from a predefined repository and tag. Use CMake option `-DFETCHCONTENT_SOURCE_DIR_CARL=<path_to_carl>` to set a local path.
- Improved support for interval models, especially parsing support for Prism models with intervals and CLI support for interval models.
- Support for discounted properties in DTMCs and MDPs.
- Added guessing value iteration.
- Support exporting schedulers for multi-objective properties.
- Revised implementation of conditional probabilities for MDPs.
- Added building of choice labels for Jani.
- Support time limits for the Gurobi LP Solver.
- Various fixes in DRN parser, BitVector, (negative) expected rewards, parsing of PDEP and for compiler warnings.
- Updated dependencies: carl-storm, gmm, googletest, l3pp, spot.
- Removed support for Intel TBB.
- Developer: Replaced `GmmxxMultiplier` by `ViOperatorMultiplier`.
- Developer: Added CI workflow for new releases.


Version 1.10.x
--------------

### Version 1.10.0 (2025/05)
- Improved `LpMinMaxLinearEquationSolver`, set relevant values in topological solvers.
- Speed up of MEC decomposition by adjusting loops.
- Minor improvements for steady-state properties.
- Improved access to state valuations.
- Improved parsing: detect and handle integer overflows in expression parsing, and handle irrational probability expressions.
- Limit Sylvan threads to 1 on Apple Silicon.
- Bug fixes in computation of cumulative rewards, transient variables in JANI, SMGs with non-synchronizing but labeled actions, in `SparseCbAchievabilityQuery` and `PcaaWeightVectorChecker`.
- Updated shipped dependencies: Eigen, ExprTk, Spot.
- `storm-dft`: Set BDD variable ordering for SFT from file.
- `storm-dft`: Warning on potential modeling issues.
- `storm-dft`: Bug fixes in DFT representant computation and failable elements.
- `storm-pars`: Bug fixes in monotonicity computation.
- `storm-pomdp`: Added cut-off memory node information.
- `storm-pomdp`: Extended handling of terminal states.
- `storm-pomdp`: Improved output.
- Developer: Require at least CMake version 3.22.
- Developer: Revised includes.
- Developer: GLPK is optional.
- Developer: `BitVector` conforms to `std::ranges::range` concept.
- Developer: Improved CI tests and added builds on macOS.


Version 1.9.x
-------------

### Version 1.9.0 (2024/08)
- Improved expected visiting times (EVTs) and steady state distribution computations.
- Support for interval-based models.
- Robust VI.
- Significantly improved compilation times.
- Support for logarithm expressions in PRISM and JANI.
- Support for sin and cos operators, and PI and Euler constants in JANI parser.
- Extraction of schedulers for minimal expected total rewards.
- More efficient MEC and SCC decompositions.
- Revised LP encoding for multi-objective verification under simple strategies.
- Added CLI option `--permute` to re-order the states after building.
- Added CLI option `--build:state limit <number>` to limit the number of explored states.
- Print all linked libraries when using `--version`.
- Removed support for HyPro and Cuda.
- Moved gamebased-ar to own library.
- Various bug fixes.
- `storm-conv`: Removed option `--stdout`.
- `storm-dft`: Fixes and improvements for DFT symmetries and DFT simulation.
- `storm-pars`: Completely reworked the command-line interface (and partially the C++ API).
- `storm-pars`: "Time travelling" optimization.
- `storm-pgcl`: Removed the library.
- Developer: Require at least CMake version 3.15.
- Developer: Moved `storm-config.h.in` into `src` directory.
- Developer: Use various Dockerfiles in CI.
- Developer: Revised includes and use pre-compiled headers.
- Developer: Fixed various compiler warnings.


Version 1.8.x
-------------

### Version 1.8.1 (2023/06)
- Workaround for issue with Boost >= 1.81

### Version 1.8.0 (2023/05)
- Revised implementation of value iteration algorithms and its variants, fixing a bug in the optimistic value iteration heuristic.
- Experimental support for compiling on Apple Silicon
- Added SoPlex as a possible LP solver
- Upgraded shipped version of sylvan
- Upgraded repo / version for carl (for polynomials), requires [carl-storm](https://github.com/stormchecker/carl-storm) in at least version 14.23.
- Removed support for just-in-time compilation (JIT). If the JIT engine is needed, use Storm version 1.7.0.
- `storm-dft`: better modularization: improved algorithm for finding independent modules and revised the DFT analysis via modularization.
- `storm-dft`: added checks whether a given DFT is well-formed and conventional.
- `storm-pomdp`: streamlined implementation for quantitative analysis
- `storm-pomdp`: added clipping for POMDP under-approximation
- `storm-pomdp`: added API for interactive exploration of belief MDPs
- Developer: Introduced forward declarations (in their own headers), in particular for `storm::RationalNumber`, `storm::RationalFunction`, and `storm::json`
- Developer: LpSolver interface now supports RawMode (to avoid overhead of `storm::expression`) and indicator constraints


Version 1.7.x
-------------

### Version 1.7.0 (2022/07)
- Fixed a bug in LP-based MDP model checking.
- DRN Parser is now more robust, e.g., it does no longer depend on tabs.
- PRISM Parser: Modulo with negative numbers is now consistent with Prism.
- Added lexicographic multi-objective model checking. Use `--lex` in the command line interface when specifying a `multi(...)` property.
- Fix handling duplicate entries in the sparse matrix builder.
- Added support for step-bounded until formulas in LTL.
- Added Dockerfile.
- API: Applying a fully defined deterministic memoryless scheduler to an MDP yields a DTMC.
- `storm-dft`: Use dedicated namespace `storm::dft`.
- `storm-dft`: Added support (parsing, export, BDD analysis) for additional BE failure distributions (Erlang, log-normal, Weibull, constant probability).
- `storm-dft`: Added instantiator for parametric DFT.
- Developer: Storm is now built in C++17 mode.
- Developer: Added support for automatic code formatting for `storm-dft`.

Version 1.6.x
-------------

### Version 1.6.4 (2022/01)
- Added support for model checking LTL properties in the sparse (and dd-to-sparse) engine. Requires building with Spot or an external LTL to deterministic automaton converter (using option `--ltl2datool`).
- Added cmake options `STORM_USE_SPOT_SYSTEM` and `STORM_USE_SPOT_SHIPPED` to facilitate building Storm with [Spot](https://spot.lrde.epita.fr/).
- Improved parsing of formulas in PRISM-style syntax.
- Added export of schedulers that use memory (in particular optimizing schedulers for LTL properties)
- Added support for PRISM models that use unbounded integer variables.
- Added support for nested arrays in JANI.
- Added `--location-elimination` that can be applied to Jani DTMC models to reduce the size of the resulting Markov models, see [here](https://arxiv.org/abs/2011.00983).
- Added an export of check results to JSON. Use `--exportresult` in the command line interface.
- Added `--exportbuilt` option that exports the built model in various formats. Deprecates `--io:exportexplicit`, `--io:exportdd` and `--io:exportdot`
- Added export of built model in .json. which can be used to debug and explore the model.
- Added computation of steady state probabilities for DTMC/CTMC in the sparse engine. Use `--steadystate` in the command line interface.
- Added computation of the expected number of times each state in a DTMC/CTMC is visited (sparse engine). Use `--expvisittimes` in the command line interface.
- Implemented parsing and model building of Stochastic multiplayer games (SMGs) in the PRISM language. No model checking implemented (yet).
- API: Simulation of prism-models 
- API: Model-builder takes a callback function to prevent extension of particular actions, prism-to-explicit mapping can be exported
- API: Export of dice-formatted expressions
- Prism-language/explicit builder: Allow action names in commands writing to global variables if these (clearly) do not conflict with assignments of synchronizing commands.
- Prism-language: n-ary predicates are supported (e.g., ExactlyOneOf)
- Added support for continuous integration with GitHub Actions.
- `storm-pars`: Exploit monotonicity for computing extremal values and parameter space partitioning.
- `storm-dft`: Support for analysis of static fault trees via BDDs (Flag `--bdd`). In particular, efficient computation of multiple time bounds was added and support for several importance measures (Argument `--importance`).
- `storm-dft`: Computation of minimal cut sets for static fault trees (Flag `--mcs`).
- `storm-dft`: Improved modularisation for DFT by exploiting SFT analysis via BDDs.
- `storm-dft`: Fixed don't care propagation for shared SPAREs which resulted in wrong results.
- Developer: Added support for automatic code formatting and corresponding CI workflow.

### Version 1.6.3 (2020/11)
- Added support for multi-objective model checking of long-run average objectives including mixtures with other kinds of objectives.
- Added support for generating optimal schedulers for globally formulae.
- Simulator supports exact arithmetic.
- Added switch `--no-simplify` to disable simplification of PRISM programs (which sometimes costs a bit of time on extremely large inputs).
- Fixed issues with JANI inputs concerning .
    * transient variable expressions in properties,
    * constants in properties, and
    * integer variables with either only an upper or only a lower bound.
- `storm-pomdp`: States can be labeled with values for observable predicates.
- `storm-pomdp`: (Only API) Track state estimates.
- `storm-pomdp`: (Only API) Reduce computation of state estimates to computation on unrolled MDP.

### Version 1.6.2 (2020/09)
- Prism program simplification improved.
- Revamped implementation of long-run-average algorithms, including scheduler export for LRA properties on Markov automata.
- Support for step-bounded properties of the form ... [F[x,y] ... ] for DTMCs and MDPs (sparse engine).
- Renamed portfolio engine to automatic 
- `storm-dft`: Fix for relevant events when using symmetry reduction.
- `storm-pomdp`: Fix for --transformsimple and --transformbinary when used with until formulae.
- `storm-pomdp`: POMDPs can be parametric as well.

### Version 1.6.0 (2020/06)
- Changed default Dd library from `cudd` to `sylvan`. The Dd library can be changed back to `cudd` using the command line switch `--ddlib`.
- Scheduler export: Properly handle models with end components. Added export in `.json` format.
- CMake: Search for Gurobi prefers new versions.
- CMake: We no longer ship xerces-c. If xerces-c is not found on the system, storm-gspn will not be able to parse xml-based GSPN formats.
- CMake: Added option `STORM_LOAD_QVBS` to automatically download the quantitative verification benchmark set.
- Eigen library: The source code of Eigen is no longer included but downloaded from an external repository instead. Incremented Eigen version to 3.3.7 which fixes a compilation issue with recent Xcode versions.
- Tests: Enabled tests for permissive schedulers.
- `storm-counterexamples`: fix when computing multiple counterexamples in debug mode.
- `storm-dft`: Renamed setting `--show-dft-stats` to `dft-statistics` and added approximation information to statistics.
- `storm-pomdp`: Implemented approximation algorithms that explore (a discretization of) the belief MDP, allowing to compute safe lower- and upper bounds for a given property.
- `storm-pomdp`: Implemented almost-sure reachability computations: graph-based, one-shot SAT-based, and iterative SAT-based.
- `storm-pomdp`: Various changes such that transformation to pMCs is now again supported (and improved).
- Fixed several compiler warnings.


Version 1.5.x
-------------

### Version 1.5.1 (2020/03)
- Jani models are now parsed using exact arithmetic.

### Version 1.5.0 (2020/03)
- Added portfolio engine which picks a good engine (among other settings) based on features of the symbolic input.
- Abort of Storm (via timeout or CTRL+C for example) is now gracefully handled. After an abort signal the program waits some seconds to output the result computed so far and terminates afterwards. A second signal immediately terminates the program.
- Setting `--engine dd-to-sparse --bisimulation` now triggers extracting the sparse bisimulation quotient.
- JIT model building is now invoked via `--engine jit` (instead of `--jit`).
- DRN: support import of choice labelling.
- Added option `--build:buildchoiceorig` to build a model (PRISM or JANI) with choice origins (which are exported with, e.g. `--exportscheduler`).
- Implemented optimistic value iteration for sound computations and set it as new default in `--sound` mode.
- Time bounded properties for Markov automata are now computed with relative precision. Use `--absolute` for the previous behavior.
- Apply the maximum progress assumption while building a Markov automaton with one of the symbolic engines.
- Added option `--build:nomaxprog` to disable applying the maximum progress assumption during model building (for Markov Automata).
- Added hybrid engine for Markov Automata.
- Improved performance of the Unif+ algorithm (used for time-bounded properties on Markov Automata).
- Various performance improvements for model building with the sparse engine.
- `storm-dft`: Symmetry reduction is now enabled by default and can be disabled via `--nosymmetryreduction`.
- `storm-pomdp`: Only accept POMDPs that are canonical.
- `storm-pomdp`: Prism language extended with observable expressions.
- `storm-pomdp`: Various fixes that prevented usage.
- Several bug fixes.


Version 1.4.x
-------------

### Version 1.4.1 (2019/12)
- Implemented long run average (LRA) computation for DTMCs/CTMCs via value iteration and via gain/bias equations.
- Added several LRA related settings in a new settings module. Note that `--minmax:lramethod` has been replaced by `--lra:nondetmethod`.

### Version 1.4.0 (2019/11)
- Added support for multi-dimensional quantile queries.
- Added support for multi-objective model checking under pure (deterministic) schedulers with bounded memory using `--purescheds`.
- Allow to quickly check a benchmark from the [Quantitative Verification Benchmark Set](http://qcomp.org/benchmarks/) using the `--qvbs` option.
- Added script `resources/examples/download_qvbs.sh` to download the QVBS.
- If an option is unknown, Storm now suggests similar option names.
- Flagged several options as 'advanced' to clean up the `--help`-message. Use `--help all` to display a complete list of options.
- Support for parsing of exact time bounds for properties, e.g., `P=? [F=27 "goal"]`.
- Export of optimal schedulers when checking MDPs with the sparse engine (experimental). Use `--exportscheduler <filename>`.
- PRISM language: Support for the new `round` operator.
- PRISM language: Improved error messages of the parser.
- JANI: Allow bounded types for constants.
- JANI: Support for non-trivial reward accumulations.
- JANI: Fixed support for reward expressions over non-transient variables.
- DRN: Added support for exact parsing and action-based rewards.
- DRN: Support for placeholder variables which allows to parse recurring rational functions only once.
- Fixed sparse bisimulation of MDPs (which failed if all non-absorbing states in the quotient are initial).
- Support for export of MTBDDs from Storm.
- Support for k-shortest path counterexamples (arguments `-cex --cextype shortestpath`)
- New settings module `transformation` for Markov chain transformations. Use `--help transformation` to get a list of available transformations.
- Support for eliminating chains of Non-Markovian states in MAs via `--eliminate-chains`.
- Export to dot format allows for maximal line width in states (argument `--dot-maxwidth <width>`)
- `storm-conv` can now apply transformations on a prism file.
- `storm-pars`: Enabled building, bisimulation and analysis of symbolic models.
- `storm-dft`: Support partial-order for state space generation.
- `storm-dft`: Compute lower and upper bounds for number of BE failures via SMT.
- `storm-dft`: Allow to set relevant events which are not set to Don't Care.
- `storm-dft`: Support for constant failed BEs. Use flag `--uniquefailedbe` to create a unique constant failed BE.
- `storm-dft`: Support for probabilistic BEs via PDEPs.
- Fixed linking with Mathsat on macOS.
- Fixed linking with IntelTBB for GCC.
- Fixed compilation for macOS Mojave and higher.
- Several bug fixes.


Version 1.3.x
-------------

### Version 1.3.0 (2018/12)
- Slightly improved scheduler extraction
- Environments are now part of the c++ API
- Heavily extended JANI support, in particular:
	 * arrays, functions, state-exit-rewards (all engines)
	 * indexed assignments, complex reward expressions (sparse engine)
	 * several jani-related bug fixes
- New binary `storm-conv` that handles conversions between model files
- New binary `storm-pomdp` that handles the translation of POMDPs to pMCs.
- Maximal progress assumption is now applied while building Markov Automata (sparse engine).
- Improved Unif+ implementation for Markov Automata, significantly reduced memory consumption.
- Added support for expected time properties for discrete time models
- Bug fix in the parser for DRN (MDPs and MAs might have been affected).
- `storm-gspn`: Improved .pnpro parser
- `storm-gspn`: Added support for single/infinite/k-server semantics for GSPNs given in the .pnpro format
- `storm-gspn`: Added option to set a global capacity for all places
- `storm-gspn`: Added option to include a set of standard properties when converting GSPNs to jani
- `storm-pars`: Added possibility to compute the extremal value within a given region using parameter lifting
- `storm-dft`: DFT translation to GSPN supports Don't Care propagation
- `storm-dft`: Support DFT analysis via transformation from DFT to GSPN to JANI
- `storm-dft`: Added SMT encoding for DFTs
- `storm-dft`: Improved Galileo and JSON parser
- Several bug fixes
- Storm uses the `master14` branch of carl from now on

### Comparison with Version 1.2.0 (details see below)
- Heavily extended JANI-support
- New binary `storm-conv` that handles conversion between model files
- New binary `storm-pomdp` that handles the translation of POMDPs to pMCs.
- `storm-gspn` improved
- Sound value iteration


Version 1.2.x
-------------

### Version 1.2.3 (2018/07)
- Fix in version parsing

### Version 1.2.2 (2018/07)
- Sound value iteration (SVI) for DTMCs and MDPs
- Topological solver for linear equation systems and MinMax equation systems (enabled by default)
- Added support for expected total rewards in the sparse engine
- By default, iteration-based solvers are no longer aborted after a given number of steps.
- Improved export for jani models
- A fix in parsing jani properties
- Several extensions to high-level counterexamples
- `storm-parsers` extracted to reduce linking time
- `storm-counterexamples` extracted to reduce linking time
- `storm-dft`: improvements in Galileo parser
- `storm-dft`: test cases for DFT analysis
- Improved Storm installation
- Several bug fixes

### Version 1.2.1 (2018/02)
- Multidimensional reward bounded reachability properties for DTMCs.
- `storm-dft`: transformation of DFTs to GSPNs
- Several bug fixes

### Version 1.2.0 (2017/12)
- C++ api changes: Building model takes `BuilderOptions` instead of extended list of Booleans, does not depend on settings anymore.
- `storm-cli-utilities` now contains cli related stuff, instead of `storm-lib`
- Symbolic (MT/BDD) bisimulation
- Fixed issue related to variable names that can not be used in Exprtk.
- DRN parser improved
- LP-based MDP model checking
- Sound (interval) value iteration
- Support for Multi-objective multidimensional reward bounded reachability properties for MDPs.
- RationalSearch method to solve equation systems exactly
- WalkerChae method for solving linear equation systems with guaranteed convergence
- Performance improvements for sparse model building
- Performance improvements for conditional properties on MDPs
- Automatically convert MA without probabilistic states into CTMC
- Fixed implementation of Fox and Glynn's algorithm
- `storm-pars`: support for welldefinedness constraints in MDPs.
- `storm-dft`: split DFT settings into IO settings and fault tree settings
- `storm-dft`: removed obsolete explicit model builder for DFTs
- Features for developers:
	* Solvers can now expose requirements
	* unbounded reachability and reachability rewards now correctly respect solver requirements
	* Environment variables (such as the solver precisions) can now be handled more flexible
	* changes to Matrix-Vector operation interfaces, in particular fixed some issues with the use Intel TBB


Version 1.1.x
-------------

### Version 1.1.0 (2017/8)
- Support for long-run average rewards on MDPs and Markov automata using a value-iteration based approach.
- Storm can now check MDPs and Markov Automata (i.e. MinMax equation systems) via Linear Programming.
- Parametric model checking is now handled in a separated library/executable called `storm-pars`.
- Wellformedness constraints on PMCs:
    * include constraints from rewards
    * are in smtlib2
    * fixed
    * computation of only constraints without doing model checking is now supported
- Fix for nested formulae
- JANI: Explicit engine supports custom model compositions.
- Support for parsing/building models given in the explicit input format of IMCA.
- Storm now overwrites files if asked to write files to a specific location.
- Changes in build process to accommodate for changes in carl. Also, more robust against issues with carl.
- `USE_POPCNT` removed in favor of `FORCE_POPCNT`. The popcnt instruction is used if available due to `march=native`, unless portable is set.
  Then, using `FORCE_POPCNT` enables the use of the SSE 4.2 instruction


Version 1.0.x
-------------

### Version 1.0.1 (2017/4)
- Multi-objective model checking support now fully included
- Several improvements in parameter lifting
- Several improvements in JANI parsing
- Properties can contain model variables
- Support for rational numbers/functions in decision diagrams via sylvan
- Elimination-based solvers (exact solution) for models stored as decision diagrams
- Export of version and configuration to cmake
- Improved building process

### Version 1.0.0 (2017/3)
Start of this changelog

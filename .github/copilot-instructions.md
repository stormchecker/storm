# Copilot Instructions for Storm

## Repository Overview

Storm is a **modern probabilistic model checker** written in C++20. It checks properties of probabilistic systems (Markov chains, Markov decision processes, etc.) against PCTL/LTL/CSL-style specifications. The project is developed by researchers at TU/e, Radboud University, and RWTH Aachen University.

- Website: https://www.stormchecker.org/
- Documentation: https://stormchecker.github.io/storm-doc/
- Version: see `CMakeLists.txt` top-level `project()` call

---

## Repository Layout

```
CMakeLists.txt          # Top-level CMake (C++20, min CMake 3.25)
src/
  storm/                # Core library → libstorm
  storm-cli/            # CLI binary → storm
  storm-cli-utilities/
  storm-conv/           # Model conversion library → libstorm-conv
  storm-conv-cli/
  storm-counterexamples/
  storm-dft/            # Dynamic Fault Trees → libstorm-dft
  storm-dft-cli/
  storm-gamebased-ar/   # Game-based abstraction refinement
  storm-gspn/           # Generalised Stochastic Petri Nets
  storm-gspn-cli/
  storm-pars/           # Parametric models → libstorm-pars
  storm-pars-cli/
  storm-parsers/        # Model/property parsers (PRISM, JANI, …)
  storm-permissive/     # Permissive schedulers
  storm-pomdp/          # POMDPs → libstorm-pomdp
  storm-pomdp-cli/
  storm-version-info/
  test/                 # GTest test suite; mirrors the library structure
    storm/
    storm-dft/
    storm-pars/
    storm-pomdp/
    …
resources/cmake/        # CMake find_modules and macros
doc/                    # Developer documentation
```

Each library `src/storm-xyz` has:
- A `CMakeLists.txt` that produces `libstorm-xyz`
- An `api/` subdirectory with the stable public API
- A matching test directory `src/test/storm-xyz/`

---

## Building

Storm uses **CMake** out-of-source builds. All dependencies must be pre-installed (see Dockerfile for the exact set). There is no one-command dev setup in the repo itself — the CI uses a pre-built Docker image `stormchecker/storm-dependencies:latest`.

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DSTORM_DEVELOPER=ON
make -j$(nproc)        # Build everything
make storm             # Build only the core library
make storm-pars        # Build a specific sub-library
make test              # Run all tests via CTest
make format            # Apply clang-format to all src/
```

Important CMake options:
| Option | Default | Meaning |
|--------|---------|---------|
| `STORM_DEVELOPER` | OFF | Enable extra warnings and assertions |
| `STORM_WARNING_AS_ERROR` | OFF | Treat warnings as errors (used in CI) |
| `STORM_USE_CLN_EA` | OFF | Use CLN instead of GMP for exact arithmetic |
| `STORM_USE_CLN_RF` | ON  | Use CLN for rational functions |
| `STORM_BUILD_TESTS` | ON | Build test binaries |
| `STORM_DISABLE_<DEP>` | OFF | Disable optional dependencies (CUDD, GLPK, Z3, …) |

Individual test binaries are in `build/bin/test-storm`, `build/bin/test-storm-pars`, etc. Run a single test with `./bin/test-storm --gtest_filter='TestSuite.TestName'`.

---

## Coding Conventions

### Language
- C++20. Heavy use of templates — the main template parameter is `ValueType` (commonly `double`, `storm::RationalNumber`, `storm::RationalFunction`).
- No `std::cout`. Use the logging macros below.
- Use `'\n'` instead of `std::endl` (avoids unnecessary flushes).

### File Structure
- Every header starts with `#pragma once`.
- Include order (clang-format sorts within groups):
  ```cpp
  #include "storm/this_file.h"   // Only in .cpp files (corresponding header first)
  
  #include <external_library>    // System/third-party headers
  
  #include "storm/other/header.h" // Project headers
  ```
- Test files additionally start with:
  ```cpp
  #include "storm-config.h"
  #include "test/storm_gtest.h"
  ```

### Logging & Output Macros (from `storm/utility/macros.h`)
| Macro | Use |
|-------|-----|
| `STORM_LOG_DEBUG(msg)` | Debug-level log |
| `STORM_LOG_INFO(msg)` | Info-level log |
| `STORM_LOG_WARN(msg)` | Warning log |
| `STORM_LOG_ERROR(msg)` | Error log |
| `STORM_LOG_THROW(cond, exc, msg)` | Throw exception with message if `!cond` |
| `STORM_LOG_ASSERT(cond, msg)` | Assert with message |
| `STORM_PRINT(msg)` | Print to user output (no log) |
| `STORM_PRINT_AND_LOG(msg)` | Print to user output and log |

### Documentation
Doxygen-style comments for public APIs:
```cpp
/*!
 * Brief description.
 * @param paramName Description.
 * @return Description.
 */
```

### Formatting
The project uses **clang-format 20** with the config in `.clang-format` (based on Google style with modifications). Always run `make format` or the equivalent `clang-format` command before committing. The CI (`formatcheck.yml`) runs on every push/PR and will fail if formatting is wrong.

A convenience CI job (`formatapply.yml`) can apply formatting automatically — trigger it manually from GitHub Actions.

---

## CI / Continuous Integration

Workflows are in `.github/workflows/`:

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `buildtest.yml` | PR, daily, manual | Full build+test matrix (Debug/Release, GMP/CLN combos, with/without optional deps) |
| `formatcheck.yml` | push, PR, manual | Checks `src/` with clang-format 20 |
| `formatapply.yml` | manual | Auto-applies formatting and commits |
| `doxygen.yml` | push to master | Publishes API docs |
| `release.yml` | tag push | Publishes GitHub releases |
| `release_docker.yml` | manual | Builds and pushes Docker images |

The `buildtest.yml` runs several configurations including:
- GMP exact / GMP rational functions (all deps, Debug + Release)
- CLN exact / GMP rational functions
- CLN exact / CLN rational functions
- GMP exact / CLN rational functions (no optional deps — tests minimal build)
- A sanitizer build (`-DSTORM_COMPILE_WITH_ALL_SANITIZERS=ON`)

**To fix a CI failure:** Check the failed job in the Actions tab; the most common causes are formatting errors (`make format` locally) or test failures from an incorrect template instantiation.

---

## Key Patterns & Architecture Notes

### ValueType Template Pattern
The entire model-checking pipeline is templated on `ValueType`. When adding new functionality, replicate existing explicit instantiations at the bottom of `.cpp` files:
```cpp
template class MyClass<double>;
template class MyClass<storm::RationalNumber>;
template class MyClass<storm::RationalFunction>;
```

### Exception Handling
Use `STORM_LOG_THROW` rather than throwing exceptions directly:
```cpp
STORM_LOG_THROW(condition, storm::exceptions::InvalidArgumentException, "Descriptive message.");
```
Exception types live in `src/storm/exceptions/`.

### Model Access Pattern
Models are accessed via `src/storm/api/` — prefer the API over internal classes when writing new features that integrate at a higher level.

### JANI / PRISM Parsers
Parsers are in `src/storm-parsers/`. The main entry points are `storm::api::parseProgram()` (PRISM) and `storm::api::parseJaniModel()` (JANI).

---

## Known Issues / Workarounds

- **Shallow clones**: CI checks out the repo without full history. If you need history (e.g., for `git log`), run `git fetch --unshallow origin`.
- **Optional dependencies absent**: The minimal build (no CUDD, GLPK, Z3, etc.) is tested in CI. Guard new code that requires optional deps with the appropriate `STORM_HAVE_*` preprocessor flag defined in `storm-config.h.in`.
- **Long compile times**: The codebase is heavily templated. Using `STORM_COMPILE_WITH_PCH=ON` (default) and `STORM_COMPILE_WITH_CCACHE=ON` (default, requires ccache installed) speeds up incremental builds significantly.
- **CLN vs GMP**: Rational arithmetic via CLN and GMP behaves differently in edge cases. New numerical code should be tested in both configurations (`STORM_USE_CLN_EA` on/off, `STORM_USE_CLN_RF` on/off).

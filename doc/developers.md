# Developer information

The following contains some general guidelines for developers.


## Structure
- Storm consists of the core library `lib/libstorm` resulting from the source code in `src/storm`.
- Several additional libraries `lib/libstorm-xyz` provide additional features (parametric models, POMPD, DFT, etc.) and are built from the corresponding code in `src/storm-xyz`.
- For each library, a corresponding binary `/bin/storm-xyz` is built from the source in `src/storm-xyz-cli`.
- Functionality is accompanied by tests (`src/test`) whenever possible.
  The complete test suite can be executed by `make test` and individual tests can be executed via the corresponding binaries `bin/test-xyz`.
- Storm is heavily templated.
  In particular, it features the template argument `ValueType` representing the underlying number type.
  The most commonly used types are `double`, `storm::RationalNumber` and `storm::RationalFunction`.

The key source directories are:

| Directory | Output | Description |
|-----------|--------|-------------|
| `src/storm` | `libstorm` | Core model-checking library |
| `src/storm-cli` | `storm` binary | Main command-line interface |
| `src/storm-dft` | `libstorm-dft` | Dynamic Fault Trees |
| `src/storm-pars` | `libstorm-pars` | Parametric models |
| `src/storm-pomdp` | `libstorm-pomdp` | POMDPs |
| `src/storm-gspn` | `libstorm-gspn` | Generalised Stochastic Petri Nets |
| `src/storm-conv` | `libstorm-conv` | Model conversion |
| `src/storm-parsers` | (part of libstorm) | PRISM and JANI parsers |
| `src/storm-gamebased-ar` | | Game-based abstraction refinement |
| `src/storm-permissive` | | Permissive schedulers |
| `src/test` | | GTest test suite mirroring the library structure |

Each library's public API lives in its `api/` subdirectory (e.g., `src/storm/api/`, `src/storm-pars/api/`).


## Building

Storm uses CMake out-of-source builds. All dependencies must be pre-installed (see [the website](https://www.stormchecker.org/documentation/obtain-storm/dependencies.html) for the exact set). The CI uses the pre-built Docker image `stormchecker/storm-dependencies:latest` which comes with most dependencies already installed.

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DSTORM_DEVELOPER=ON
make -j$(nproc)        # Build everything
make storm             # Build only the core library
make storm-pars        # Build a specific sub-library
make test              # Run all tests via CTest
make format            # Apply clang-format to all src/
```

Individual test binaries are placed in `build/bin/`. Run a specific test with:
```bash
./bin/test-storm --gtest_filter='TestSuite.TestName'
```

Important CMake options:

| Option | Default | Meaning |
|--------|---------|---------|
| `STORM_DEVELOPER` | OFF | Enable extra warnings and assertions |
| `STORM_WARNING_AS_ERROR` | OFF | Treat warnings as errors (used in CI) |
| `STORM_USE_CLN_EA` | OFF | Use CLN instead of GMP for exact arithmetic |
| `STORM_USE_CLN_RF` | ON | Use CLN for rational functions |
| `STORM_BUILD_TESTS` | ON | Build test binaries |
| `STORM_DISABLE_<DEP>` | OFF | Disable optional dependencies (CUDD, GLPK, Z3, …) |


## Coding conventions

### Templates and `ValueType`
New functionality should typically be templated on `ValueType`. Add explicit instantiations at the bottom of each `.cpp` file:
```cpp
template class MyClass<double>;
template class MyClass<storm::RationalNumber>;
template class MyClass<storm::RationalFunction>;
```
New numerical code must be tested under both CLN and GMP configurations (`STORM_USE_CLN_EA` on/off, `STORM_USE_CLN_RF` on/off) since their arithmetic differs in edge cases.

### Exception handling
Use `STORM_LOG_THROW` rather than throwing exceptions directly:
```cpp
STORM_LOG_THROW(condition, storm::exceptions::InvalidArgumentException, "Descriptive message.");
```
This throws an `InvalidArgumentException` if `condition` is violated.
Exception types live in `src/storm/exceptions/`.

### Optional dependencies
Code that requires optional dependencies (CUDD, GLPK, Z3, …) must be guarded with the corresponding preprocessor flag from `storm-config.h.in`:
```cpp
#ifdef STORM_HAVE_Z3
  // z3-specific code
#endif
```

### Formatting
- Code should be formatted according to the given rules set by clang-format.
  Proper formatting can be ensured by executing `make format`.
  For more information see [PR#175](https://github.com/stormchecker/storm/pull/175).

### Documentation
- We use Doxygen for documentation, see [storm-doc](https://stormchecker.github.io/storm-doc/).
  Code blocks should be documented with:
  ```
  /*!
   * ... 
   * @param ... 
   * @return ... 
   */
  ```

### Includes
- Header files should start with
  ```
  #pragma once
  ```
- Includes should follow the following order:
  ```
  #include "storm/header.h"  // If cpp file
  
  #include <external_library1>
  #include <external_library2>
  ...
  
  #include "storm/additional/headerfile1.h"
  #include "storm/additional/headerfile2.h"
  ...
  ```
  There should only be empty lines between the header file and the external libraries and between the external libraries and the additional header files.
  Clang-format will then automatically sort the includes in alphabetical order.
- Tests follow the same order as before but typically start by including two helper files:
  ```
  #include "storm-config.h"
  #include "test/storm_gtest.h"
  
  #include ...
  ```

### Output
- We provide custom macros for output and logging.
  The use of `std::cout` should be avoided and instead, macros such as `STORM_LOG_DEBUG`, `STORM_LOG_INFO` or `STORM_PRINT_AND_LOG` should be used.
- For line breaks, we use `'\n'` instead of `std::endl` to avoid unnecessary flushing.
  See [PR 178](https://github.com/stormchecker/storm/pull/178) for details.


## CI / Continuous Integration

Workflows are in `.github/workflows/`:

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `buildtest.yml` | PR, daily, manual | Full build+test matrix (Debug/Release, GMP/CLN combos, with/without optional deps) |
| `formatcheck.yml` | push, PR, manual | Checks `src/` with clang-format 20 |
| `formatapply.yml` | manual | Auto-applies formatting and commits |
| `doxygen.yml` | push to master | Publishes API docs |
| `release.yml` | tag push | Publishes GitHub releases |

`buildtest.yml` covers multiple configurations including GMP/CLN combinations and a sanitizer build. A CI failure is most commonly caused by a formatting error (fix with `make format`) or a missing template instantiation for one of the `ValueType` variants.


## Contributing
- Check that all tests run successfully: `make test`.
- Check that the code is properly formatted: `make format`.
  There is also a CI job which can provide automated code formatting.
- New code should be submitted by opening a [pull request](https://github.com/stormchecker/storm/pulls).
  Our continuous integration automatically checks that the code in the PR is properly formatted and all tests run successfully. 

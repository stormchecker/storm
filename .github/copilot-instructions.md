# Copilot Instructions for Storm

Storm is a modern probabilistic model checker written in C++20.
Before making changes, read [doc/developers.md](../doc/developers.md) for the full developer reference (structure, build system, coding conventions, CI, contributing guidelines).

The sections below cover agent-specific context that complements that document.

---

## Quick orientation

- Website: <https://www.stormchecker.org/>
- API docs: <https://stormchecker.github.io/storm-doc/>
- Version: top-level `CMakeLists.txt` → `project()` call
- CI Docker image: `stormchecker/storm-dependencies:latest` (contains all pre-built deps)

---

## Agent workflow tips

### Making changes
1. Identify the affected library under `src/storm-xyz/` and its test directory `src/test/storm-xyz/`.
2. Build only what you changed: `make storm-xyz` (faster than `make -j$(nproc)`).
3. Run the relevant test binary: `./bin/test-storm-xyz --gtest_filter='Suite.Test'`.
4. Run `make format` before committing — `formatcheck.yml` runs on every push and will fail otherwise.

### Logging macros (never use `std::cout`)

| Macro | Use |
|-------|-----|
| `STORM_LOG_DEBUG(msg)` | Debug-level log |
| `STORM_LOG_INFO(msg)` | Info-level log |
| `STORM_LOG_WARN(msg)` | Warning log |
| `STORM_LOG_ERROR(msg)` | Error log |
| `STORM_LOG_THROW(cond, exc, msg)` | Throw `exc` with `msg` if `!cond` |
| `STORM_LOG_ASSERT(cond, msg)` | Assertion with message |
| `STORM_PRINT(msg)` | Print to user output (no log) |
| `STORM_PRINT_AND_LOG(msg)` | Print to user output and log |

All macros are defined in `src/storm/utility/macros.h`.

### Investigating CI failures
Use the GitHub Actions MCP tools to read job logs directly. Common causes:
- **Format failure** (`formatcheck.yml`): run `make format` locally and push.
- **Build/test failure** (`buildtest.yml`): usually a missing template instantiation for one `ValueType` variant, or a preprocessor-guarded optional dependency being used unconditionally.

---

## Known pitfalls

- **Shallow clones**: the agent's repo clone may lack full history. If `git log` or diff against `main` is needed, run `git fetch --unshallow origin` first.
- **Template instantiations**: when adding a class or function templated on `ValueType`, always add explicit instantiations (`double`, `storm::RationalNumber`, `storm::RationalFunction`) at the bottom of the `.cpp` file. Omitting one will cause a linker error only in the affected CI configuration.
- **Optional-dependency guards**: wrap code that needs CUDD, GLPK, Z3, etc. in `#ifdef STORM_HAVE_<DEP>`. The flag is defined in `storm-config.h.in`. The "no optional deps" CI configuration will otherwise fail to compile.
- **CLN vs GMP edge cases**: rational arithmetic behaves differently between CLN and GMP. New numerical code should be verified in both configurations.

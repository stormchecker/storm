# Contributing to Storm

We welcome contributions to Storm.
General information on the development of Storm, such as the code structure, building, coding conventions and the CI, is available in the [information for developers](doc/developers.md).

The following steps describe how to contribute changes to Storm:

1. Check locally that all tests run successfully: `make test`.
2. Format the code locally: `make format`.
   There is also a CI job which can provide automated code formatting.
3. New code should be submitted by opening a [pull request](https://github.com/stormchecker/storm/pulls).
4. Our continuous integration (see [CI](doc/developers.md#ci--continuous-integration)) then automatically checks that
   - the code in the PR is properly formatted,
   - clang-tidy does not report any issues,
   - the quick configurations build Storm and all tests pass.
5. If the PR is ready to be reviewed, add the label "ready for review".
   This label automatically triggers the CI checks with a wider set of configurations, including the stormpy compatibility check.
6. If the changes break stormpy (as reported by the CI), increase the development counter `N` of the Storm version in `CMakeLists.txt` (see [Versioning](doc/developers.md#versioning)) and later adapt stormpy accordingly.
7. Run (if possible) the Copilot review on the PR.
8. Wait for a review by one of the Storm maintainers and revise the PR according to potential comments.
9. If the PR review was positive and the CI was successful, the PR can be merged by *squash and merge*.

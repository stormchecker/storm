# Third-party resources
Storm makes extensive use of third-party resources.
An overview of the dependencies is available on the [website](https://www.stormchecker.org/documentation/obtain-storm/dependencies.html).

The resources are defined in `resources/3rdparty/CMakeLists.txt`.
If resources are shipped with Storm they are located in a separate directory in `resources/3rdparty`.

## Adding new third-party resources
Before adding new third-party libraries make sure that the licensing allows to use the library.
If the library will be shipped with Storm, make sure that the license allows this.
Libraries should be maintained and future support should be guaranteed.

To add a new library, support must be added to CMake, in Storm itself, and various other places need updating.
See the list below for the steps.

### Adding CMake support
- Extend `resources/3rdparty/CMakeLists.txt` such that it supports the new resource. Take a look at existing libraries to get an idea how to make these changes.
  Some libraries are optional. Then a new CMake variable `STORM_HAVE_X` should be added which allows to check the support for the library.
- Extend `CMakeLists.txt` if needed.
  Some libraries have a CMake `option(STORM_DISABLE_X)` to disable the library.
  Also add `export_option(STORM_HAVE_X)`.
- Update the CMake configs in `src/storm-config.h.in`, `resources/cmake/stormConfig.cmake.in` and `resources/cmake/stormConfig.cmake.install.in`.
- If a Debian/Ubuntu package exists, add it to `CPACK_DEBIAN_PACKAGE_DEPENDS` in `resources/cmake/stormCPackConfig.cmake`.

### Implement support in Storm
- Adapt the settings to support the new library. This will typically be `CoreSettings`.
- Add a dedicated class in Storm:
  - solvers are defined in `storm/solver`
  - dd libraries are defined in `storm/storage/dd`
  - some libraries are defined in `storm/adapters`
- Add output to `printVersion()` in `src/storm-cli-utilities/print.cpp` which prints information about the library.
- Add tests for the new library.

### Extend documentation, CI, Docker, etc.
- Mention the new library in the `CHANGELOG.md`.
- Update the `Dockerfile` and also Dockerfiles in `.github/workflows`.
- Adapt the CI tests in `.github/workflows`.
- Update the documentation below on how to update the library.
- Extend the Docker images in [docker-storm](https://github.com/stormchecker/docker-storm/) to ship with the new library.
- Update the information on the [dependencies](https://www.stormchecker.org/documentation/obtain-storm/dependencies.html) on the [Storm-website](https://github.com/stormchecker/storm-website/).
- If packages exist, add support for [Archlinux](https://aur.archlinux.org/packages/stormchecker), [Homebrew](https://github.com/moves-rwth/homebrew-storm).

## Update third-party resources
New versions of third-party resources should be supported by Storm.
In the following, we list the steps for specific resources.

### Eigen
In Eigen, we have adapted `SparseLU` to work with scalar types that do not default construct from a double (like CLN numbers) or that do not have an operator< or std::abs

To update the Eigen version, just change the corresponding commit hash in `$STORM_DIR/resources/3rdparty/CMakeLists.txt`.
Check whether the patch located at `$STORM_DIR/resources/3rdparty/patches/eigen501.patch` can be applied without issues (in particular check for changes in `Eigen/src/SparseLU/`).

The commit hash is forwarded to carl-storm via the `CARL_EIGEN_GIT_TAG` variable to ensure that carl-storm and Storm check out the same Eigen version.
It might be reasonable to update the default value of `CARL_EIGEN_GIT_TAG` in carl-storm as well.

In case a new patch needs to be created follow these steps:

1. Clone `https://gitlab.com/libeigen/eigen.git` somewhere and checkout the previously shipped version
2. Checkout a new branch e.g., `git branch storm-patch; git checkout storm-patch`
3. Apply the old patch via `git apply $STORM_DIR/resources/3rdparty/patches/eigen501.patch`. At this point, `git diff` shows you all the changes we apply to Eigen
4. Make a commit, e.g., `git commit -a -m "Storm patch"`
5. Merge or rebase the new Eigen tag, branch or commit, e.g., `git rebase <new_commit_hash>`
6. Resolve issues, make changes, and commit them
7. Create a new patch file via `git format-patch <new_commit_hash> --stdout > eigenXYZ.patch`, where `<new_commit_hash>` is the tag, branch or commit from step 5 and `XYZ` reflects the new Eigen version
8. Add the patch to `resources/3rdparty/patches/` and change the `resources/3rdparty/CMakeLists.txt` file accordingly.

### ExprTk
To update ExprTk, download the latest version from the [website](https://www.partow.net/programming/exprtk/index.html#downloads) and copy the file `exprtk.hpp` to `$STORM_DIR/resources/3rdparty/exprtk/`.

### GMM
To update GMM, change the corresponding version in `$STORM_DIR/resources/3rdparty/CMakeLists.txt`.
Check whether the patch located at `$STORM_DIR/resources/3rdparty/patches/gmm55.patch` still applies to the new version; update or rename it (and adjust the patch path in the CMake file) if it does not.

### googletest / gtest
To update gtest, bump the `GTEST_VERSION` number.

We add some extra code to gtest located in `$STORM_DIR/src/test/storm_gtest.h`. Note that our code might not be compatible with future versions of gtest.


### GTL
Download the new sources from [GitHub](https://github.com/greg7mdp/gtl) and put the files from `include/gtl` to `$STORM_DIR/resources/3rdparty/gtl/gtl`.
All other directories are not needed.


### Gurobi
To support newer versions of Gurobi, adapt `$STORM_DIR/resources/cmake/find_modules/FindGUROBI.cmake` with the new version numbers.
Also update the error message in the Gurobi section of `$STORM_DIR/resources/3rdparty/CMakeLists.txt`


### l3pp
The l3pp version can be bumped by updating the corresponding `GIT_TAG`.


### nlohmann/json for Modern C++
The currently shipped version is forked from the [official GitHub](https://github.com/nlohmann/json) commit `6eab7a2b187b10b2494e39c1961750bfd1bda500`.
We extended the library towards rational numbers, see [here](../resources/3rdparty/modernjson/README_STORM.md).
To update, you can follow these steps:

1. Check out the above commit in a separate repository
2. Copy the contents of `$STORM_DIR/resources/3rdparty/modernjson/include` to the repository you just checked out and commit to a fresh branch
3. The diff for that commit shows you the exact modifications we made.
4. Merge the JSON version you want to update to into your branch.
5. Resolve potential conflicts and review what has changed, in particular if it affects handling of floating point numbers.
6. When this is all done, copy the contents back into the storm directory. Make sure to not apply any unnecessary code formatting to keep the diff smallish.
7. *Update the commit hash mentioned in this document*


### Spot
To update (shipped version of Spot), just change the `SPOT_SHIPPED_VERSION` in `$STORM_DIR/resources/3rdparty/include_spot.cmake`.


### Sylvan & Lace
The currently shipped version of [sylvan](https://github.com/trolando/sylvan) is based on commit b08d75eb56461178a614188ad94cbab211adc253 (tag 1.7.1) but with parts of the build system updated to a newer version.
Our Sylvan version also includes [lace](https://github.com/trolando/lace) which is currently based on commit 3577d983e8c40e276fb8070dc4c12c68940e2f2c (tag 1.4.0)
To update, you can follow these steps:

1. Check out the above commit in a separate repository
2. Copy the contents of `$STORM_DIR/resources/3rdparty/sylvan` to the repository you just checked out and commit to a fresh branch
3. The diff for that commit shows you the modifications we made to the sylvan source code. 
   We also include *lace* (`lace.c` and `lace.h`) which is a dependency of sylvan that is normally downloaded by sylvan's build scripts via cmake `FetchContent`.
   As this requires cmake 3.14 and complicates the overall build process, we decided to ship the files `lace.c` and `lace.h` directly (see Storm commit 095e78a897b01db20df95bca89e746229e6a85c8)
4. Merge the `master` of sylvan (or whatever version you want to update to) into your branch
5. Resolve potential conflicts and review what has changed, in particular if it affects the API that Storm uses
6. Update the shipped `lace` sources.
7. When this is all done, copy the contents back into the storm directory
8. *Update commit hashes of sylvan and lace mentioned in this document* 

# Dockerfile for Storm
######################
# The Docker image can be built by executing:
# docker build -t yourusername/storm .
# A different base image can be set from the commandline with:
# --build-arg BASE_IMAGE=<new_base_image>
#
# Two types of images can be built via --build-arg image_type=<type>:
# - dev (default): contains the Storm sources, the build directory and the installed Storm.
# - release: only contains the installed Storm (in /usr/local).
# With --build-arg run_tests=ON, the tests and further checks are run during the build.

# Set base image
ARG BASE_IMAGE=movesrwth/storm-dependencies:latest
# Type of the final image (dev or release)
ARG image_type=dev


# Development image (also used to build the release image)
###########################################################
FROM $BASE_IMAGE AS dev
LABEL org.opencontainers.image.authors="dev@stormchecker.org"


# Configuration arguments
#########################
# The arguments can be set from the commandline with:
# --build-arg <arg_name>=<value>

# Specify number of threads to use for parallel compilation
ARG no_threads=4
# CMake build type
ARG build_type=Release
# Carl repo and tag to use
ARG carl_repo="https://github.com/stormchecker/carl-storm.git"
ARG carl_tag="14.37"
# Specify Storm configuration (ON/OFF)
ARG disable_cudd="OFF"
ARG disable_cvc5="OFF"
ARG disable_glpk="OFF"
ARG disable_gmm="OFF"
ARG disable_gurobi="OFF"
ARG disable_highs="OFF"
ARG disable_libarchive="OFF"
ARG disable_mathsat="OFF"
ARG disable_soplex="OFF"
ARG disable_spot="OFF"
ARG disable_sylvan="OFF"
ARG disable_xerces="OFF"
ARG disable_z3="OFF"
ARG developer="OFF"
ARG cln_exact="OFF"
ARG cln_ratfunc="ON"
ARG all_sanitizers="OFF"

# Specify additional CMake arguments for Storm
ARG cmake_args=""
ARG ccache_size="3G"
# Whether to run the tests and checks after building
ARG run_tests="OFF"

# Check for ccache
ENV CCACHE_DIR=/root/.ccache
RUN command -v ccache >/dev/null 2>&1 || { echo "ERROR: ccache not found in the base image"; exit 1; }


# Build Storm
#############
RUN mkdir /opt/storm
WORKDIR /opt/storm

# Copy the content of the current local Storm repository into the Docker image
COPY . .

# Seed ccache from the workflow cache directory if available.
RUN mkdir -p "$CCACHE_DIR" && \
    if [ -d "/opt/storm/.ci-ccache" ]; then cp -a /opt/storm/.ci-ccache/. "$CCACHE_DIR"/; fi

# Switch to build directory
RUN mkdir -p /opt/storm/build
WORKDIR /opt/storm/build

# Configure Storm
RUN cmake -DCMAKE_BUILD_TYPE=$build_type \
          -DSTORM_PORTABLE=ON \
          -DSTORM_CARL_GIT_REPO=$carl_repo \
          -DSTORM_CARL_GIT_TAG=$carl_tag \
          -DSTORM_DISABLE_CUDD=$disable_cudd \
          -DSTORM_DISABLE_CVC5=$disable_cvc5 \
          -DSTORM_DISABLE_GLPK=$disable_glpk \
          -DSTORM_DISABLE_GMM=$disable_gmm \
          -DSTORM_DISABLE_GUROBI=$disable_gurobi \
          -DSTORM_DISABLE_HIGHS=$disable_highs \
          -DSTORM_DISABLE_LIBARCHIVE=$disable_libarchive \
          -DSTORM_DISABLE_MATHSAT=$disable_mathsat \
          -DSTORM_DISABLE_SOPLEX=$disable_soplex \
          -DSTORM_DISABLE_SPOT=$disable_spot \
          -DSTORM_DISABLE_SYLVAN=$disable_sylvan \
          -DSTORM_DISABLE_XERCES=$disable_xerces \
          -DSTORM_DISABLE_Z3=$disable_z3 \
          -DSTORM_DEVELOPER=$developer \
          -DSTORM_USE_CLN_EA=$cln_exact \
          -DSTORM_USE_CLN_RF=$cln_ratfunc \
          -DSTORM_COMPILE_WITH_ALL_SANITIZERS=$all_sanitizers \
          $cmake_args ..

# Build Storm
# (This can be adapted to only build 'storm' or 'binaries' depending on custom needs)
RUN ccache --max-size=$ccache_size && ccache --zero-stats && make -j $no_threads && ccache --show-stats --verbose

# Install Storm
RUN make install

# Run tests
RUN if [ "$run_tests" = "ON" ]; then \
      ctest --output-on-failure; \
    fi
# Check compile flags
RUN if [ "$run_tests" = "ON" ]; then \
      bash /opt/storm/.github/actions/check-compile-flags-action/check-flags.sh /opt/storm/build/src/storm/CMakeFiles/storm.dir "$build_type"; \
    fi
# Build and run starter project with Storm from the build directory
RUN if [ "$run_tests" = "ON" ]; then \
      git clone https://github.com/stormchecker/storm-project-starter-cpp /opt/storm-starter && \
      cmake -S /opt/storm-starter -B /opt/storm-starter/build-intree -DNEVER_FETCH_STORM=TRUE -DSTORM_ROOT=/opt/storm/build && \
      cmake --build /opt/storm-starter/build-intree -j $no_threads && \
      /opt/storm-starter/build-intree/bin/starter-project /opt/storm/resources/examples/testfiles/dtmc/brp-16-2.pm 'P=? [F s=5]'; \
    fi
# Build and run starter project with installed Storm
# The build directory is hidden (by an empty tmpfs)
RUN --mount=type=tmpfs,target=/opt/storm/build \
    if [ "$run_tests" = "ON" ]; then \
      /usr/local/bin/storm --version && \
      cmake -S /opt/storm-starter -B /opt/storm-starter/build-installed -DNEVER_FETCH_STORM=TRUE && \
      cmake --build /opt/storm-starter/build-installed -j $no_threads && \
      /opt/storm-starter/build-installed/bin/starter-project /opt/storm/resources/examples/testfiles/dtmc/brp-16-2.pm 'P=? [F s=5]'; \
    fi

WORKDIR /opt/storm


# Release image
###############
# Only contains the files installed by Storm (as listed in the install manifest).
# The files are copied via a bind mount to avoid duplicating the base image's /usr/local.
FROM $BASE_IMAGE AS release
LABEL org.opencontainers.image.authors="dev@stormchecker.org"
RUN --mount=type=bind,from=dev,source=/,target=/mnt/dev \
    test -s /mnt/dev/opt/storm/build/install_manifest.txt && \
    sed 's|^/||' /mnt/dev/opt/storm/build/install_manifest.txt | tar -cf - -C /mnt/dev -T - | tar -xf - -C / && \
    storm --version
WORKDIR /opt


# Final image
#############
FROM ${image_type}

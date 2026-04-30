# Dockerfile for Storm
######################
# The Docker image can be built by executing:
# docker build -t yourusername/storm .
# A different base image can be set from the commandline with:
# --build-arg BASE_IMAGE=<new_base_image>

# Set base image
ARG BASE_IMAGE=movesrwth/storm-dependencies:latest
FROM $BASE_IMAGE
LABEL org.opencontainers.image.authors="dev@stormchecker.org"


# Configuration arguments
#########################
# The arguments can be set from the commandline with:
# --build-arg <arg_name>=<value>

# Specify number of threads to use for parallel compilation
ARG no_threads=1
# CMake build type
ARG build_type=Release
# Carl tag to use
ARG carl_tag="14.34"
# Specify Storm configuration (ON/OFF)
ARG disable_cudd="OFF"
ARG disable_glpk="OFF"
ARG disable_gmm="OFF"
ARG disable_gurobi="OFF"
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


# Build Storm
#############
RUN if ! command -v ccache >/dev/null 2>&1; then \
        if command -v apt-get >/dev/null 2>&1; then \
            apt-get update && apt-get install -y --no-install-recommends ccache && rm -rf /var/lib/apt/lists/*; \
        elif command -v apk >/dev/null 2>&1; then \
            apk add --no-cache ccache; \
        elif command -v pacman >/dev/null 2>&1; then \
            pacman -S --noconfirm ccache; \
        else \
            echo "No supported package manager found for ccache installation"; \
        fi; \
    fi

ENV CCACHE_DIR=/root/.ccache
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
          -DSTORM_CARL_GIT_TAG=$carl_tag \
          -DSTORM_DISABLE_CUDD=$disable_cudd \
          -DSTORM_DISABLE_GLPK=$disable_glpk \
          -DSTORM_DISABLE_GMM=$disable_gmm \
          -DSTORM_DISABLE_GUROBI=$disable_gurobi \
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
RUN ccache --max-size=3G && ccache --zero-stats && make -j $no_threads && ccache --show-stats --verbose

WORKDIR /opt/storm

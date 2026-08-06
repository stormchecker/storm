# Create a package for Offline installation

**Written in 2020, checked in 2024. Not updated with latest changes.**

On this page we detail steps to obtain an installation package that can be used to install Storm on a system without an internet connection.
This is useful, e.g., for long term archiving where we can not be sure how long dependencies will be available.

We assume that there is a fixed reference system based on Ubuntu which is not going to change (e.g. a Virtual Machine that will be made available)

Since we can not simply execute `apt-get install ...`, the dependencies (and their dependencies...) have to be included in the installation package.
The easiest way to do that is to tell `apt-get` to only download the packages and to put them into a directory you like (lets say the current working directory `.`):
```console
sudo apt-get -d -o Dir::Cache=. -o DIR::Cache::archives=. install <Packages>
```

Replace `<Packages>` with the packages you need (see stormchecker.org for more info).
The downloaded `*.deb` files have to be included in the installation package.

Apart from that we will need to download the sources from _carl_ (recall to use the master14 branch) and, of course, the storm sources.

We currently check out the _l3pp_ and _eigen_ git repositories during the building process. To make this possible in an offline fashion you might need to
 
 * remove the `GIT_REPOSITORY ...` and `GIT_TAG ...` arguments in the _l3pp_ section in `$STORMDIR/resources/3rdparty/CMakeLists.txt`,
 * remove the `ExternalProject_Add(..)` statement in the _eigen_ section in `$STORMDIR/resources/3rdparty/CMakeLists.txt`, and
 * make sure that `$STORMDIR/build/include/resources/3rdparty/StormEigen` already exist and contains the Eigen sources before running `cmake`/`make`. You can take these files from a working Storm installation.
 
Assuming that the directory `dependencies` contains the `*.deb` files, and `carl` and `storm` contain the source files of Carl and Storm, respectively, an installation script can look as follows:

```console
#!/bin/bash

# Get the number of available threads for multithreaded compiling
THREADS=$(nproc)

# cd to the directory where the script lies in
cd "$( dirname "${BASH_SOURCE[0]}" )"

echo "Installing dependencies. You might need to enter the root password"
cd dependencies
sudo dpkg -i *.deb
cd ..

echo "Installing carl using $THREADS threads"
cd carl
mkdir -p build
cd build
cmake .. -DUSE_CLN_NUMBERS=ON -DUSE_GINAC=ON
make lib_carl -j$THREADS
cd ../../

echo "Installing Storm using $THREADS threads"
cd storm
mkdir -p build
cd build
cmake ..
make storm-cli -j$THREADS
cd ../../

echo "Installation successfull."
```

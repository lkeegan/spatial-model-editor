#!/bin/bash

# install common linux CI dependencies

set -e -x

sudo apt-get update -yy

# temporary fix (?) for https://github.com/actions/virtual-environments/issues/3376
sudo apt-get remove -yy libgcc-11-dev gcc-11

# install build/test dependencies
sudo apt-get install -yy ccache xvfb jwm lcov

# install qt5 build dependencies
sudo apt-get install -yy libglu1-mesa-dev libx11-dev libx11-xcb-dev libxext-dev libxfixes-dev libxi-dev libxrender-dev libxcb1-dev libxcb-glx0-dev libxcb-keysyms1-dev libxcb-image0-dev libxcb-shm0-dev libxcb-icccm4-dev libxcb-sync-dev libxcb-xfixes0-dev libxcb-shape0-dev libxcb-randr0-dev libxcb-render-util0-dev libxkbcommon-dev libxkbcommon-x11-dev '^libxcb.*-dev'

# use gcc 9
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 100
sudo update-alternatives --install /usr/bin/gcov gcov /usr/bin/gcov-9 100

# use clang 9
sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-9 100
sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-9 100
sudo update-alternatives --install /usr/bin/llvm-cov llvm-cov /usr/bin/llvm-cov-9 100

# check versions
cmake --version
g++ --version
gcov --version
clang++ --version
llvm-cov --version
python --version
ccache --version

# set maximum ccache size to 400MB
ccache --max-size 400M
ccache --cleanup
ccache --zero-stats
ccache --show-stats

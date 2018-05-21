#!/usr/bin/env bash
set -ex

export LLVM_DIR="/usr/share/llvm-${LLVM_VERSION}/cmake/"

sudo add-apt-repository -y "deb http://llvm.org/apt/trusty/ llvm-toolchain-trusty-${LLVM_VERSION} main"
wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-get -q update
sudo apt-get -y install "llvm-${LLVM_VERSION}-dev" "llvm-${LLVM_VERSION}-tools" "libclang-${LLVM_VERSION}-dev" libedit-dev libyaml-cpp-dev libboost-dev

# Install test dependencies.
sudo apt-get -y install libboost-python-dev libboost-thread-dev
sudo apt-get -y install lcov
sudo apt-get install python-dev python3-dev

# Install pybind11 from source (we need pybind11 (>=2.2.0))
git clone https://github.com/pybind/pybind11.git
cd pybind11
git checkout tags/v2.2.3
mkdir build
cd build
cmake .. -DPYBIND11_TEST=OFF -DPYBIND11_PYTHON_VERSION=$PYTHON_VERSION
make -j4
sudo make install
cd ../..

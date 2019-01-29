#!/usr/bin/env bash
set -ex

if [ "${OS_NAME}" = "linux" ]; then
  export LLVM_DIR="/usr/share/llvm-${LLVM_VERSION}/cmake/"
fi

mkdir build
cd build

# [DEBUG]
test -e ${LLVM_VERSION} && echo file exists || echo file not found
find / -name ClangConfig.cmake

if [ $BUILD_NAME = TRUSTY_GCC_DEBUG ]; then
  cmake "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}" "-DLLVM_DIR=${LLVM_DIR}" "-DCODECOV=ON" ..
else
  cmake "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}" "-DLLVM_DIR=${LLVM_DIR}" "-DCODECOV=OFF" ..
fi

if [ "$OS_NAME" = "linux" ] && [ $(lsb_release -sc) = "bionic" ]; then
  make check-format
fi

make -j4 tests binding_tests

if [ $BUILD_NAME = TRUSTY_GCC_DEBUG ]; then
  make chimera_coverage
else
  ctest --output-on-failure
fi

$SUDO make install

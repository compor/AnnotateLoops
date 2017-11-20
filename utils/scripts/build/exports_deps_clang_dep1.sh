#!/usr/bin/env bash

export CC=clang 
export CXX=clang++

export LLVMCONFIG=$(which llvm-config)

export LLVM_DIR=$(${LLVMCONFIG} --prefix)/share/llvm/cmake/

export GTEST_ROOT=/usr/local/gtest-libcxx

export BOOST_ROOT=/bulk/workbench/boost/015900/install/

export CXX_FLAGS="-stdlib=libc++"

export LINKER_FLAGS="-Wl,-L$(${LLVMCONFIG} --libdir)" 
export LINKER_FLAGS="${LINKER_FLAGS} -lc++ -lc++abi" 

export BUILD_TYPE=Debug

export ANNOTATELOOPS_SKIP_TESTS=OFF
export ANNOTATELOOPS_DEBUG=ON

export JSON_DIR=${JSON_DIR}


CMAKE_OPTIONS="-DLLVM_DIR=${LLVM_DIR}"
CMAKE_OPTIONS="${CMAKE_OPTIONS} -DBOOST_ROOT=${BOOST_ROOT}"
CMAKE_OPTIONS="${CMAKE_OPTIONS} -DGTEST_ROOT=${GTEST_ROOT}"
CMAKE_OPTIONS="${CMAKE_OPTIONS} -DANNOTATELOOPS_SKIP_TESTS=${ANNOTATELOOPS_SKIP_TESTS}"
CMAKE_OPTIONS="${CMAKE_OPTIONS} -DANNOTATELOOPS_DEBUG=${ANNOTATELOOPS_DEBUG}"
CMAKE_OPTIONS="${CMAKE_OPTIONS} -Dnlohmann_json_DIR=${JSON_DIR}"

export CMAKE_OPTIONS


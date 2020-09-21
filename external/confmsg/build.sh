#!/usr/bin/env bash

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set -e

CONFIG="${CONFIG:-RelWithDebInfo}"
GENERATOR="${GENERATOR:-Ninja}"
OE_DIR="${OE_DIR:-/opt/openenclave}"

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT_DIR=${THIS_DIR}
BUILD_DIR_BASE="${BUILD_DIR:-$ROOT_DIR/build}"
BUILD_DIR=$BUILD_DIR_BASE/$CONFIG

if [ "$SKIP_SYNC" != 1 ]; then
    cd $ROOT_DIR
    git submodule sync --recursive
    git submodule update --init --recursive
fi

mkdir -p $BUILD_DIR/enclave
mkdir -p $BUILD_DIR/host

COMMON_ARGS=(
    "-G" "$GENERATOR"
    "-DCMAKE_BUILD_TYPE=$CONFIG"
    "-DBUILD_TESTING=ON"
    "-Dopenenclave_DIR=$OE_DIR/lib/openenclave/cmake")

cd $BUILD_DIR/enclave
cmake "${COMMON_ARGS[@]}" -DBUILD_MODE=enclave $ROOT_DIR
cmake --build .

cd $BUILD_DIR/host
cmake "${COMMON_ARGS[@]}" -DBUILD_MODE=host -DENCLAVE_BUILD_DIR=$BUILD_DIR/enclave $ROOT_DIR
cmake --build .

if [ "$SKIP_TESTS" != 1 ]; then
    if [ "$VERBOSE" == 1 ]; then
      ctest_out_flag=-V
    else
      ctest_out_flag=--output-on-failure
    fi
    ctest -C $CONFIG $ctest_out_flag
fi
#!/usr/bin/env bash
set -e

CONFIG="${CONFIG:-RelWithDebInfo}"
GENERATOR="${GENERATOR:-Ninja}"
OE_DIR="${OE_DIR:-/opt/openenclave}"
BUILD_SERVER="${BUILD_SERVER:-ON}"
PYTHON_EXECUTABLE="${PYTHON_EXECUTABLE:-$(which python3)}"

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT_DIR=${THIS_DIR}
BUILD_DIR_BASE="${BUILD_DIR:-$ROOT_DIR/build}"
BUILD_DIR=$BUILD_DIR_BASE/$CONFIG

if [ "$SKIP_SYNC" != 1 ]; then
    cd $ROOT_DIR
    git submodule sync --recursive
    git submodule update --init --recursive
fi

mkdir -p $BUILD_DIR

COMMON_ARGS=(
  "-G" "$GENERATOR"
  "-DCMAKE_BUILD_TYPE=$CONFIG"
  "-DCMAKE_INSTALL_PREFIX=$ROOT_DIR/dist/$CONFIG"
  "-Dopenenclave_DIR=$OE_DIR/lib/openenclave/cmake"
  "-DPYTHON_EXECUTABLE=$PYTHON_EXECUTABLE"
  "-DBUILD_TESTING=ON"
  "-DBUILD_SERVER=$BUILD_SERVER"
  "-DENABLE_CMAKE_GRAPHVIZ=$GRAPHVIZ")
if [ "$GRAPHVIZ" == "1" ]; then
  COMMON_ARGS+=("--graphviz=graphviz.dot")
fi
if [ "$VERBOSE" == "1" ]; then
  export OE_LOG_LEVEL=INFO
  # export OE_LOG_LEVEL=VERBOSE
fi

cd $BUILD_DIR
cmake "${COMMON_ARGS[@]}" $ROOT_DIR
cmake --build . --target install

if [ "$SKIP_TESTS" != 1 ]; then
    ctest -C $CONFIG -V --output-log $BUILD_DIR/tests.log
fi

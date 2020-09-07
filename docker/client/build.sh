#!/usr/bin/env bash

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set -exo pipefail

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT_DIR=${THIS_DIR}/../..

if [ "$SKIP_SYNC" != 1 ]; then
    cd $ROOT_DIR
    git submodule sync --recursive
    git submodule update --init --recursive
fi

BUILD_TYPE="${BUILD_TYPE:-Release}"

BUILD_DIR=build/docker/client$TYPE/${BUILD_TYPE}
mkdir -p ${ROOT_DIR}/${BUILD_DIR}
MOUNT_DIR=/confonnx
VOLUME_MOUNTS=${ROOT_DIR}:${MOUNT_DIR}

PYTHON_VERSION="${PYTHON_VERSION:-3.7}"
PYTHON_VERSION_DOTLESS="${PYTHON_VERSION//./}"
if [ "$TYPE" == "manylinux" ]; then
  suffix=
  if [[ $PYTHON_VERSION_DOTLESS -lt 38 ]]; then
    suffix=m
  fi
  PYTHON_EXECUTABLE="/opt/python/cp${PYTHON_VERSION_DOTLESS}-cp${PYTHON_VERSION_DOTLESS}${suffix}/bin/python"
  DOCKERFILE=Dockerfile.manylinux.build
  PYTHON_DOCKER_CMD="\
    auditwheel repair wheelhouse/confonnx*-cp${PYTHON_VERSION_DOTLESS}*-linux*.whl --plat manylinux2010_x86_64 -w wheelhouse &&
    mv wheelhouse/confonnx*-manylinux*.whl ."
else
  PYTHON_EXECUTABLE="python"
  DOCKERFILE=Dockerfile.build
  PYTHON_DOCKER_CMD="\
    mv wheelhouse/confonnx*-cp${PYTHON_VERSION_DOTLESS}*-linux*.whl ."
fi

DOCKER_CMD="\
  set -x && \
  cd ${MOUNT_DIR}/${BUILD_DIR} && \
  cmake -GNinja \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${MOUNT_DIR}/dist/${BUILD_TYPE} \
        -DBUILD_SERVER=OFF \
        -DBUILD_CLIENT=ON \
        -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} \
        -DENABLE_ENCLAVE_TESTS=OFF \
        ${MOUNT_DIR} && \
  cmake --build . --target install && \
  cd ${MOUNT_DIR}/dist/${BUILD_TYPE}/lib/python && \
  ${PYTHON_EXECUTABLE} -m pip wheel . -w wheelhouse && \
  ${PYTHON_DOCKER_CMD} && \
  rm -rf wheelhouse"

sudo docker image build \
  --build-arg PYTHON_VERSION=${PYTHON_VERSION} \
  -t confonnx-client:build \
  -f ${THIS_DIR}/${DOCKERFILE} ${THIS_DIR} |& tee ${THIS_DIR}/image-build.log 

sudo docker run --rm \
  --user $(id -u):$(id -g) \
  -v $VOLUME_MOUNTS \
  confonnx-client:build /bin/bash -c "${DOCKER_CMD}" |& tee build.log
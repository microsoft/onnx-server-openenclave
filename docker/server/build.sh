#!/usr/bin/env bash

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set -exo pipefail

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT_DIR=${THIS_DIR}/../..

if [ ! -f ${ROOT_DIR}/enclave.pem ]; then
  openssl genrsa -out ${ROOT_DIR}/enclave.pem -3 3072
  openssl rsa -in ${ROOT_DIR}/enclave.pem -pubout > ${ROOT_DIR}/enclave.pub.pem
fi

if [ "$OE_VERSION" != "" ]; then
  build_args=(
    "--build-arg" "OE_VERSION=${OE_VERSION}"
    "--build-arg" "BUILD_TYPE=${BUILD_TYPE}"
    )
fi

sudo docker image build \
  -t confonnx-server:build \
  -f ${THIS_DIR}/Dockerfile.build \
  "${build_args[@]}" \
  ${THIS_DIR} |& tee ${THIS_DIR}/image-build.log

if [ "$SKIP_SYNC" != 1 ]; then
    cd $ROOT_DIR
    git submodule sync --recursive
    git submodule update --init --recursive
fi

BUILD_TYPE="${BUILD_TYPE:-Release}"
BUILD_DIR=build/docker/server/${BUILD_TYPE}
mkdir -p ${ROOT_DIR}/${BUILD_DIR}
MOUNT_DIR=/confonnx
VOLUME_MOUNTS=${ROOT_DIR}:${MOUNT_DIR}
PYTHON_EXECUTABLE="/usr/bin/python3"

if [[ "$RUN_TESTS" == 1 ]]; then
  test_cmd="&& ctest -C ${BUILD_TYPE} -V"
  # Note: BUILD_CLIENT is ON so that integration tests can be run.
  test_cmake_args="\
    -DBUILD_CLIENT=ON \
    -DENABLE_ENCLAVE_TESTS=ON"
fi


DOCKER_CMD="\
  cd ${MOUNT_DIR}/${BUILD_DIR} && \
  cmake -GNinja \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${MOUNT_DIR}/dist/${BUILD_TYPE} \
        -DBUILD_SERVER=ON \
        -DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} \
        ${test_cmake_args} \
        ${MOUNT_DIR} && \
  cmake --build . --target install && \
  chmod +x ${MOUNT_DIR}/dist/${BUILD_TYPE}/bin/confonnx_server_host && \
  /opt/openenclave/bin/oesign sign -e ${MOUNT_DIR}/dist/${BUILD_TYPE}/bin/confonnx_server_enclave -c ${MOUNT_DIR}/enclave.conf -k ${MOUNT_DIR}/enclave.pem && \
  /opt/openenclave/bin/oesign dump -e ${MOUNT_DIR}/dist/${BUILD_TYPE}/bin/confonnx_server_enclave.signed > ${MOUNT_DIR}/dist/${BUILD_TYPE}/bin/enclave_info.txt \
  ${test_cmd}"

if [ "$CONFONNX_TEST_APP_ID" != "" ]; then
  run_envs=(
    "-e" "CONFONNX_TEST_APP_ID=$CONFONNX_TEST_APP_ID"
    "-e" "CONFONNX_TEST_APP_PWD=$CONFONNX_TEST_APP_PWD"
    "-e" "CONFONNX_TEST_VAULT_URL=$CONFONNX_TEST_VAULT_URL"
    "-e" "CONFONNX_TEST_VAULT_HSM_URL=$CONFONNX_TEST_VAULT_HSM_URL"
    "-e" "CONFONNX_TEST_ATTESTATION_URL=$CONFONNX_TEST_ATTESTATION_URL"
    "-e" "CONFONNX_TEST_VAULT_KEY_PREFIX=$CONFONNX_TEST_VAULT_KEY_PREFIX"
    )
fi

if [[ "$RUN_TESTS" == 1 ]]; then
  test_flags="--device=/dev/sgx"
fi

sudo docker run --rm \
  --user $(id -u):$(id -g) \
  ${test_flags} -v $VOLUME_MOUNTS \
  "${run_envs[@]}" \
  confonnx-server:build /bin/bash -c "${DOCKER_CMD}" |& tee build.log
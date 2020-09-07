#!/usr/bin/env bash

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

echo STARTING

set -exo pipefail

THIS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
ROOT_DIR=${THIS_DIR}/../..

IMAGE_NAME="${IMAGE_NAME:-confonnx-sample-server}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
MODEL_PATH="${MODEL_PATH:-${ROOT_DIR}/external/onnxruntime/onnxruntime/test/testdata/squeezenet/model.onnx}"

DIST_DIR=${ROOT_DIR}/dist/${BUILD_TYPE}/bin

TMP_DIR=$ROOT_DIR/tmp
rm -rf $TMP_DIR
mkdir $TMP_DIR

cp ${DIST_DIR}/* ${TMP_DIR}
cp ${MODEL_PATH} ${TMP_DIR}/model.onnx

sudo docker image build -t $IMAGE_NAME -f ${THIS_DIR}/Dockerfile.run ${TMP_DIR} |& tee ${THIS_DIR}/image-run.log

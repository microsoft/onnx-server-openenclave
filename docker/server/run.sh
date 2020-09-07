#!/usr/bin/env bash

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

set -exo pipefail

IMAGE_NAME="${IMAGE_NAME:-confonnx-sample-server}"
CONTAINER_NAME="${CONTAINER_NAME:-confonnx-sample-server}"
HTTP_PORT=${HTTP_PORT:-8001}

extra_docker_args=()

if [ "$DAEMON" == 1 ]; then
    extra_docker_args+=("-d")
fi
if [ "$KEEP" != 1 ]; then
    extra_docker_args+=("--rm")
fi

server_args=()

if [ "$VERBOSE" == "1" ]; then
    server_args+=("--log_level" "verbose")
fi
if [ "$AUTH_KEY" != "" ]; then
    server_args+=("--auth_key" "$AUTH_KEY")
fi
if [ "$AKV_URL" != "" ]; then
    server_args+=(
        "--use_akv"
        "--akv_app_id" "$AKV_APP_ID"
        "--akv_app_pwd" "$AKV_APP_PWD"
        "--akv_vault_url" "$AKV_URL"
    )
fi
if [ "$AKV_SERVICE_KEY_NAME" != "" ]; then
    server_args+=("--akv_service_key_name" "$AKV_SERVICE_KEY_NAME")
fi
if [ "$AKV_MODEL_KEY_NAME" != "" ]; then
    server_args+=("--akv_model_key_name" "$AKV_MODEL_KEY_NAME")
fi
if [ "$AKV_ATTESTATION_URL" != "" ]; then
    server_args+=("--akv_attestation_url" "$AKV_ATTESTATION_URL")
fi

echo "Starting server on port ${HTTP_PORT}"
sudo docker run --name ${CONTAINER_NAME} \
    --device=/dev/sgx \
    -p ${HTTP_PORT}:8888 \
    "${extra_docker_args[@]}" \
    ${IMAGE_NAME} "${server_args[@]}"

echo "Use 'docker logs ${CONTAINER_NAME}' for server output"

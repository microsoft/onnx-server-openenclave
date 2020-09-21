# Development notes

## Build and Test

General note: If any build problem occurs, try to use a build environment as close as possible
to the one used in the Docker builds (see `docker/server/Dockerfile.build`).
Other configurations may work but have not been tested.

```sh
# Environment variables influence the build:
# OE_DIR=/path/to/openenclave (default: /opt/openenclave)
# CONFIG=Debug (default: RelWithDebInfo)
# VERBOSE=1 (default: 0)
# SKIP_TESTS=1 (default: 0)
CC=clang-7 CXX=clang++-7 ./build.sh
```
Note: Replace `CC` and `CXX` with the relevant clang version on your system.
      See `docker/server/Dockerfile.build` for the currently supported version.

Start the server:
```sh
dist/RelWithDebInfo/bin/confonnx_server_host --model-path external/onnxruntime/onnxruntime/test/testdata/squeezenet/model.onnx --http-port 8001 --enclave-path dist/RelWithDebInfo/bin/confonnx_server_enclave --debug
```

Run an inference request:
```sh
PYTHONPATH=dist/RelWithDebInfo/lib/python python -m confonnx.main --pb-in external/onnxruntime/onnxruntime/test/testdata/squeezenet/test_data_set_0/test_data_0_input.pb --pb-in-names data_0
```

Build Python client wheels (adjust the Python version to your target version):
```sh
PYTHON_VERSION=3.7 ./docker/client/build.sh
```

## Debugging

See https://github.com/openenclave/openenclave/blob/master/docs/GettingStartedDocs/Debugging.md.

If you're using VS Code, then you also find the pre-made debug launch configurations
in `.vscode/launch.json` useful.

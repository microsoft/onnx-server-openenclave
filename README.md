# Confidential ONNX Inference Server

The *Confidential Inferencing Beta* is a collaboration between Microsoft Research, Azure Confidential Compute, Azure Machine Learning, and Microsoft’s ONNX Runtime project and is provided here **as-is** as **beta** in order to showcase a hosting possibility which restricts the machine learning hosting party from accessing both the inferencing request and its corresponding response.

As part of this implementation, the secure Trusted Execution Environment (TEE), generates a private key which is secured within the enclave and is used to decrypt incoming inference requests. The client (reference code also provided), calls the secure enclave to get the code attestation and the public key, with which it would encrypt its requests.

Currently, the provided implementation does not scale as coordinating the private key across multiple nodes is cumbersome. However, 3rd party solutions, alternative hosting environments, and batch processing can incorporate the needed mechanism and build upon this beta offering. 

Microsoft is working on enhancing this offering, but timelines and expected features are not available at this time.

## Getting started

This section describes the steps needed to build and run a confidential inference server on an Azure Confidential Compute VM.

If you choose to [deploy to AKS](AKS-Deployment.md) directly without local testing, follow the build instructions and skip the run instructions.
In that case you do not need to set up an ACC VM but a standard Ubuntu VM (or WSL 2) will be fine.
The steps below were tested on Ubuntu 18.04.

### Setting up an [Azure Confidential Computing](https://azure.microsoft.com/en-us/solutions/confidential-compute) VM

An ACC VM is needed as it provides SGX enclave hardware support for running the server.

[Deploy an Azure confidential computing VM](https://docs.microsoft.com/en-us/azure/confidential-computing/quick-create-marketplace) and connect to it via SSH.

*Note: Azure subscriptions have a default of 8 ACC VM cores per region,
and the development VM would take some of them – if you intend to deploy on AKS,
it is recommended you use the DC2s_v2 VM size - this would use 2 cores
and leave enough cores available for a DC4s_v2 VM in the AKS ACC node pool.
An alternative is to request more core quota by contacting Azure Support.*

We will need Python and Docker:
```sh
sudo apt install docker.io python3 python3-pip
```

For running the inference client, the Azure DCAP Client has to be installed.
This requirement will be removed in a future release.
To install the Azure DCAP Client on Ubuntu 18.04, run:
```sh
echo "deb [arch=amd64] https://packages.microsoft.com/ubuntu/18.04/prod bionic main" | sudo tee /etc/apt/sources.list.d/msprod.list
wget -qO - https://packages.microsoft.com/keys/microsoft.asc | sudo apt-key add -
sudo apt update
sudo apt install az-dcap-client
```

### ONNX models

[Open Neural Network Exchange (ONNX)](http://onnx.ai/) is an open standard format for representing machine learning models.
ONNX is supported by a community of [partners](https://onnx.ai/supported-tools) who have implemented it in many frameworks and tools. 
This repository supports a variation of the generally available ONNX runtime version and can be found in the `external/onnxruntime` folder.
Make sure the ONNX model you use is supported by the provided runtime.

### Building the server image

To build the Docker image of the server, you will follow these three steps:
1. Clone this git repository.
2. Build the server using Docker.
3. Bundle the server and ONNX model into a Docker image.

To provide guarantees to your clients that your code is secure
you would need to provide them with the container code (this repository).
Their client would match the enclave hash they generate (`enclave_info.txt`) with your hosted server.
You can also provide the model hash, so they know which model binary was used to provide the inference results.
More details on that in the following sections.

Clone this repository:
```
git clone https://...
```

All the following commands have to be run in the repository root folder.

Open `enclave.conf` and adjust enclave parameters as necessary:
- `Debug`: Set to 0 for deployment. If left as 1, an attacker has access to the enclave memory.
- `NumTCS`: Set to number of available cores in deployment VM.
- `NumHeapPages`: In-enclave heap memory, increase if out-of-memory errors occur, for example with large models.

By default, an enclave signing key pair is created if it doesn't exist yet.
To use your own, copy the private key to `enclave.pem` in the repository root.

Run the following to build the server:
```sh
docker/server/build.sh
```

The server binaries are stored in `dist/Release`.
In the subfolder `bin/` you will also find an `enclave_info.txt` file.
This file contains the enclave hash `mrenclave` that is needed for validating the enclave identity
when sending inference requests from a client.

As ONNX model we will use an MNIST model from the ONNX model zoo:
```sh
curl https://media.githubusercontent.com/media/onnx/models/master/vision/classification/mnist/model/mnist-7.onnx -o model.onnx
```

We are now ready to bundle the model and the server into a Docker image ready for testing and deployment:
```sh
# Adjust model path and image name if needed.
MODEL_PATH=model.onnx IMAGE_NAME=model-server docker/server/build_image.sh 
```

### Building the client

To use your server, your clients would need to use a proprietary protocol
which will make sure the target server is secure before sending it the
encrypted inferencing request.
This git repository provides an open source Python library called confonnx
which can be used to call the server with the proprietary protocol.

In this section, you will learn how to build, install, and use the library. 

Note that the client is currently only available as Python package for Linux.

Show your Python version:
```sh
python3 --version
# Python 3.7.6
```

Build the Python package:
```sh
# Adjust PYTHON_VERSION accordingly.
PYTHON_VERSION=3.7 docker/client/build.sh
```

The folder `dist/Release/lib/python` now contains the `.whl` file for the requested Python version, for example `confonnx-0.1.0-cp37-cp37m-linux_x86_64.whl`.

Install the Python package:
```sh
python3 -m pip install dist/Release/lib/python/confonnx-0.1.0-cp37-cp37m-linux_x86_64.whl
```

Note: manylinux wheels can be built with `TYPE=manylinux`,
however those do not support enclave identity validation yet.
The non-manylinux wheels built above should work on Ubuntu 18.04 and possibly other versions.

### Running the server and sending requests

Start the server with:
```sh
docker run --rm --name model-server-test --device=/dev/sgx -p 8888:8888 model-server
```

Before sending our first inference request, we will generate some random test data:
```sh
python3 -m confonnx.create_test_inputs --model model.onnx --out input.json
```

Now we can send a request:
```sh
python3 -m confonnx.main --url http://localhost:8888/ --enclave-hash "<mrenclave>" --json-in input.json --json-out output.json
```
The inference result is stored in `output.json`.

Note: Add `--enclave-allow-debug` if `Debug` is set to `1` in `enclave.conf`.

To make sure that the inference results were created from our specific model, we need to hash the model and provide the hash to the client:
```sh
python3 -m confonnx.hash_model model.onnx
# 0d715376572e89832685c56a65ef1391f5f0b7dd31d61050c91ff3ecab16c032
python3 -m confonnx.main --url http://localhost:8888/ --enclave-hash "<mrenclave>" --enclave-model-hash "<modelhash>" --json-in input.json --json-out output.json
```

Stop the server again:
```sh
docker stop model-server-test
```

*Note: In this guide we use the command line interface of the Python client. You may want to consider using the API directly instead.
For more information, see later sections.*

### Deployment to AKS

See the dedicated [AKS deployment page](AKS-Deployment.md).

### Using the Python client API

In the above instructions, the command line inference client was used.
This client is not meant for production scenarios and offers restricted functionality.

Using the Python API directly has the following advantages:
- Simple inference input/output format (dictionary of numpy arrays).
- Efficient handling of multiple requests (avoiding repeated key exchanges).
- Custom error handling.

Example:
```py
import numpy as np
from confonnx.client import Client

client = Client('https://...', auth_key='password123', enclave_hash='<mrenclave>', enclave_model_hash='...')
result = client.predict({
  'image': np.random.random_sample((5,128,128)) # five 128x128 images
})
print(result['digit'])
# [1,6,2,1,0]
```

## Frequently asked questions

### Can models be protected?

The server includes experimental and undocumented options for model protection which
advanced users may use at their own risk. No support is provided for these options.

Full support for model protection will come in a future release.

### Can the server be tested on a non-SGX machine?

Currently not, though this is planned for a future release.

### Can the Python client be built for macOS or Windows?

Currently not, though community contributions are highly welcome to support this.

### Is there a C/C++ version of the client?

The Python client is a thin wrapper around C++ code (see `confonnx/client` and `external/confmsg`).
This code can be used as basis for building a custom native client.

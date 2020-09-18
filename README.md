# Confidential ONNX Inference Server

The *Confidential Inferencing Beta* is a collaboration between Microsoft Research, Azure Confidential Compute, Azure Machine Learning, and Microsoft’s ONNX Runtime project and is provided here **As-Is** as **beta** in order to showcase a hosting possibility which restricts the machine learning hosting party from accessing both the inferencing request and its corresponding response.

As part of this implementation, the secure Trusted Execution Environment (TEE) generates a private (ECDH) key which is secured within the enclave and used to decrypt incoming inference requests. The client (reference code also provided) first obtains the server's public key and the attestation report proving that the key was created by the TEE. It then completes a key exchange to derive an encryption key for its request.

Currently, the provided AKS deployment example only work on a single node, as it is required to provision the same private key to all inference enclaves to scale to multiple nodes. Developers can use the [key provider interface](confonnx/server/enclave/key_vault_provider.h) to plug their key distribution solution, but we do not support any official implementation at this stage. [We welcome open source contributions](CONTRIBUTING.md)

## Overview of steps

To make this tutorial easier to follow, we first describe how to locally build and run the inference server on an Azure Confidential Computing VM, then separately describe the steps to build and deploy a container on Azure Kubernetes Server.

### Setting up a local deployment on an [ACC VM](https://azure.microsoft.com/en-us/solutions/confidential-compute)
The reason for running this deployment flow on an ACC VM is that during the deployment, you will be able to test the server locally. This requires Intel SGX support on the server, which is enabled on DC-series VMs from Azure Confidential Computing.
In case you don't need to test the server locally, an ACC VM is not required - skip to the [AKS deployment tutorial](AKS-Deployment.md) after you build the server image.

*Note: The azure subscriptions have default of 8 cores, and the development VM would take some of them – it is recommended you use the DC2sv2 VM for the build machine with 2 cores and the rest can be used by the ACC AKS*

### Prepare your ONNX model
[Open Neural Network Exchange (ONNX)](http://onnx.ai/) is an open standard format for representing machine learning models. ONNX is supported by a community of [partners](https://onnx.ai/supported-tools) who have implemented it in many frameworks and tools. Most frameworks (Pytorch, TensorFlow, etc) support converting models to ONNX.

This repository depends on an OpenEnclave port of the generally available ONNX runtime in [external/onnxruntime].
Make sure the ONNX model you use is supported by the provided runtime. 

### Build the server image
In this flow you would build a server image, which you can test locally, and eventually deploy to an AKS cluster. 
You would need to follow these 3 main steps: 
1.	Clone this git repo.
2.	Build a generic container.
3.	Bundle the generic container with your own ONNX model.

To provide guarantees to your clients that your code is secure – you would need to provide them with the container code. Their client would match the enclave hash they generate (enclave_info.txt) with your hosted server. You can also provide the model hash, so they know which model binary was used to provide the inferencing.

### Test inference with the sample client
To use your server, your clients would need to use a proprietary protocol which will make sure the target server is secure before sending it the encrypted inferencing request. This git repository provides an open source Python library called confonnx which can be used to call the server with the proprietary protocol.

*Note: During the deployment we use a command line, but the command line interface is not ideal for production. Consider using the API directly (more information hereunder)*
*Note: The client library is also used to generate the hash of the ONNX model and create inference test data.*

### AKS Deployment
Once you have the server image you can run it on your VM via docker, and via client library you will be able to test that everything is working properly. 
Once you have built and tested the confidential inference server container on your VM, you are ready to [deploy on an AKS cluster](AKS-Deployment.md).
**Remember, without a key management solution you are can only deploy on a single node**

# Building and testing on an ACC VM
This section describes the steps needed to build and run a confidential inference server on an Azure Confidential Compute VM.

## Deployment Environment
You will need to set up a deployment environment in order to build, bundle, test and deploy the server.
*Note: The following commands were tested on an ACC VM running Ubuntu 18.04.*

### Provision an ACC VM
Follow the steps on how to [Deploy an Azure confidential computing VM](https://docs.microsoft.com/en-us/azure/confidential-computing/quick-create-marketplace)

**Notes:** You will need an empty resource group.
Image: Ubuntu Server 18.04 (Gen 2)
Choose SSH public key option
VM size: 1x Standard DC2s v2
Public inbound ports SSH(Linux)/RDP(Windows)

### SSH
You can now SSH into your mahcine, go to the accvm resource, choose connect, select SSH
```
ssh -i <private key path> <username>@<ip address>
# <username>@accvm:~$
sudo apt update
```

### Get the code
Clone this repository:
```
git clone https://github.com/microsoft/onnx-server-openenclave
cd onnx-server-openenclave
```

### Install the Azure DCAP client
For running the inference client, the Azure DCAP Client has to be installed. (*note: This requirement may be removed in a future release.*)
To install the Azure DCAP Client on Ubuntu 18.04, run:
```sh 
echo "deb [arch=amd64] https://packages.microsoft.com/ubuntu/18.04/prod bionic main" | sudo tee /etc/apt/sources.list.d/msprod.list
wget -qO - https://packages.microsoft.com/keys/microsoft.asc | sudo apt-key add -
sudo apt update
sudo apt install az-dcap-client
```

##### Install Python3
Check version:
```sh
python3.7 --version
# Python 3.7.5
```
Install:
```
sudo apt install python3.7
sudo apt-get install python3-pip
```
### Install Docker
Install Docker
```sh
sudo apt install docker.io
```

### Build the confidential inference Python client
Build the Python package (it takes some time)
```sh
PYTHON_VERSION=3.7 docker/client/build.sh
```

The folder `dist/Release/lib/python` now contains the `.whl` file for the requested Python version, for example `confonnx-0.1.0-cp37-cp37m-linux_x86_64.whl`.

*Note: manylinux wheels can be built with `TYPE=manylinux`,
however those do not support enclave identity validation yet.
The non-manylinux wheels built above should work on Ubuntu 18.04 and possibly other versions.*

### Install the Python wheel
Install the built library
```sh
python3.7 -m pip install dist/Release/lib/python/confonnx-0.1.0-cp37-cp37m-linux_x86_64.whl
```

### Build the generic server

Open `enclave.conf` and adjust enclave parameters as necessary:
- `Debug`: Set to 0 for deployment. If left as 1, an attacker has access to the enclave memory.
- `NumTCS`: Set to number of available cores in deployment VM.
- `NumHeapPages`: In-enclave heap memory, increase if out-of-memory errors occur, for example with large models.

By default, an enclave signing key pair is created if it doesn't exist yet.
To use your own, copy the private key to `enclave.pem` in the repository root.

Run the following to build the server using Docker: (It takes a while)

```sh
sudo docker/server/build.sh
```

The server binaries are stored in `dist/Release`. In the subfolder `bin/` you will also find an `enclave_info.txt` file. This file contains the enclave hash `mrenclave` that is needed for the clients to validate the enclave's identity before sending inference requests from a client.

### Prepare your ONNX model

The inference server uses the [ONNX Runtime](https://github.com/microsoft/onnxruntime) and hence the model has to be converted into ONNX format first. See the [ONNX Tutorials](https://github.com/onnx/tutorials#converting-to-onnx-format) page for an overview of available converters. **Make sure the target runtime (see external/onnxruntime) supports the ONNX model version.**

For testing, you can download pre-trained ONNX models from the [ONNX Model Zoo](https://github.com/onnx/models).

This guide will use one of the pre-trained MNIST models from the Zoo.
```sh
curl -0 https://media.githubusercontent.com/media/onnx/models/master/vision/classification/mnist/model/mnist-7.onnx --output model.onnx
```

### Compute the Model Hash
To ensure that inference requests are only sent to inference servers that are loaded with a specific model, we can compute the model hash and have the client verify it before sending the inferencing request.
*Note that this is an optional feature.*
```sh
python3.7 -m confonnx.hash_model model.onnx --out model.hash
# 0d715376572e89832685c56a65ef1391f5f0b7dd31d61050c91ff3ecab16c032
```

### Create the Docker Image
We are now ready to bundle the model and the server into a Docker image ready for deployment:
```sh
# Adjust model path and image name if needed.
MODEL_PATH=model.onnx IMAGE_NAME=model-server docker/server/build_image.sh 
```

### Create Inference Test Data
Before testing the server we need some inference test data.
We can use the following tool to create random data according to the model schema:

```sh
python3.7 -m confonnx.create_test_inputs --model model.onnx --out input.json
```
*Note that in this guide we rely on the prefetch mnist model and inputs, so this step is not needed*

### Test the Server Locally

Start the server with:
```sh
sudo docker run --rm --name model-server-test --device=/dev/sgx -p 8888:8888 model-server
```

Now we can send our first inference request:
```sh
python3.7 -m confonnx.main --url http://localhost:8888/ --enclave-hash "<mrenclave>" --enclave-model-hash-file model.hash --json-in input.json --json-out output.json
```
The inference result is stored in `output.json`.

Note: Add `--enclave-allow-debug` if `Debug` is set to `1` in `enclave.conf`.

To stop the server, run:
```sh
sudo docker stop model-server-test
```

## Deployment to AKS

See the dedicated [AKS deployment tutorial](AKS-Deployment.md).

## Using the Python client API

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

### Can multiple instances of the server be deployed for scalability?

Currently not, but support for it will be added in a future release.

### Can models be protected?

The server includes experimental and undocumented options for model protection which
advanced users may use at their own risk. No support is provided for these options.

Full support for model protection will come in a future release.

### Can the server be tested on a non-SGX machine?

Currently not, though this is planned for a future release.

### Can the Python client be built for macOS or Windows?

Currently not, though community contributions are highly welcomed to support this.

### Is there a C/C++ version of the client?

The Python client is a thin wrapper around C++ code (see `confonnx/client` and `external/confmsg`).
This code can be used as basis for building a custom native client.

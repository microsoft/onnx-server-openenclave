# Confidential ONNX Inference Server

The *Confidential Inferencing Beta* is a collaboration between Microsoft Research, Azure Confidential Compute, Azure Machine Learning, and Microsoft’s ONNX Runtime project and is provided here **As-Is** as **beta** in order to showcase a hosting possibility which restricts the machine learning hosting party from accessing both the inferencing request and its corresponding response.

As part of this implementation, the secure Trusted Execution Environment (TEE), generates a private key which is secured within the enclave and is used to decrypt incoming inference requests. The client (reference code also provided), calls the secure enclave to get the code attestation and the public key, with which it would encrypt its requests.

Currently, the provided implementation does not scale as coordinating the private key across multiple nodes is cumbersome. However, 3rd party solutions, alternative hosting environments, and batch processing can incorporate the needed mechanism and build upon this beta offering. [consider contributing](CONTRIBUTING.md)

Microsoft is working on enhancing this offering, but timelines and expected features are not available at this time.

# Abstract
This tutorial is cumbersome and have multiple components to it, this section helps put everything in place.
#### Setting up the deployment environment on an [Azure confidential compute ](https://azure.microsoft.com/en-us/solutions/confidential-compute) VM
The reason for running this deployment flow on an ACC VM is that during the deployment, you will be able to test the server locally. As the server validates the trusted enclave, it will not be able to run on regular CPU. 
In case you choose not to test the server locally, ACC VM is not required, although note that the following deployment flow was only verified on an ACC VM.

*Note: The azure subscriptions have default of 8 cores, and the development VM would take some of them – it is recommended you use the DC2sv2 VM - would use 2 cores and the rest can be used by the ACC AKS*

#### ONNX model
[Open Neural Network Exchange (ONNX)](http://onnx.ai/) is an open standard format for representing machine learning models. ONNX is supported by a community of [partners](https://onnx.ai/supported-tools) who have implemented it in many frameworks and tools. 
This repository supports a variation of the generally available ONNX runtime version and is in external/onnxruntime folder. Make sure the ONNX model you use is supported by the provided runtime. 

#### Build the server image
In this flow you would build a server image, which you can test locally, and eventually deploy to an AKS cluster. 
You would need to follow these 3 main steps: 
1.	Clone this git repo.
2.	Build a generic container.
3.	Bundle the generic container with your own ONNX model.

To provide guarantees to your clients that your code is secure – you would need to provide them with the container code. Their client would match the enclave hash they generate (enclave_info.txt) with your hosted server. You can also provide the model hash, so they know which model binary was used to provide the inferencing.

#### The client
To use your server, your clients would need to use a proprietary protocol which will make sure the target server is secure before sending it the encrypted inferencing request. This git repository provides an open source Python library called confonnx which can be used to call the server with the proprietary protocol.

In this document, you will learn how to build, install, and use the library. 

*Note: During the deployment we use a command line, but the command line interface is not ideal for production. Consider using the API directly (more information hereunder)*

*Note: This library is also used throughout the development flow to generate the hash of the ONNX model and create inference test data.*

#### Next steps
Once you have the server image you can run it locally using docker, and via client library you will be able to test that everything is working properly. Once that is done, you would deploy the image on an ACC AKS cluster.
**remember, only ACC compute is required so the enclave can be attested, and only 1 AKS node, as this is currently limitation**

# Deployment Flow
## Deployment Environment
You will need to set up a deployment environment in order to build, bundle, test and deploy the server.
*note: The following commands were tested on an ACC VM running Ubuntu 18.04, other environments may not work as well.*
##### Provision an ACC VM
[Deploy an Azure confidential computing VM](https://docs.microsoft.com/en-us/azure/confidential-computing/quick-create-marketplace)
**Notes:**
You will need an empty resource group.
Image: Ubuntu Server 18.04 (Gen 2)
Choose SSH public key option
VM size: 1x Standard DC2s v2
Public inbound ports SSH(Linux)/RDP(Windows)

##### SSH
You can now SSH into your mahcine, go to the accvm resource, choose connect, select SSH
```
ssh -i <private key path> <username>@<ip address>
# <username>@accvm:~$
sudo apt update
```
##### Get the code
Clone this repository:
```
git clone https://...
cd onnx-server-openenclave
```
##### Install Azure DCAP
For running the inference client, the Azure DCAP Client has to be installed.
*note: This requirement will be removed in a future release.*
To install the Azure DCAP Client on Ubuntu 18.04, run:
```sh 
echo "deb [arch=amd64] https://packages.microsoft.com/ubuntu/18.04/prod bionic main" | sudo tee /etc/apt/sources.list.d/msprod.list
wget -qO - https://packages.microsoft.com/keys/microsoft.asc | sudo apt-key add -
sudo apt update
sudo apt install az-dcap-client
```
##### Python
You will need it for the client library, and a few more things.
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
##### Docker
Install Docker
```sh
sudo apt install docker.io
```

## Python library
##### Build
Build the Python package (it takes some time)
```sh
PYTHON_VERSION=3.7 docker/client/build.sh
```

The folder `dist/Release/lib/python` now contains the `.whl` file for the requested Python version, for example `confonnx-0.1.0-cp37-cp37m-linux_x86_64.whl`.

*Note: manylinux wheels can be built with `TYPE=manylinux`,
however those do not support enclave identity validation yet.
The non-manylinux wheels built above should work on Ubuntu 18.04 and possibly other versions.*
##### Install
Install the built library
```sh
python3.7 -m pip install dist/Release/lib/python/confonnx-0.1.0-cp37-cp37m-linux_x86_64.whl
```
## Build the generic server

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

## Model and Docker Image Preparation

##### ONNX model

The inference server uses the [ONNX Runtime](https://github.com/microsoft/onnxruntime) and hence the model has to be converted into ONNX format first. See the [ONNX Tutorials](https://github.com/onnx/tutorials#converting-to-onnx-format) page for an overview of available converters. **Make sure the target runtime (see external/onnxruntime) supports the ONNX model version.**

For testing, you can download pre-trained ONNX models from the [ONNX Model Zoo](https://github.com/onnx/models).

This guide will use mnist
```sh
curl -0 https://media.githubusercontent.com/media/onnx/models/master/vision/classification/mnist/model/mnist-7.onnx --output model.onnx
curl -0 https://media.githubusercontent.com/media/onnx/models/master/vision/classification/mnist/model/mnist-7.tar.gz --output dataset.tar.gz
tar -xvf dataset.tar.gz  
```
#### Compute the Model Hash
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

Stop the server again:
```sh
sudo docker stop model-server-test
```

## AKS Deployment
On your deployment vm you will need to install and set up additional components
##### Install Helm

This guide uses Helm to generate Kubernetes manifests from templates.
Follow the instructions at https://helm.sh/docs/intro/install/ to install Helm.
```sh
sudo snap install helm --classic
```
##### Install Htpasswd 
Needed for auth keys
```sh
sudo apt install apache2-utils
```

##### Install Azure CLI and Login
Follow https://docs.microsoft.com/en-us/cli/azure/install-azure-cli?view=azure-cli-latest or:
```sh
curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash
```
Then:
```sh
az login
az account set --subscription "<subscription id>"
```
##### Install kubectl
Install kubectl to control the cluster:
```sh
sudo az aks install-cli
```


##### Set up Azure Container Registry
If you haven't got one already, create an Azure Container Registry:


**must be globally unique**
```sh
MYACR=myregistry
```
Follow https://docs.microsoft.com/en-us/azure/container-registry/container-registry-get-started-azure-cli or
```sh
az group create --name $MYACR-rg --location eastus
az acr create --name $MYACR --resource-group $MYACR-rg --sku basic
```
*Note that it will take some time*
##### Push the Docker Image

You can now push your local Docker server image (which you have built in **Create the Docker Image** and named **model-server**)

```sh
sudo az acr login --name $MYACR
sudo docker tag model-server $MYACR.azurecr.io/model-server
sudo docker push $MYACR.azurecr.io/model-server
```

##### Setting up Confidential AKS Preview
We first need to prepare the Azure enviourment to support the Confidential AKS as it is now only in preview. Check https://review.docs.microsoft.com/en-us/azure/confidential-computing/confidential-nodes-aks-getstarted?branch=pr-en-us-130012 for latest instuctions or:
```sh
az extension add --name aks-preview
az extension list
```
Verify aks-preview is installed
```sh
az feature register --name Gen2VMPreview --namespace Microsoft.ContainerService
az feature list -o table --query "[?contains(name, 'Microsoft.ContainerService/Gen2VMPreview')].{Name:name,State:properties.state}"
```
Verify Microsoft.ContainerService/Gen2VMPreview is registered (it might take a few minutes)
```sh
az provider register --namespace Microsoft.ContainerService
az provider show -n Microsoft.ContainerService | grep registrationState
```
Verify the service is registered
##### Creating AKS cluster
Now we will deploy a new AKS cluster
**Important:** make sure the node count is 1, as aforementioned, this is a current limitation.

First verify MYACR is still an environment variable, if not, redefine it.
```sh
echo $MYACR
```
Then
```sh
MYAKS=aks-acc-test
az group create --name $MYAKS-rg --location eastus
az aks create --resource-group $MYAKS-rg --name $MYAKS --attach-acr $MYACR --vm-set-type VirtualMachineScaleSets --node-count 1 --node-vm-size Standard_DS2_v2 --aks-custom-headers usegen2vm=true
az aks nodepool add --resource-group $MYAKS-rg --cluster-name $MYAKS --name accpool --mode User --node-count 1 --enable-cluster-autoscaler --min-count 1 --max-count 2 --node-vm-size Standard_DC4s_v2 --aks-custom-headers usegen2vm=true
``` 
You can verify the nodepools deployment by
```sh
az aks nodepool show -g $MYAKS-rg --cluster-name $MYAKS -n nodepool1
az aks nodepool show -g $MYAKS-rg --cluster-name $MYAKS -n accpool
```
Check that the SGX device plugin was deployed:
```sh
az aks get-credentials --name $MYAKS --resource-group $MYAKS-rg
kubectl get pods -n kube-system -l app=sgx-device-plugin
#kube-system     sgx-device-plugin-xxxxx     1/1     Running
```
Verify you see sgx-device-plugin-xxxxx

##### Prepare AKS Cluster

In this guide we will use nginx as an L7 load balancer and for TLS support.
The following steps need to be done only once.

First, the nginx ingress controller needs to be installed:
```sh
# See https://kubernetes.github.io/ingress-nginx/deploy/#azure.
kubectl apply -f https://raw.githubusercontent.com/kubernetes/ingress-nginx/controller-0.32.0/deploy/static/provider/cloud/deploy.yaml

# Wait until the installation is complete:
kubectl wait --namespace ingress-nginx \
  --for=condition=ready pod \
  --selector=app.kubernetes.io/component=controller \
  --timeout=120s
```

For more details see:
- https://kubernetes.github.io/ingress-nginx

##### Deploy to AKS Cluster
In this guide all resources are deployed in a custom namespace instead of the default
namespace. This allows to deploy two models at the same time.
**Create a new namespace:**
```sh
kubectl create namespace mymodel
```
**Create an API Authentication Secret**
In this example we use a simple HTTP Basic authentication to prevent anonymous users from sending inference requests:
*Note that you can't use an empty password*
```sh
htpasswd -c auth api
kubectl create secret generic api-auth --from-file=auth --namespace mymodel
```
*Note that the authentication is handled by the ingress controller (which does not run inside an enclave) and does not provide protection where an attacker has administrator access to the Kubernetes cluster. In-enclave client authentication is out-of-scope for this project.*
For more details, see:
https://kubernetes.github.io/ingress-nginx/examples/auth/basic/

**Deploy the server**
We will deploy a single instance of the server and expose it publicly
through an nginx ingress which offers features like TLS and authentication.

```sh
# Open samples/k8s/values.yaml and adjust values as required. You will at least need to change the registry name.
helm template --namespace mymodel --values samples/k8s/values.yaml --output-dir samples/k8s/manifests samples/k8s
kubectl apply --recursive -f samples/k8s/manifests
# Monitor the deployment:
kubectl get all --namespace mymodel
#Show the server logs:
kubectl logs -l app=model-server --namespace mymodel
```
**Send inference requests**
Show the public IP of the nginx ingress:
```sh
kubectl get ingress --namespace mymodel
# model-service-ingress * <IP ADDRESS>  80 4m39s
```
Open your browser at http://<IP ADDRESS>/mymodel (user:api, password: the one you've created) and you should see the message "Healthy".
It may take a minute until the server is fully ready after deployment.
*Note that HTTP (instead of HTTPS) is automatically enabled because we
did not configure a DNS name with TLS certificate.
See the "Next Steps" section for further details.*

Now, send an inference request securily with the client:
```sh
python3.7 -m confonnx.main --url http://<IP ADDRESS>/mymodel --enclave-hash "<mrenclave>" --enclave-model-hash-file model.hash --json-in input.json --json-out output.json --auth-user api --auth-pass 123
```
*Note: Add `--enclave-allow-debug` if `Debug` is set to `1` in `enclave.conf`.*

# Next Steps
##### Consider changing the DNS and TLS 
https://docs.microsoft.com/en-us/azure/aks/ingress-tls
##### Use the Python client API

In the deployment instructions, the command line inference client was used for testing the server.
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

##### Consider a different ingress controller

Azure offers integration with its native Application Gateway L7 load balancer
which can be used as alternative to the nginx ingress controller.

For more details, see:
https://docs.microsoft.com/en-us/azure/application-gateway/ingress-controller-overview

# Frequently asked questions

##### Can multiple instances of the server be deployed for scalability?

Currently not, but support for it will be added in a future release.

##### Can models be protected?

The server includes experimental and undocumented options for model protection which
advanced users may use at their own risk. No support is provided for these options.

Full support for model protection will come in a future release.

##### Can the server be tested on a non-SGX machine?

Currently not, though this is planned for a future release.

##### Can the Python client be built for macOS or Windows?

Currently not, though community contributions are highly welcomed to support this.

##### Is there a C/C++ version of the client?

The Python client is a thin wrapper around C++ code (see `confonnx/client` and `external/confmsg`).
This code can be used as basis for building a custom native client.

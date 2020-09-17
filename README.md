# Confidential ONNX Inference Server

The *Confidential Inferencing Beta* is a collaboration between Microsoft Research, Azure Confidential Compute, Azure Machine Learning, and Microsoft’s ONNX project as is provided here **As-Is** in order to showcase an hosting possibility which restricts the machine learning hosting party from accessing both the inferencing request and its corresponding response.

As part of this implementation, the secure Trusted Execution Environment (TEE), generates a private key which is secured within the enclave and is used to decrypt incoming inference requests. The client (reference code also provided), calls the secure enclave to get the code attestation and the public key, with which it would encrypt its requests.

Currently, the provided implementation does not scale as coordinating the private key across multiple nodes is cumbersome. However, 3rd party solutions, alternative hosting environments, and batch processing can incorporate the needed mechanism and build upon this beta offering. 

Microsoft is working on enhancing this offering, but timelines and expected features are not available at this time.

## Prerequisites

In the following a deployment on Azure Kubernetes Service (AKS) using Azure Confidential Compute (ACC) VMs is assumed.

### Development environment

The following commands were tested on an ACC VM running Ubuntu 18.04, other environments may work as well.

For running the inference client, the Azure DCAP Client has to be installed.
This requirement will be removed in a future release.
To install the Azure DCAP Client on Ubuntu 18.04, run:
```sh
echo "deb [arch=amd64] https://packages.microsoft.com/ubuntu/18.04/prod bionic main" | sudo tee /etc/apt/sources.list.d/msprod.list
wget -qO - https://packages.microsoft.com/keys/microsoft.asc | sudo apt-key add -
sudo apt update
sudo apt install az-dcap-client
```

For more details see https://docs.microsoft.com/en-us/azure/virtual-machines/dcv2-series.

### ACC Quota

Make sure there is enough core quota available for DC-series VMs in the Azure subscription where the server should be deployed. Note that each subscription by default has a quota of 8 cores for the DC series. This is enough for following the rest of this guide.

### Install Azure CLI and Login

Follow https://docs.microsoft.com/en-us/cli/azure/install-azure-cli, then run:

```sh
az login

# Set default subscription
az account set --subscription "<subscription id>"
```

### Install Helm

This guide uses Helm to generate Kubernetes manifests from templates.
Follow the instructions at https://helm.sh/docs/intro/install/ to install Helm.

On Ubuntu, you can install Helm using Snap:
```sh
sudo snap install helm --classic
```

### Set-up ACR

If you haven't got one already, create an Azure Container Registry:

```sh
# must be globally unique
MYACR=myregistry

az group create --name $MYACR-rg --location eastus
az acr create --name $MYACR --resource-group $MYACR-rg --sku basic
```

For more details see https://docs.microsoft.com/en-us/azure/container-registry/.

### Enable Preview AKS

Enable the `Gen2VMPreview` AKS preview feature in your subscription in order to use ACC VMs:
```sh
az extension add --name aks-preview
az feature register --name "Gen2VMPreview" --namespace "Microsoft.ContainerService" --verbose
# wait until the following command outputs "Registered"
az feature show --name Gen2VMPreview --namespace "Microsoft.ContainerService"
az provider register --namespace "Microsoft.ContainerService"
# wait until the following command outputs "Registered"
az provider show -n Microsoft.ContainerService | grep registrationState
```

### Set-up AKS Cluster

Create the AKS cluster:
```sh
# from "Set-up ACR" step
MYACR=myregistry

MYAKS=aks-acc-test

az group create --name $MYAKS-rg --location eastus

# Create the cluster with a one-node non-ACC system node pool.
az aks create --resource-group $MYAKS-rg --name $MYAKS --attach-acr $MYACR --vm-set-type VirtualMachineScaleSets --node-count 1 --node-vm-size Standard_DS2_v2 --aks-custom-headers usegen2vm=true

# Add the ACC user node pool with auto-scaling.
# Note: min-count 0 is available soon (https://github.com/Azure/AKS/issues/1565).
az aks nodepool add --resource-group $MYAKS-rg --cluster-name $MYAKS --name accpool --mode User  --node-count 1 --enable-cluster-autoscaler --min-count 1 --max-count 2 --node-vm-size Standard_DC4s_v2 --aks-custom-headers usegen2vm=true

# Show both nodepools:
az aks nodepool show -g $MYAKS-rg --cluster-name $MYAKS -n nodepool1
az aks nodepool show -g $MYAKS-rg --cluster-name $MYAKS -n accpool
```

Install kubectl to control the cluster:
```sh
sudo az aks install-cli
```

Check that the SGX device plugin was deployed:
```sh
az aks get-credentials --name $MYAKS --resource-group $MYAKS-rg
kubectl get pods -n kube-system -l app=sgx-device-plugin
```

If you can't see `sgx-device-plugin`, run:
```
kubectl apply -f https://raw.githubusercontent.com/agowdamsft/sgxk8ssamples/master/samples/sgxplugin/sgx-1.17plus.yaml
```

If you decide to delete the AKS cluster again, run:
```sh
az aks delete --resource-group $MYAKS-rg --name $MYAKS
```

For more details see:
- https://docs.microsoft.com/en-us/azure/aks/cluster-container-registry-integration
- https://docs.microsoft.com/en-us/azure/aks/cluster-autoscaler
- https://github.com/Azure/aks-engine/blob/master/docs/topics/sgx.md

## Building

### Clone the repository

Clone this repository:
```
git clone https://...
```

All the following commands have to be run in the repository root folder.

### Build the Server

Open `enclave.conf` and adjust enclave parameters as necessary:
- `Debug`: Set to 0 for deployment. If left as 1, an attacker has access to the enclave memory.
- `NumTCS`: Set to number of available cores in deployment VM.
- `NumHeapPages`: In-enclave heap memory, increase if out-of-memory errors occur, for example with large models.

By default, an enclave signing key pair is created if it doesn't exist yet.
To use your own, copy the private key to `enclave.pem` in the repository root.

Run the following to build the server using Docker:

```sh
docker/server/build.sh
```

The server binaries are stored in `dist/Release`. In the subfolder `bin/` you will also find an `enclave_info.txt` file. This file contains the enclave hash `mrenclave` that is needed for validating the enclave identity when sending inference requests from a client.

### Build and Install the Client

The client is currently only available as Python package for Linux.

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

## Model and Docker Image Preparation

### Convert the Model to ONNX

The inference server uses the [ONNX Runtime](https://github.com/microsoft/onnxruntime) and hence the model has to be converted into ONNX format first. See the [ONNX Tutorials](https://github.com/onnx/tutorials#converting-to-onnx-format) page for an overview of available converters.

For testing, you can download pre-trained ONNX models from the [ONNX Model Zoo](https://github.com/onnx/models).

### Compute the Model Hash

To ensure that inference requests are only sent to inference servers that are loaded with our model, we need to compute the model hash and keep it for later.
Although this is an optional feature, it is highly recommended.
Otherwise it would allow an attacker to swap out the inference server and load a different model, returning different inference results.

```sh
python3 -m confonnx.hash_model model.onnx --out model.hash
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
python3 -m confonnx.create_test_inputs --model model.onnx --out input.json
```

### Test the Server Locally

Start the server with:
```sh
docker run --rm --name model-server-test --device=/dev/sgx -p 8888:8888 model-server
```

Now we can send our first inference request:
```sh
python3 -m confonnx.main --url http://localhost:8888/ --enclave-hash "<mrenclave>" --enclave-model-hash-file model.hash --json-in input.json --json-out output.json
```
The inference result is stored in `output.json`.

Note: Add `--enclave-allow-debug` if `Debug` is set to `1` in `enclave.conf`.

Stop the server again:
```sh
docker stop model-server-test
```

## Deployment

### Push the Docker Image

You can now push your Docker image to ACR:

```sh
# See the "Set-up ACR" step.
MYACR=myregistry

az acr login --name $MYACR
docker tag model-server $MYACR.azurecr.io/model-server
docker push $MYACR.azurecr.io/model-server
```

### Prepare AKS Cluster

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

### Deploy to AKS Cluster

We now have everything in place to deploy the inference server to AKS.

#### Create a Namespace

In this guide all resources are deployed in a custom namespace instead of the default
namespace. This allows to deploy two models at the same time.

Create a new namespace:
```sh
kubectl create namespace mymodel
```

To delete the namespace (and all its resources):
```sh
kubectl delete namespace mymodel
```

#### Create an API Authentication Secret

In this example we use a simple HTTP Basic authentication to prevent anonymous users from sending inference requests:
```sh
htpasswd -c auth api
kubectl create secret generic api-auth --from-file=auth --namespace mymodel

# Delete secret:
# kubectl delete secret api-auth --namespace mymodel
```
Note that the authentication is handled by the ingress controller (which does not run inside an enclave) and does not provide protection where an attacker has administrator access to the Kubernetes cluster. In-enclave client authentication is out-of-scope for this project.

For more details, see:
- https://kubernetes.github.io/ingress-nginx/examples/auth/basic/

#### Deploy the server

We will deploy a single instance of the server and expose it publicly
through an nginx ingress which offers features like TLS and authentication.

```sh
# Open samples/k8s/values.yaml and adjust values as required.
# Then run:
helm template --namespace mymodel --values samples/k8s/values.yaml --output-dir samples/k8s/manifests samples/k8s
kubectl apply --recursive -f samples/k8s/manifests

# Monitor the deployment:
kubectl get all --namespace mymodel
```

Show the server logs:
```sh
kubectl logs -l app=model-server --namespace mymodel
```

If you need to remove the deployment again, run:
```sh
kubectl delete --recursive -f samples/k8s/manifests
```
Note: You can also remove the whole namespace as shown earlier.

### Send inference requests

Show the public IP of the nginx ingress:
```sh
kubectl get ingress --namespace mymodel
```

Open your browser at http://1.2.3.4/mymodel and you should see the message "Healthy".
It may take a minute until the server is fully ready after deployment.
Note that HTTP (instead of HTTPS) is automatically enabled because we
did not configure a DNS name with TLS certificate.
See the "Next Steps" section for further details.

Now, send an inference request securily with the client:
```sh
python3 -m confonnx.main --url http://1.2.3.4/mymodel --enclave-hash "<mrenclave>" --enclave-model-hash-file model.hash --json-in input.json --json-out output.json --auth-user api --auth-pass password123
```

Note: Add `--enclave-allow-debug` if `Debug` is set to `1` in `enclave.conf`.

## Next Steps

### Use a DNS name with a TLS certificate

In this example we use a free DNS name from Azure of the form `<name>.<location>.cloudapp.azure.com` together with free TLS certificates from [Let's Encrypt](https://letsencrypt.org/).

First, let's set up the DNS name.

Retrieve the public IP address of the nginx ingress controller:
```sh
IP=$(kubectl get service --namespace ingress-nginx ingress-nginx-controller -o=jsonpath='{$.status.loadBalancer.ingress[0].ip}')
echo $IP
```

Retrieve the Azure ID of the public IP address:
```sh
IP_ID=$(az network public-ip list --query "[?contains(ipAddress, '$IP')].[id]" --output tsv)
echo $IP_ID
```

Set a DNS name for the IP address:
```sh
az network public-ip update --ids $IP_ID --dns-name aks-test-mymodel
```

Make a note of the full DNS name as we will need it soon:
```sh
az network public-ip show --ids $IP_ID --query "dnsSettings.fqdn"
```

Now, we need to update the cluster.

We will install [cert-manager](https://github.com/jetstack/cert-manager) for automatically retrieving TLS certificates from Let's Encrypt:
```sh
kubectl apply -f https://github.com/jetstack/cert-manager/releases/download/v0.11.0/cert-manager.yaml

# Verify the installation and wait until everything is running:
kubectl get pods --namespace cert-manager
```

Let's Encrypt can now be registered as certificate issuer at cluster level:
```sh
kubectl apply -f samples/k8s/lets-encrypt.yaml
```

Open the `values.yaml` configuration file and fill in the `host` entry with the full DNS name, and change `letsEncrypt.enable` to `true`.

We are now ready to deploy (or update the existing deployment):
```sh
# Adjust --values as required. 
helm template --namespace mymodel --values samples/k8s/values.yaml --output-dir samples/k8s/manifests samples/k8s
kubectl apply --recursive -f samples/k8s/manifests
```

Open https://name.location.cloudapp.azure.com/mymodel in your browser to see if the certificate got deployed correctly.
Note that if you used the `staging` mode of Let's Encrypt then your browser will notify you that the certificate is from an untrusted certificate authority.
Change the mode to `production` when you are ready and re-run the deployment commands above.

For more details, see:
- https://docs.microsoft.com/en-us/azure/aks/ingress-tls
- https://medium.com/@GeoffreyDV/how-to-set-up-ssl-certificates-for-free-on-azure-kubernetes-service-with-lets-encrypt-c7daca4e9385

### Use the Python client API

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

### Consider a different ingress controller

Azure offers integration with its native Application Gateway L7 load balancer
which can be used as alternative to the nginx ingress controller.

For more details, see:
- https://docs.microsoft.com/en-us/azure/application-gateway/ingress-controller-overview

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

Currently not, though community contributions are highly welcome to support this.

### Is there a C/C++ version of the client?

The Python client is a thin wrapper around C++ code (see `confonnx/client` and `external/confmsg`).
This code can be used as basis for building a custom native client.

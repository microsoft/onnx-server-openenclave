
# AKS Deployment

This guide assumes that you followed the build instructions in the "Getting started" section of the main [README](README.md).

## Preparation

### Install Helm

This guide uses Helm to generate Kubernetes manifests from templates.
Follow the instructions at https://helm.sh/docs/intro/install/ to install Helm.

On Ubuntu, you can install Helm using Snap:
```sh
sudo snap install helm --classic
```

### Install htpasswd 

The `htpasswd` tool is needed for to create API keys to authenticate clients:
```sh
sudo apt install apache2-utils
```

### Install Azure CLI and Login

Follow https://docs.microsoft.com/en-us/cli/azure/install-azure-cli, or run:

```sh
curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash
```

then login with:
```sh
az login
# Set default subscription
az account set --subscription "<subscription id>"
```

### Install kubectl

Install kubectl to control the cluster:
```sh
sudo az aks install-cli
```

### Set up Azure Container Registry

If you haven't got one already, create an Azure Container Registry:

Set the registry name (**must be globally unique**):
```sh
MYACR=myregistry
```

Follow https://docs.microsoft.com/en-us/azure/container-registry/container-registry-get-started-azure-cli or run:
```sh
az group create --name $MYACR-rg --location eastus
az acr create --name $MYACR --resource-group $MYACR-rg --sku basic
```

### Push the Docker Image

You can now push your Docker image to ACR:

```sh
MYACR=myregistry

sudo az acr login --name $MYACR
sudo docker tag model-server $MYACR.azurecr.io/model-server
sudo docker push $MYACR.azurecr.io/model-server
```

### Enable the ACC Preview feature of AKS

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

### Set-up the AKS Cluster

Create the AKS cluster:
```sh
# from "Set-up ACR" step
MYACR=myregistry
MYAKS=aks-acc-test

az group create --name $MYAKS-rg --location eastus

# Create the cluster with a one-node non-ACC system node pool.
az aks create --resource-group $MYAKS-rg --name $MYAKS --attach-acr $MYACR --vm-set-type VirtualMachineScaleSets --node-count 1 --enable-addon confcom --enable-sgxquotehelper --node-vm-size Standard_DS2_v2 --aks-custom-headers usegen2vm=true

# Add the ACC user node pool.
az aks nodepool add --resource-group $MYAKS-rg --cluster-name $MYAKS --name accpool --mode User --node-count 1 --node-vm-size Standard_DC4s_v2 --aks-custom-headers usegen2vm=true

# Show both nodepools:
az aks nodepool show -g $MYAKS-rg --cluster-name $MYAKS -n nodepool1
az aks nodepool show -g $MYAKS-rg --cluster-name $MYAKS -n accpool
```

Check that the SGX device plugin and SGX quote helper were deployed:
```sh
az aks get-credentials --name $MYAKS --resource-group $MYAKS-rg
kubectl get pods -n kube-system -l app=sgx-device-plugin
kubectl get pods -n kube-system -l app=sgx-quote-helper
```
Verify you see sgx-device-plugin-xxxxx and sgx-quote-helper-xxxxx

If you decide to delete the AKS cluster run:
```sh
az aks delete --resource-group $MYAKS-rg --name $MYAKS
```

For more details see:
- https://docs.microsoft.com/en-us/azure/confidential-computing/confidential-nodes-aks-getstarted
- https://docs.microsoft.com/en-us/azure/aks/cluster-container-registry-integration

## AKS Deployment


### Install an Ingress Controller

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

In this guide all resources are deployed in a custom namespace instead of the default namespace.
This allows to deploy two models at the same time.

Create a new namespace:
```sh
kubectl create namespace mymodel
```

To delete the namespace (and all its resources):
```sh
kubectl delete namespace mymodel
```

#### Create an API Authentication Secret
*Note that you cannot use an empty password*

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
# Open samples/k8s/values.yaml and adjust values as required. You will at least need to change the registry name.
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
*Note: You can also remove the whole namespace as shown earlier.*

### Send inference requests

Show the public IP of the nginx ingress:
```sh
kubectl get ingress --namespace mymodel
```

Open your browser at http://<IP ADDRESS>/mymodel (user:api, password: the one you've created) and you should see the message "Healthy".
It may take a minute until the server is fully ready after deployment.
Note that HTTP (instead of HTTPS) is automatically enabled because we
did not configure a DNS name with TLS certificate.
See the "Next Steps" section for further details.

Now, send an inference request securily with the client:
```sh
python3 -m confonnx.main --url http://<IP ADDRESS>/mymodel --enclave-hash "<mrenclave>" --enclave-model-hash "<modelhash>" --json-in input.json --json-out output.json --auth-user api --auth-pass <PASSWORD>
```

*Note: Add `--enclave-allow-debug` if `Debug` is set to `1` in `enclave.conf`.*

## Next Steps

### Use TLS with Let's Encrypt

The proprietary protocol of the inference server ensures that request and response data are encrypted even if the request is served over plaintext HTTP.
However, you might still want to expose your server under a regular domain name with a standard TLS certificate for better integration in existing ecosystems.

Follow the [TLS with Let's Encrypt guide](AKS-TLS-LetsEncrypt.md) to get started.

### Consider a different ingress controller

Azure offers integration with its native Application Gateway L7 load balancer
which can be used as alternative to the nginx ingress controller.

For more details, see the [Ingress for AKS page](https://docs.microsoft.com/en-us/azure/application-gateway/ingress-controller-overview).

## Frequently asked questions

See also the FAQ in the main [README](README.md).

### Can multiple instances of the server be deployed for scalability?

Currently not, but support for it will be added in a future release.

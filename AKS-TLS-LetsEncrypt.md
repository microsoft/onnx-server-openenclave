# Use TLS with Let's Encrypt on AKS

The proprietary protocol of the inference server ensures that request and response data are encrypted.
However, you might still want to expose your server under a regular domain name with a standard TLS certificate for better integration in existing ecosystems.

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
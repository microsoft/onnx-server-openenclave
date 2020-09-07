# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

from typing import Mapping, Optional
import json
from datetime import datetime

import requests
from requests.auth import HTTPBasicAuth, AuthBase
import numpy as np

import confonnx.predict_pb2 as predict_pb2
# Cannot use onnx.numpy_helper as the Python types between predict.proto and onnx.proto differ.
import confonnx.numpy_helper as numpy_helper

import confonnx.confonnx_py as confonnx_py
import confonnx.colors as C

REQUEST_HEADERS = {
    'Content-Type': 'application/octet-stream',
    'Accept': 'application/octet-stream'
}

PREDICT_URL_PATH = 'score'
MODEL_KEY_PROVISIONING_URL_PATH = 'provisionModelKey'

class HTTPBearerKeyAuth(AuthBase):
    def __init__(self, key):
        self.key = key

    def __call__(self, r):
        r.headers['Authorization'] = f'Bearer {self.key}'
        return r

class Client(object):
    def __init__(self, url: str,
                 auth: Optional[dict]=None,
                 enclave_signing_key: Optional[str]=None,
                 enclave_hash: Optional[str]=None,
                 enclave_model_hash: Optional[str]=None) -> None:
        if url[-1] != '/':
            url += '/'
        self.url = url
        self.key_outdated = True
        self.key_rollover_count = -1 # initial key exchange brings it to 0
        self.key_invalid_count = 0
        # Currently, service identifier is equal to the hash of the model loaded in the enclave.
        enclave_service_id = ''
        if enclave_model_hash:
            enclave_service_id = enclave_model_hash
        if enclave_signing_key is None:
            enclave_signing_key = ''
        if enclave_hash is None:
            enclave_hash = ''
        verbose = True
        self._client = confonnx_py.Client(enclave_signing_key,
                                          enclave_hash,
                                          enclave_service_id,
                                          verbose)
        if auth:
            if 'key' in auth:
                self.auth = HTTPBearerKeyAuth(auth['key'])
            elif 'user' in auth and 'pass' in auth:
                self.auth = HTTPBasicAuth(auth['user'], auth['pass'])
            else:
                raise ValueError('unknown auth type')
        else:
            self.auth = None

    def provision_model_key(self, key: str) -> None:
        self._request_key_if_outdated()
        req_msg = self._create_request(bytes.fromhex(key))
        self._send_request(req_msg, MODEL_KEY_PROVISIONING_URL_PATH)
        print('Model key provisioned')

    def predict(self, data: Mapping[str,np.ndarray]) -> Mapping[str,np.ndarray]:
        # Create Protobuf inference request payload
        predict_request = predict_pb2.PredictRequest()
        for input_name, arr in data.items():
            tensor = numpy_helper.from_array(arr)
            predict_request.inputs[input_name].CopyFrom(tensor)

        req_data = predict_request.SerializeToString()

        # Encrypt and send request
        self._request_key_if_outdated()
        print(f'{C.HEADER}{C.BOLD}{C.UNDERLINE}STEP 2{C.END}: Inference request')
        try:
            req_msg = self._create_request(req_data)
            resp_msg = self._send_request(req_msg, PREDICT_URL_PATH)
        except RequestError as e:
            # Retry once from scratch (incl. key refresh) if we got a crypto error.
            # This may happen if the server did a key rollover and we hold a key
            # that is not accepted anymore, e.g. by not making requests for a longer time.
            if e.error_code == 2: # CRYPTO_ERROR
                self._request_key_if_outdated(force=True)
                req_msg = self._create_request(req_data)
                resp_msg = self._send_request(req_msg, PREDICT_URL_PATH)
                self.key_invalid_count += 1
            else:
                raise

        print()
        print(f'{C.HEADER}{C.BOLD}{C.UNDERLINE}STEP 3{C.END}: Inference response')

        # Decrypt response
        print('Decrypting inference response')
        resp_obj = self._client.handle_message(resp_msg)
        assert resp_obj.has_data()
        resp_data = resp_obj.get_data()
        if resp_obj.is_key_outdated():
            self.key_outdated = True

        # Parse Protobuf inference result payload
        print('Parsing inference response')
        predict_response = predict_pb2.PredictResponse()
        predict_response.ParseFromString(resp_data)

        # Convert to numpy arrays
        outputs = {output_name: numpy_helper.to_array(tensor)
                   for output_name, tensor in predict_response.outputs.items()}
        #for output_name, arr in outputs.items():
        #    print(' {}: dtype={} shape={}'.format(output_name, arr.dtype, arr.shape))

        print(f'{C.OKGREEN}{C.BOLD}Inference response successfully processed{C.END}')

        return outputs

    def _create_request(self, data):
        print('Encrypting data')
        req_msg = self._client.make_request(data)
        return req_msg

    def _send_request(self, data, url_path):
        print(f'Sending request ({len(data)/1024:.1f} KiB)')
        t0 = datetime.now()
        resp_msg = _do_request(self.url + url_path, data, REQUEST_HEADERS, self.auth)
        t1 = datetime.now()
        latency_ms = (t1-t0).total_seconds() * 1000
        print(f'Received response after {latency_ms:.1f} ms ({len(resp_msg)/1024:.1f} KiB)')
        return resp_msg

    def _request_key_if_outdated(self, force=False):
        if self.key_outdated or force:
            print(f'{C.HEADER}{C.BOLD}{C.UNDERLINE}STEP 1{C.END}: Establishing encrypted & attested connection with enclave')
            req_msg = self._client.make_key_request()
            print(f'Sending request ({len(req_msg)/1024:.1f} KiB)')
            t0 = datetime.now()
            resp_msg = _do_request(self.url + PREDICT_URL_PATH, req_msg, REQUEST_HEADERS, self.auth)
            t1 = datetime.now()
            latency_ms = (t1-t0).total_seconds() * 1000
            print(f'Received response after {latency_ms:.1f} ms ({len(resp_msg)/1024:.1f} KiB)')
            self._client.handle_message(resp_msg)
            print(f'{C.OKGREEN}{C.BOLD}Established encrypted & attested connection with enclave{C.END}')
            print()
            self.key_outdated = False
            self.key_rollover_count += 1

class RequestError(Exception):
    def __init__(self, status_code: int, response_text: str):
        self.status_code = status_code
        self.response_text = response_text
        try:
            obj = json.loads(response_text)
            self.error_code = obj['error_code']
            self.error_message = obj['error_message']
        except json.JSONDecodeError:
            self.error_code = "N/A"
            self.error_message = response_text
        super().__init__(f"HTTP status={status_code}, error code={self.error_code}, error message={self.error_message}")

def _do_request(url: str, data: bytes, headers: Mapping[str,str], auth: Optional[AuthBase]):
    response = requests.post(url, headers=headers, data=data, auth=auth)
    try:
        response.raise_for_status()
    except requests.HTTPError:
        raise RequestError(response.status_code, response.text)
    return response.content

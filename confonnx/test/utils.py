# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import os
import subprocess
import signal
import time
import requests
import json
import secrets
import numpy as np
from numpy.testing import assert_allclose

SERVER_HOST_PATH = os.environ['SERVER_HOST_PATH']
SERVER_ENCLAVE_PATH = os.environ['SERVER_ENCLAVE_PATH']

SERVER_START_TIMEOUT = 120

class Server(object):
    def __init__(self, model_path, port=8001,
                 key_rollover_interval=None, # seconds
                 key_sync_interval=None, # seconds
                 use_akv=False, akv_app_id=None, akv_app_pwd=None,
                 akv_vault_url=None, akv_service_key_name=None, akv_model_key_name=None,
                 akv_attestation_url=None,
                 use_model_key_provisioning=False
                ):
        self.stopped = False
        self.port = port
        print(f'Starting server (port {port})')
        args = [SERVER_HOST_PATH,
            '--debug',
            '--log-level', 'verbose',
            '--http-port', f'{port}',
            '--enclave-path', SERVER_ENCLAVE_PATH,
            '--model-path', str(model_path)]
        if key_rollover_interval:
            args += ['--key-rollover-interval', str(key_rollover_interval)]
        if key_sync_interval:
            args += ['--key-sync-interval', str(key_sync_interval)]
        if use_akv:
            assert akv_app_id
            assert akv_app_pwd
            assert akv_vault_url
            args += ['--use-akv',
                     '--akv-app-id', akv_app_id,
                     '--akv-app-pwd', akv_app_pwd,
                     '--akv-vault-url', akv_vault_url
                     ]
            if akv_service_key_name:
                args += ['--akv-service-key-name', akv_service_key_name]
            if akv_model_key_name:
                args += ['--akv-model-key-name', akv_model_key_name]
            if akv_attestation_url:
                args += ['--akv-attestation-url', akv_attestation_url]
        if use_model_key_provisioning:
            args += ['--use-model-key-provisioning']
        print('> ' + ' '.join(args))
        self.process = subprocess.Popen(args, bufsize=1)

        # wait until server is ready
        ready = False
        t0 = time.time()
        running = lambda: self.process.poll() is None
        below_timeout = lambda: time.time() - t0 < SERVER_START_TIMEOUT
        while running() and below_timeout() and not ready:
            time.sleep(1)
            try:
                # root url is heartbeat endpoint
                r = requests.get(f'http://localhost:{port}/', timeout=1)
            except requests.exceptions.ConnectionError:
                continue
            ready = r.status_code == 200

        if self.process.returncode is not None:
            self.stopped = True
            raise RuntimeError(f'server exited with code {self.process.returncode}')
        if not ready:
            raise RuntimeError('server did not start up in time')

    def stop(self):
        if self.stopped:
            return
        self.stopped = True
        assert self.process.returncode is None, \
            f'Server exited unexpectedly with code {self.process.returncode}'
        print(f'Stopping server (port {self.port})')
        self.process.terminate()
        exit_code = self.process.wait()
        assert exit_code == -signal.SIGTERM, f'{exit_code}'

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.stop()

class NumpyEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.ndarray):
            return { 'type': obj.dtype.name, 'values': obj.tolist() }
        return json.JSONEncoder.default(self, obj)

def load_json(path):
    with open(path) as fp:
        obj = json.load(fp)

    obj = { name: np.asarray(obj['values'], dtype=obj['type'])
            for name, obj in obj.items() }
    return obj

def save_json(path, data):
    with open(path, 'w') as fp:
        json.dump(data, fp, cls=NumpyEncoder)

def assert_output_allclose(actual, expected):
    assert set(actual.keys()) == set(expected.keys())

    for name, arr in actual.items():
        actual = arr
        expected = expected[name]
        assert_allclose(actual, expected)

def random_akv_key_name() -> str:
    key_name = 'test-' + secrets.token_hex(16)
    print('Generated random key name: ' + key_name)
    return key_name

def delete_akv_key(app_id: str, app_pwd: str, vault_url: str, key_name: str, is_hsm: bool) -> None:
    if is_hsm:
        url = f'{vault_url}keys/{key_name}?api-version=7.0-preview'
    else:
        url = f'{vault_url}secrets/{key_name}?api-version=7.0'

    # Get and parse auth challenge
    r = requests.delete(url)
    challenge = r.headers['WWW-Authenticate']
    prefix = 'Bearer '
    assert challenge.startswith(prefix)
    entries = {pair.split('=')[0].strip(): pair.split('=')[1].replace('"', '')
               for pair in challenge.replace(prefix, '').split(',')}
    authority_url = entries['authorization']
    resource = entries['resource']

    # Retrieve token
    r = requests.post(authority_url + "/oauth2/token", data={
        "grant_type": "client_credentials",
        "client_id": app_id,
        "client_secret": app_pwd,
        "resource": resource
    }, headers={
        "Content-Type": "application/x-www-form-urlencoded",
        "Accept": "application/json"
    })
    token = r.json()['access_token']

    # Delete key
    r = requests.delete(url, headers={'Authorization': prefix + token})
    assert r.status_code in [200, 404]
    print(f'AKV key {key_name} deleted')

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import os
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import time

import pytest
import numpy as np

import confonnx.main
import confonnx.encrypt_model
from confonnx.client import Client
import tensorproto_diff
from utils import Server, load_json, save_json, assert_output_allclose, random_akv_key_name, delete_akv_key

ROOT_DIR = Path(os.environ['REPO_ROOT'])
ORT_TESTDATA_DIR = ROOT_DIR / 'external' / 'onnxruntime' / 'onnxruntime' / 'test' / 'testdata'

SQUEEZENET_DIR = ORT_TESTDATA_DIR / 'squeezenet'
SQUEEZENET = {
    'model': SQUEEZENET_DIR / 'model.onnx',
    'input': {'data_0': SQUEEZENET_DIR / 'test_data_set_0' / 'test_data_0_input.pb'},
    'ref_output': {'softmaxout_1': SQUEEZENET_DIR / 'test_data_set_0' / 'test_data_0_output.pb'}
}

MATMUL_1 = {
    'model': ORT_TESTDATA_DIR / 'matmul_1.onnx',
    'input': {
        'X' : np.array([[1.0, 2.0], [3.0, 4.0], [5.0, 6.0]], dtype=np.float32)
    },
    'ref_output': {
        'Y': np.array([[5.0], [11.0], [17.0]], dtype=np.float32)
    }
}

# TODO add tests that check for enclave hash, signer, model hash

def test_cli_single_server_single_inference(tmp_path):
    model = SQUEEZENET
    port = 8001
    with Server(model_path=model['model'], port=port):
        confonnx.main.main([
            '--url', f'http://localhost:{port}/',
            '--pb-in', str(model['input']['data_0']),
            '--pb-in-names', 'data_0',
            '--pb-out', str(tmp_path)
        ])
    tensorproto_diff.check_equal(model['ref_output']['softmaxout_1'], tmp_path / 'output_0.pb')

def test_cli_single_server_parallel_inferences(tmp_path):
    model = SQUEEZENET
    port = 8001

    with Server(model_path=model['model'], port=port):
        def run_inference(i):
            out_dir = tmp_path / str(i)
            confonnx.main.main([
                '--url', f'http://localhost:{port}/',
                '--pb-in', str(model['input']['data_0']),
                '--pb-in-names', 'data_0',
                '--pb-out', str(out_dir)
            ])
            tensorproto_diff.check_equal(model['ref_output']['softmaxout_1'], out_dir / 'output_0.pb')

        with ThreadPoolExecutor(max_workers=8) as e:
            for _ in e.map(run_inference, range(100)):
                pass

def test_cli_single_server_provision_model_key(tmp_path):
    model = SQUEEZENET
    port = 8001
    encrypted_model_path = tmp_path / 'model.onnx.enc'

    key = confonnx.encrypt_model.generate_key()
    confonnx.encrypt_model.encrypt_model(model['model'], encrypted_model_path, key=key)

    with Server(model_path=encrypted_model_path, port=port, use_model_key_provisioning=True):
        confonnx.main.main([
            '--url', f'http://localhost:{port}/',
            '--mode', 'provision-model-key',
            '--model-key', key
        ])

        confonnx.main.main([
            '--url', f'http://localhost:{port}/',
            '--pb-in', str(model['input']['data_0']),
            '--pb-in-names', 'data_0',
            '--pb-out', str(tmp_path)
        ])
    
    tensorproto_diff.check_equal(model['ref_output']['softmaxout_1'], tmp_path / 'output_0.pb')

def test_cli_json_input_output(tmp_path):
    model = MATMUL_1
    port = 8001

    json_in_path = tmp_path / 'in.json'
    json_out_path = tmp_path / 'out.json'

    save_json(json_in_path, model['input'])

    with Server(model_path=model['model'], port=port):
        confonnx.main.main([
            '--url', f'http://localhost:{port}/',
            '--json-in', str(json_in_path),
            '--json-out', str(json_out_path)
        ])

    output = load_json(json_out_path)

    assert_output_allclose(output, model['ref_output'])

def test_api_basic():
    model = MATMUL_1
    port = 8001

    client = Client(f'http://localhost:{port}/')
    
    with Server(model_path=model['model'], port=port):
        output = client.predict(model['input'])

    assert_output_allclose(output, model['ref_output'])

def test_api_invalid_key():
    model = MATMUL_1
    port = 8001

    client = Client(f'http://localhost:{port}/')

    with Server(model_path=model['model'], port=port):
        client.predict(model['input'])

    assert client.key_invalid_count == 0

    # Simulate multiple key rollovers by restarting the server (without using AKV).
    # The client's key then becomes invalid.
    with Server(model_path=model['model'], port=port):
        # If the client does not repeat the key exchange,
        # then the following request will fail.
        client.predict(model['input'])

    assert client.key_invalid_count == 1

def test_api_local_key_rollover():
    model = MATMUL_1
    port = 8001

    client = Client(f'http://localhost:{port}/')

    with Server(model_path=model['model'], port=port,
                key_rollover_interval=5, key_sync_interval=1):
        for _ in range(12):
            client.predict(model['input'])
            time.sleep(1)

    assert client.key_rollover_count in [2, 3]
    assert client.key_invalid_count == 0

akv = pytest.mark.skipif(
    not os.getenv('CONFONNX_TEST_APP_ID'), reason="CONFONNX_TEST_* env var missing"
)

@akv
def test_api_akv_key_rollover_single_server():
    model = MATMUL_1
    port = 8001
    key_name = random_akv_key_name()
    try:
        client = Client(f'http://localhost:{port}/')
        with Server(model_path=model['model'], port=port,
                    key_rollover_interval=5, key_sync_interval=5,
                    use_akv=True,
                    akv_app_id=os.environ['CONFONNX_TEST_APP_ID'],
                    akv_app_pwd=os.environ['CONFONNX_TEST_APP_PWD'],
                    akv_service_key_name=key_name,
                    akv_vault_url=os.environ['CONFONNX_TEST_VAULT_URL']
                    ):

            for i in range(12):
                print(f'Request #{i}')
                client.predict(model['input'])
                print(f'Request #{i} -- done')
                time.sleep(1)
        assert client.key_rollover_count in [1, 2]
        assert client.key_invalid_count == 0
    finally:
        delete_akv_key(
            os.environ['CONFONNX_TEST_APP_ID'], os.environ['CONFONNX_TEST_APP_PWD'],
            os.environ['CONFONNX_TEST_VAULT_URL'], key_name, is_hsm=False)

@akv
def test_api_akv_multiple_servers():
    model = MATMUL_1
    num_servers = 3
    start_port = 8001
    ports = list(range(start_port, start_port + num_servers))
    key_name = random_akv_key_name()

    servers = []
    try:
        for port in ports:
            servers.append(
                Server(model_path=model['model'], port=port,
                    use_akv=True,
                    akv_app_id=os.environ['CONFONNX_TEST_APP_ID'],
                    akv_app_pwd=os.environ['CONFONNX_TEST_APP_PWD'],
                    akv_service_key_name=key_name,
                    akv_vault_url=os.environ['CONFONNX_TEST_VAULT_URL']))

        client = Client(f'http://foo/')

        for port in ports:
            client.url = f'http://localhost:{port}/'
            client.predict(model['input'])
            # All servers should have the same key from AKV.
            # This assumes that the key itself exists already in AKV,
            # which will be the case due to running of other AKV tests above.
            assert client.key_rollover_count == 0
            assert client.key_invalid_count == 0
    finally:
        stop_errors = []
        for server in servers:
            try:
                server.stop()
            except Exception as e:
                stop_errors.append(e)
        delete_akv_key(
            os.environ['CONFONNX_TEST_APP_ID'], os.environ['CONFONNX_TEST_APP_PWD'],
            os.environ['CONFONNX_TEST_VAULT_URL'], key_name, is_hsm=False)
        for e in stop_errors:
            print(e)
        if stop_errors:
            raise stop_errors[0]

akv_hsm = pytest.mark.skipif(
    not os.getenv('CONFONNX_TEST_VAULT_HSM_URL'), reason="CONFONNX_TEST_* env var missing/empty"
)

@akv_hsm
def test_api_akv_hsm_key_rollover_single_server():
    model = MATMUL_1
    port = 8001
    key_name = random_akv_key_name()
    try:
        client = Client(f'http://localhost:{port}/')

        with Server(model_path=model['model'], port=port,
                    key_rollover_interval=5, key_sync_interval=5,
                    use_akv=True,
                    akv_app_id=os.environ['CONFONNX_TEST_APP_ID'],
                    akv_app_pwd=os.environ['CONFONNX_TEST_APP_PWD'],
                    akv_service_key_name=key_name,
                    akv_vault_url=os.environ['CONFONNX_TEST_VAULT_HSM_URL'],
                    akv_attestation_url=os.environ['CONFONNX_TEST_ATTESTATION_URL']
                    ):
            for i in range(12):
                print(f'Request #{i}')
                client.predict(model['input'])
                print(f'Request #{i} -- done')
                time.sleep(1)
        
        assert client.key_rollover_count in [1, 2]
        assert client.key_invalid_count == 0
    finally:
        delete_akv_key(
            os.environ['CONFONNX_TEST_APP_ID'], os.environ['CONFONNX_TEST_APP_PWD'],
            os.environ['CONFONNX_TEST_VAULT_HSM_URL'], key_name, is_hsm=True)
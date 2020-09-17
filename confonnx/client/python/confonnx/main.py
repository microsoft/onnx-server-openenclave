# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

from typing import List
import argparse
import os
import sys
import json

import numpy as np
import onnx
from onnx import numpy_helper

THIS_DIR = os.path.dirname(__file__)
sys.path.append(os.path.join(THIS_DIR, os.pardir))

from confonnx.client import Client
import confonnx.colors as C

class NumpyEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, np.ndarray):
            return { 'type': obj.dtype.name, 'values': obj.tolist() }
        return json.JSONEncoder.default(self, obj)

def _main_predict(args) -> None:
    inputs = {}
    if args.pb_in:
        for path, input_name in zip(args.pb_in, args.pb_in_names):
            with open(path, 'rb') as fp:
                tensor = onnx.TensorProto()
                tensor.ParseFromString(fp.read())
            print('Using {} as "{}" input'.format(path, input_name))
            inputs[input_name] = numpy_helper.to_array(tensor)
    elif args.json_in:
        with open(args.json_in) as fp:
            obj = json.load(fp)
        for input_name, obj in obj.items():
            arr = np.asarray(obj['values'], dtype=obj['type'])
            inputs[input_name] = arr
    else:
        raise NotImplementedError

    if args.verbose:
        print('Inputs:')
        for input_name, arr in inputs.items():
            print(' {}: dtype={} shape={}'.format(input_name, arr.dtype, arr.shape))

    enclave_signing_key = None
    if args.enclave_signing_key_file:
        with open(args.enclave_signing_key_file) as fp:
            enclave_signing_key = fp.read()

    if args.enclave_model_hash_file:
        with open(args.enclave_model_hash_file) as f:
            enclave_model_hash = f.read()
    else:
        enclave_model_hash = args.enclave_model_hash

    c = Client(url=args.url,
               auth=get_auth(args),
               enclave_signing_key=enclave_signing_key,
               enclave_hash=args.enclave_hash,
               enclave_model_hash=enclave_model_hash,
               enclave_allow_debug=args.enclave_allow_debug)

    try:
        outputs = c.predict(inputs)
    except Exception as e:
        if args.verbose:
            raise
        else:
            print(f'{C.FAIL}{C.BOLD}ERROR: {e}{C.END}')
            sys.exit(1)

    if args.verbose:
        print('Outputs:')
        for output_name, arr in outputs.items():
            print(' {}: dtype={} shape={}'.format(output_name, arr.dtype, arr.shape))

    if args.pb_out:
        os.makedirs(args.pb_out, exist_ok=True)
        for i, (output_name, arr) in enumerate(outputs.items()):
            filename = 'output_{}.pb'.format(i)
            print('Saving "{}" output as {}'.format(output_name, filename))
            path = os.path.join(args.pb_out, filename)
            tensor = numpy_helper.from_array(arr, output_name)
            with open(path, 'wb') as fp:
                fp.write(tensor.SerializeToString())

    if args.json_out:
        print('Saving inference results to {}'.format(args.json_out))
        with open(args.json_out, 'w') as fp:
            json.dump(outputs, fp, cls=NumpyEncoder, sort_keys=True)

def _main_provision_model_key(args) -> None:
    if args.model_key:
        key = args.model_key
    else:
        with open(args.model_key_file) as fp:
            key = fp.read()
    
    enclave_signing_key = None
    if args.enclave_signing_key_file:
        with open(args.enclave_signing_key_file) as fp:
            enclave_signing_key = fp.read()

    c = Client(url=args.url,
               auth=get_auth(args),
               enclave_signing_key=enclave_signing_key,
               enclave_hash=args.enclave_hash,
               enclave_allow_debug=args.enclave_allow_debug)

    c.provision_model_key(key)

def get_auth(args):
    if args.auth_key:
        auth = {'key': args.auth_key}
    elif args.auth_pass:
        auth = {'user': args.auth_user, 'pass': args.auth_pass}
    else:
        auth = None
    return auth

def main(argv: List[str]) -> None:
    parser = argparse.ArgumentParser(description='Test client for sending inference requests')
    parser.add_argument('--mode', help='Request mode', choices=['predict', 'provision-model-key'], default='predict')
    parser.add_argument('--model-key', help='Model key (if --mode provision-model-key)')
    parser.add_argument('--model-key-file', help='Path to model key file (if --mode provision-model-key)')
    parser.add_argument('--url', help='Server URL', default='http://localhost:8001/')
    parser.add_argument('--auth-key', help='Authentication key (HTTP Bearer)')
    parser.add_argument('--auth-user', default='api', help='Authentication username (HTTP Basic)')
    parser.add_argument('--auth-pass', help='Authentication password (HTTP Basic)')
    parser.add_argument('--enclave-signing-key-file', help='Path to expected enclave signing public key (PEM format)')
    parser.add_argument('--enclave-hash', help='Expected enclave hash (hex encoded)')
    parser.add_argument('--enclave-model-hash', help='Expected enclave model hash (hex encoded)')
    parser.add_argument('--enclave-model-hash-file', help='Path to expected enclave model hash (hex encoded)')
    parser.add_argument('--enclave-allow-debug', action='store_true', help='Allow debug-enabled enclaves')
    parser.add_argument('--pb-in', nargs='+', help='TensorProto input files')
    parser.add_argument('--pb-in-names', nargs='+', help='Graph input name for each TensorProto input file')
    parser.add_argument('--pb-out', help='Folder where TensorProto output files should be stored')
    parser.add_argument('--json-in', help='JSON input file')
    parser.add_argument('--json-out', help='JSON output file')
    parser.add_argument('-v', '--verbose', action='store_true', help='Verbose output')
    args = parser.parse_args(argv)

    if args.auth_key and args.auth_pass:
        parser.error('Only one of --auth-key or --auth-user/--auth-pass can be specified')

    if args.mode == 'predict':
        if args.pb_in and args.json_in:
            parser.error('Only one of --pb-in/--json-in can be specified')

        if not args.pb_in and not args.json_in:
            parser.error('One of --pb-in/--json-in must be specified')

        if args.pb_in and len(args.pb_in) != len(args.pb_in_names):
            parser.error('Number of input files (--pb-in) does not match number of input names (--pb-in-names)')
    
        _main_predict(args)

    elif args.mode == 'provision-model-key':
        if not args.model_key and not args.model_key_file:
            parser.error('One of --model-key-file/--model-key is required for --mode provision-model-key')
    
        _main_provision_model_key(args)
    else:
        assert False    

if __name__ == '__main__':
    main(sys.argv[1:])

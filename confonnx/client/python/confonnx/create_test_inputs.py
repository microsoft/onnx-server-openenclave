# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import argparse
import os
import sys
import json
import numpy as np

THIS_DIR = os.path.dirname(__file__)
sys.path.append(os.path.join(THIS_DIR, os.pardir))

from confonnx.get_model_info import get_model_info

def create_test_inputs(model_info):
    input_info = model_info['inputs']
    inputs = {}
    for name, info in input_info.items():
        shape = info['shape']
        shape = [s if isinstance(s, int) else 1 for s in shape]
        dtype = info['type']
        if dtype.startswith('float'):
            val = np.random.random_sample(shape)
        else:
            dtinfo = np.iinfo(dtype)
            val = np.random.randint(low=dtinfo.min, high=dtinfo.max, size=shape)        
        inputs[name] = {'type': dtype, 'values': val.tolist()}
    return inputs

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', help='Path to ONNX model', required=True)
    parser.add_argument('--out', help='Path to randomly generated inference input data in JSON format',
                        required=True)
    args = parser.parse_args()

    if not os.path.exists(args.model):
        parser.error('model file does not exist')
    
    model_info = get_model_info(args.model)
    data = create_test_inputs(model_info)
    
    if args.out:
        with open(args.out, 'w') as f:
            json.dump(data, f)
    print(f'Data written to {args.out}')

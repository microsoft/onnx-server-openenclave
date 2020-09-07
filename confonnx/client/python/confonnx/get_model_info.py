# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import argparse
import os
import json
import onnxruntime as rt

def get_model_info(model_path):
    sess = rt.InferenceSession(model_path)

    def parse_type_name(io_name, type_name: str):
        prefix = 'tensor('
        suffix = ')'
        if not type_name.startswith(prefix):
            raise RuntimeError(f'unsupported type: {io_name} -> {type_name}')
        dtype = type_name[len(prefix):-len(suffix)]
        if dtype == 'float':
            return 'float32'
        elif dtype == 'double':
            return 'float64'
        else:
            return dtype

    info = {
        'inputs': {i.name: {'shape': i.shape, 'type': parse_type_name(i.name, i.type)}
                   for i in sess.get_inputs()},
        'outputs': {i.name: {'shape': i.shape, 'type': parse_type_name(i.name, i.type)}
                    for i in sess.get_outputs()}
    }
    return info

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('model', help='Path to model')
    parser.add_argument('--out', help='Path to JSON model info file (default: print to console only')
    args = parser.parse_args()

    if not os.path.exists(args.model):
        parser.error('model file does not exist')
    
    info = get_model_info(args.model)
    
    if args.out:
        with open(args.out, 'w') as f:
            json.dump(info, f, indent=2)
    print(json.dumps(info, indent=2))

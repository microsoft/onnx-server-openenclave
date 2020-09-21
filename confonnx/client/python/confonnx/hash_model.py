# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import argparse
import os
import hashlib

def hash_model(model_path) -> str:
    with open(model_path, 'rb') as f:
        model = f.read()
    m = hashlib.sha256()
    m.update(model)
    sha_hex = m.digest().hex()
    return sha_hex

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('model', help='Path to model')
    parser.add_argument('--out', help='Path to computed model hash (default: print to console only')
    args = parser.parse_args()

    if not os.path.exists(args.model):
        parser.error('model file does not exist')
    
    sha = hash_model(args.model)
    
    if args.out:
        with open(args.out, 'w') as f:
            f.write(sha)
    print(sha)

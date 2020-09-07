# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import argparse
import os
import secrets
from Crypto.Cipher import AES

IV_SIZE = 12
SYMMETRIC_KEY_SIZE = 32

def encrypt_model(model_path, encrypted_model_path, key=None, key_path=None) -> None:
    if key_path:
        with open(key_path) as f:
            key = f.read()
    key = bytes.fromhex(key)
    nonce = bytes(IV_SIZE) # zeros
    with open(model_path, 'rb') as f:
        model = f.read()
    cipher = AES.new(key, AES.MODE_GCM, nonce=nonce)
    ciphertext, tag = cipher.encrypt_and_digest(model)
    with open(encrypted_model_path, 'wb') as f:
        f.write(ciphertext)
        f.write(tag)

def generate_key() -> str:
    key = bytearray(secrets.token_bytes(SYMMETRIC_KEY_SIZE))
    # Convert into Curve25517 key.
    # See https://tools.ietf.org/html/rfc8032#section-5.1.5.
    key[0] &= 248
    key[31] &= 127
    key[31] |= 64
    return key.hex()

def generate_key_file(key_path) -> None:
    key = generate_key()
    with open(key_path, 'w') as f:
        f.write(key)

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('model', help='Path to model')
    parser.add_argument('--key', help='Path to key, will be generated if not existing (default: <filename>.key)')
    parser.add_argument('--out', help='Path to encrypted model (default: <filename>.enc)')
    args = parser.parse_args()

    if not os.path.exists(args.model):
        parser.error('model file does not exist')
    if not args.out:
        args.out = os.path.basename(args.model) + '.enc'
    if not args.key:
        args.key = os.path.basename(args.model) + '.key'
    if not os.path.exists(args.key):
        generate_key_file(args.key)
    
    encrypt_model(args.model, args.out, key_path=args.key)

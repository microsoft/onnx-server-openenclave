# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import argparse
from numpy.testing import assert_allclose
import onnx
from onnx import numpy_helper

def load_tensorproto(path):
    with open(path, 'rb') as fp:
        tensor = onnx.TensorProto()
        tensor.ParseFromString(fp.read())
    return numpy_helper.to_array(tensor)

def check_equal(expected_path, actual_path, rtol=1e-5):
    expected = load_tensorproto(expected_path)
    actual = load_tensorproto(actual_path)
    assert_allclose(actual, expected, rtol=rtol)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Compares two TensorProto files')
    parser.add_argument('expected', help='TensorProto file')
    parser.add_argument('actual', help='TensorProto file')
    parser.add_argument('--rtol', default=1e-5)
    args = parser.parse_args()

    check_equal(args.expected, args.actual, args.rtol)

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.

import os
import sysconfig
import shutil
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext

ext_path_rel = os.path.join('confonnx', 'confonnx_py' + sysconfig.get_config_var('EXT_SUFFIX'))
ext_path = os.path.join(os.path.dirname(__file__), ext_path_rel)


class my_build_ext(build_ext):
    ''' Makes the CMake-built Python extension module available to setuptools '''
    def build_extension(self, ext):
        build_temp_path = self.get_ext_fullpath(ext.name)
        print("copying {} -> {}".format(ext_path_rel, build_temp_path))
        shutil.copyfile(ext_path, build_temp_path)

setup(
    name='confonnx',
    version='0.1.0',
    description='TBD',
    packages=['confonnx'],
    ext_modules = [Extension("confonnx.confonnx_py", sources=[])],
    cmdclass = {
        'build_ext': my_build_ext,
    },
    python_requires='>=3.3',
    install_requires=[
        'six',
        'requests',
        'numpy',
        'onnx',
        'onnxruntime',
        'protobuf',
        'tabulate',
        'pycryptodome'
    ]
)
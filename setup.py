# -*- coding: utf-8 -*-
import sys
import os
import os.path
import shutil
from glob import glob

import setuptools
from distutils.core import setup, Extension
from distutils import sysconfig


import platform

build_asan = False

# If we're not on Windows, assume something POSIX-compatible (either Linux, OSX, *BSD or Cygwin) with a working gcc-like compiler
windows = platform.system() == "Windows"

if platform.system() == "Windows":
    assert not build_asan

def get_cflags(asan=False, warnings=True, std_flag=False):
    if windows:
        return ["/O2", "/std:c++20"]
    if asan:
        ret = "-Og -g -std=c++20 -fsanitize=address".split()
        return ret
    res = ["-g", "-O3"]
    if std_flag:
        res.append("-std=c++20")
    if warnings:
        res.extend(["-Wall", "-Wextra"])
    return res


cflags = get_cflags


class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked."""

    def __str__(self):
        try:
            import pybind11

            return pybind11.get_include()
        except ImportError:
            print(
                "pybind11 not found. Please either install it manually, or install via pip rather than through setuptools directly."
            )
            sys.exit(1)


ext_modules = [
    Extension(
        name="opentimspy_cpp",
        sources=[
            #os.path.join("opentims++", "opentims_all.cpp"),
            os.path.join("opentims++", "opentims_pybind11.cpp"),
            #os.path.join("opentims++", "zstd", "zstddeclib.c"),
        ],
        extra_compile_args=get_cflags(asan=False, warnings=False, std_flag=True),
        libraries=[] if windows else "pthread dl".split(),
        include_dirs=[get_pybind_include()],
    )
]


setup(
    name="opentimspy",
    packages=["opentimspy"],
    version="1.0.18",
    author="Mateusz Krzysztof Łącki (MatteoLacki), Michał Startek (michalsta)",
    author_email="matteo.lacki@gmail.com, michal.startek@mimuw.edu.pl",
    description="opentimspy: An open-source parser of Bruker Tims Data File (.tdf).",
    long_description="opentimspy: An open-source parser of Bruker Tims Data File (.tdf).",
    keywords=["timsTOFpro", "Bruker TDF", "data science", "mass spectrometry"],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Science/Research",
        "Topic :: Scientific/Engineering :: Chemistry",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: 3.14",
    ],
    zip_safe=False,
    setup_requires=["pybind11"],
    install_requires=["pybind11", "numpy"],
    ext_modules=ext_modules,
    scripts=glob("scripts/*.py"),
    package_dir={"opentimspy": "opentimspy"},
    package_data={
        "opentimspy": ["opentims++/*.h", "opentims++/*/*.h", "opentims++/*.hpp"]
    },
    extras_require={
        "bruker_proprietary": ["opentims_bruker_bridge>=1.0.3"],
        "plotting": ["matplotlib"],
        "pytest": ["pytest"],
    },
)

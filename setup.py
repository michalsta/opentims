# This Python file uses the following encoding: utf-8
import sys
import os
from os.path import join
from glob import glob

import setuptools
from distutils.core import setup, Extension
from distutils import sysconfig, spawn


class get_pybind_include(object):
    """Helper class to determine the pybind11 include path
    The purpose of this class is to postpone importing pybind11
    until it is actually installed, so that the ``get_include()``
    method can be invoked. """

    def __str__(self):
        try:
            import pybind11
            return pybind11.get_include()
        except ImportError:
            print("pybind11 not found. Please either install it manually, or install via pip rather than through setuptools directly.")
            sys.exit(1)

import platform

build_asan = False

# If we're not on Windows, assume something POSIX-compatible (either Linux, OSX, *BSD or Cygwin) with a working gcc-like compiler
windows = platform.system() == 'Windows'

# Dual-build: work-around for the fact that we have both C and C++ files in the extension, and sometimes need
# to split it into two. Windows and CYGWIN for now seems to need dual_build set to False, OSX to True, Linux seems fine
# with either setting.
dual_build = True

if platform.system() == "Windows":
    assert not build_asan
    dual_build = False
elif platform.system().startswith("CYGWIN"):
    dual_build = False


# Prefer clang if available
if os.getenv('ISO_USE_DEFAULT_CXX') == None and spawn.find_executable('clang++') != None:
    os.environ['CXX'] = 'clang++'


if dual_build:
    ext_modules = [
        Extension(
            name='opentimspy_support',
            sources = [join("opentims++", "sqlite", "sqlite3.c"),
                       join("opentims++", "zstd", "zstddeclib.c")],
            extra_compile_args = ["/O2"] if windows else ["-march=native", "-mtune=native", "-O3", "-ggdb"],
            libraries= '' if windows else 'pthread dl'.split(),
            include_dirs=[get_pybind_include()],
        ),
        Extension(
            name='opentimspy_cpp',
            sources=[join("opentims++","opentims_pybind11.cpp"),
                     join("opentims++", "tof2mz_converter.cpp"),
                     join("opentims++", "scan2inv_ion_mobility_converter.cpp"),],
            extra_compile_args = "-std=c++14 -O3 -march=native -mtune=native -Wall -Wextra -ggdb".split() if not build_asan else "-Og -g -std=c++14 -fsanitize=address".split(),
            libraries='pthread dl'.split(),
            include_dirs=[get_pybind_include()],
            undef_macros = [] if not build_asan else [ "NDEBUG" ]
        )
    ]
else:
    ext_modules = [
        Extension(
            name='opentimspy_cpp',
            sources = [join("opentims++", "sqlite", "sqlite3.c"),
                       join("opentims++", "zstd", "zstddeclib.c"),
                       join("opentims++", "opentims_pybind11.cpp"),
                       join("opentims++", "tof2mz_converter.cpp"),
                       join("opentims++", "scan2inv_ion_mobility_converter.cpp")],
            extra_compile_args = ["/O2"] if windows else ["-march=native", "-mtune=native", "-O3", "-ggdb", "-std=c++14"],
            libraries= '' if windows else 'pthread dl'.split(),
            include_dirs=[get_pybind_include()],
        )]


setup(
    name='opentimspy',
    packages=['opentimspy'],
    version='1.0.4',
    author='Mateusz Krzysztof Łącki (MatteoLacki), Michał Startek (michalsta)',
    author_email='matteo.lacki@gmail.com, michal.startek@mimuw.edu.pl',
    description='opentimspy: An open-source parser of Bruker Tims Data File (.tdf).',
    long_description='opentimspy: An open-source parser of Bruker Tims Data File (.tdf).',
    keywords=['timsTOFpro', 'Bruker TDF', 'data science', 'mass spectrometry'],
    classifiers=["Development Status :: 4 - Beta",
             'Intended Audience :: Science/Research',
             'Topic :: Scientific/Engineering :: Chemistry',
             'Programming Language :: Python :: 3.6',
             'Programming Language :: Python :: 3.7',
             'Programming Language :: Python :: 3.8'],
    setup_requires=['pybind11'],
    install_requires=['pybind11','numpy'],
    ext_modules=ext_modules,
    package_dir={'opentimspy':'opentimspy'},
    extras_require = {
        'bruker_proprietary': ['opentims_bruker_bridge']
    }
)

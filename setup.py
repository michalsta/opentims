# -*- coding: utf-8 -*-
import sys
import os
import os.path
from glob import glob

import setuptools
from distutils.core import setup, Extension
from distutils import sysconfig, spawn



import platform

build_asan = False

# If we're not on Windows, assume something POSIX-compatible (either Linux, OSX, *BSD or Cygwin) with a working gcc-like compiler
windows = platform.system() == 'Windows'

# Dual-build: work-around for the fact that we have both C and C++ files in the extension, and sometimes need
# to split it into two. Windows and CYGWIN for now seems to need dual_build set to False, OSX to True, Linux seems fine
# with either setting.
dual_build = not windows

if platform.system() == "Windows":
    assert not build_asan
    dual_build = False
elif platform.system().startswith("CYGWIN"):
    dual_build = False


native_build = "CIBUILDWHEEL" not in os.environ and 'darwin' not in platform.system().lower() and not 'aarch' in platform.machine().lower()
use_clang = (not windows) and spawn.find_executable('clang++') != None and os.getenv('OPENTIMS_USE_DEFAULT_CXX') == None
#use_ccache = (not windows) and spawn.find_executable('ccache') != None and native_build
use_ccache = os.path.exists("./use_ccache")

# Prefer clang on UNIX if available
if use_clang:
    if use_ccache:
        os.environ['CXX'] = 'ccache g++'
        os.environ['CC'] = 'ccache gcc'
    else:
        os.environ['CXX'] = 'clang++'
else:
    if use_ccache:
        os.environ['CXX'] = 'ccache c++'
        os.environ['CC'] = 'ccache cc'
    else:
        # leave defaults
        pass


def get_cflags(asan=False, warnings=True, std_flag=False):
    if windows:
        return ["/O2"]
    if asan:
        return "-Og -g -std=c++14 -fsanitize=address".split()
    res = ["-g", "-O3"]
    if std_flag:
        res.append("-std=c++14")
    if warnings:
        res.extend(["-Wall", "-Wextra"])
    if native_build:
        res.extend(["-march=native", "-mtune=native"])
    return res

cflags = get_cflags

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


if dual_build:
    ext_modules = [
        Extension(
            name='libopentims_support',
            sources = [os.path.join("opentims++", "sqlite", "sqlite3.c"),
                       os.path.join("opentims++", "zstd", "zstddeclib.c")],
            extra_compile_args = get_cflags(asan=False, warnings=False, std_flag=False),
            libraries= [] if windows else 'pthread dl'.split(),
            include_dirs=[get_pybind_include()],
        ),
        Extension(
            name='opentimspy_cpp',
            sources=[os.path.join("opentims++","opentims_pybind11.cpp"),],
            extra_compile_args = get_cflags(asan=build_asan, std_flag=True),
            libraries= [] if windows else 'pthread dl'.split(),
            include_dirs=[get_pybind_include()],
            undef_macros = [] if not build_asan else [ "NDEBUG" ]
        ),
        Extension(
            name='libopentims_cpp',
            sources=[os.path.join("opentims++","opentims_all.cpp")],
            extra_compile_args = get_cflags(asan=False, std_flag=True),
            libraries= [] if windows else 'pthread dl'.split(),
            )
    ]
else:
    ext_modules = [
        Extension(
            name='opentimspy_cpp',
            sources = [os.path.join("opentims++", "sqlite", "sqlite3.c"),
                       os.path.join("opentims++", "zstd", "zstddeclib.c"),
                       os.path.join("opentims++", "opentims_pybind11.cpp"),],
            extra_compile_args = get_cflags(asan=build_asan, std_flag=True),
            libraries= '' if windows else 'pthread dl'.split(),
            include_dirs=[get_pybind_include()],
        ),
        Extension(
            name='libopentims_cpp',
            sources=[os.path.join("opentims++", "sqlite", "sqlite3.c"),
                     os.path.join("opentims++", "zstd", "zstddeclib.c"),
                     os.path.join("opentims++","libopentims_py.cpp")],
            extra_compile_args = get_cflags(asan=False, std_flag=True),
            libraries= [] if windows else 'pthread dl'.split(),
            )]


setup(
    name='opentimspy',
    packages=['opentimspy'],
    version='1.0.9',
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
             'Programming Language :: Python :: 3.8',
             'Programming Language :: Python :: 3.9'],
    zip_safe=False,
    setup_requires=['pybind11'],
    install_requires=['pybind11','numpy'],
    ext_modules=ext_modules,
    package_dir={'opentimspy':'opentimspy'},
    package_data={'opentimspy':['opentims++/*.h', 'opentims++/*/*.h', 'opentims++/*.hpp']},

    extras_require = {
        'bruker_proprietary': ['opentims_bruker_bridge>=1.0.3']
    }
)

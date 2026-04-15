#    OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
#    Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
#
#    Licensed under the MIT License. See LICENCE file in the project root for details.

import pathlib
import importlib.metadata

import ctypes
import ctypes.util
import sys

import opentimspy.opentimspy_cpp as opentimspy_cpp

libpath = None

if sys.platform == "win32":
    # Prefer Python's own bundled sqlite3.dll over anything in PATH.
    # ctypes.util.find_library searches PATH and may find a wrong-architecture
    # DLL (e.g. AWS CLI ships an x64 sqlite3.dll that fails on ARM64 runners).
    _python_dir = pathlib.Path(sys.executable).parent
    for _candidate in [
        _python_dir / "DLLs" / "sqlite3.dll",
        _python_dir / "sqlite3.dll",
    ]:
        if _candidate.exists():
            libpath = str(_candidate)
            _sqlite3_backend = "Python bundled sqlite3: " + libpath
            break

if libpath is None:
    libpath = ctypes.util.find_library("sqlite3")
    if libpath is not None:
        _sqlite3_backend = "System sqlite3 library: " + libpath

if libpath is None:
    if "_sqlite3" in sys.builtin_module_names:
        # sqlite3 is statically compiled into the Python interpreter
        _sqlite3_backend = "Python builtin (static)"
        libpath = ""
    else:
        try:
            import _sqlite3
            libpath = _sqlite3.__file__
            _sqlite3_backend = "Python's _sqlite3 module: " + libpath
        except ImportError:
            _sqlite3_backend = "Unknown (fallback)"
            libpath = ""

opentimspy_cpp.setup_sqlite_so(libpath)


bruker_bridge_present = False
bruker_bridge_initialized = False

try:
    # Try to find and load Bruker's dll for data conversion
    import opentims_bruker_bridge as obb

    bruker_bridge_present = True
except ImportError:
    pass

if bruker_bridge_present:
    # Try to initialize it
    excuses = []
    so_paths = obb.get_so_paths()
    for so_path in so_paths:
        try:
            opentimspy_cpp.setup_bruker_so(so_path)
            bruker_bridge_initialized = True
            break
        except RuntimeError as e:
            excuses.append(e)
    if not bruker_bridge_initialized:
        errmsg = []
        errmsg.append(
            f"Failed to initialize the Bruker binary library for conversion. {len(excuses)} attempts were made, and here are the reasons they failed:\n\n"
        )
        for so_path, excuse in zip(so_paths, excuses):
            errmsg.append("Path " + so_path + " failed:")
            errmsg.append(str(excuse))
            errmsg.append("")
        errmsg.append("")
        errmsg.append(
            "Please either fix one of the above errors, or uninstall opentims_bruker_bridge module."
        )

        raise ImportError("\n".join(errmsg))


def get_module_dir():
    return pathlib.Path(__file__).parent


from opentimspy.opentims import (
    OpenTIMS,
    column_to_dtype,
    all_columns,
    all_columns_dtype,
    available_columns,
    pressure_compensation_strategy,
    conversion_method,
    setup_opensource,
)


def set_num_threads(n):
    """
    Set the numer of worker threads that OpenTIMS is allowed to use
    during its calculations. Note: a setting of 1 still means OpenTIMS
    might spawn a separate worker thread, just that at most 1 thread
    at a time will be doing heavy computations.

    A setting of 0 means to use all available cores (this is the default
    behaviour)
    """
    opentimspy_cpp.set_num_threads(n)


__version__ = importlib.metadata.version("opentimspy")

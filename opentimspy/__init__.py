#    OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
#    Copyright (C) 2020 Michał Startek and Mateusz Łącki
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License, version 3 only,
#    as published by the Free Software Foundation.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.

import pathlib

try:
    import libopentims_support
    import opentimspy_cpp
except ImportError:
    import ctypes
    import pkgutil
    support_lib = pkgutil.get_loader("libopentims_support")
    ctypes.CDLL(support_lib.get_filename(), ctypes.RTLD_GLOBAL)
    cpp_lib = pkgutil.get_loader("libopentims_cpp")
    ctypes.CDLL(cpp_lib.get_filename(), ctypes.RTLD_GLOBAL)
    import opentimspy_cpp


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
        errmsg.append(f"Failed to initialize the Bruker binary library for conversion. {len(excuses)} attempts were made, and here are the reasons they failed:\n\n")
        for so_path, excuse in zip(so_paths, excuses):
            errmsg.append("Path " + so_path + " failed:")
            errmsg.append(str(excuse))
            errmsg.append("")
        errmsg.append("")
        errmsg.append("Please either fix one of the above errors, or uninstall opentims_bruker_bridge module.")

        raise ImportError('\n'.join(errmsg))


def get_module_dir():
    return pathlib.Path(__file__).parent


from opentimspy.opentims import OpenTIMS

def set_num_threads(n):
    '''
    Set the numer of worker threads that OpenTIMS is allowed to use
    during its calculations. Note: a setting of 1 still means OpenTIMS
    might spawn a separate worker thread, just that at most 1 thread
    at a time will be doing heavy computations.

    A setting of 0 means to use all available cores (this is the default
    behaviour)
    '''
    opentimspy_cpp.set_num_threads(n)

__version__ = "1.0.9"

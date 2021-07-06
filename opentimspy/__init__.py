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


try:
    import opentimspy_cpp
except ImportError:
    import ctypes
    import pkgutil
    support_lib = pkgutil.get_loader("opentimspy_support")
    ctypes.CDLL(support_lib.get_filename(), ctypes.RTLD_GLOBAL)
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

__version__ = "1.0.7"

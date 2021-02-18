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

try:
    # Try to find and load Bruker's dll for data conversion
    import opentims_bruker_bridge as obb
    for so_path in obb.get_so_paths():
        try:
            opentimspy_cpp.setup_bruker_so(so_path)
            bruker_bridge_present = True
            break
        except RuntimeError:
            pass
except ImportError:
    pass


__version__ = "1.0.4"

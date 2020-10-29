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

def parse_idx(x):
    if isinstance(x, slice):
        s = 0 
        if x.start is None:
            s = 0
            if x.stop is None:
                raise IndexError("You need to specify end of the range.")
        else:
            s = x.start
        e = x.stop
        if x.step != None:
            raise Warning("Step is not being consider for now.")
        if s >= e:
            return e, s
        else:
            return s, e  
    elif isinstance(x, int):
        return x, x+1
    elif isinstance(x, list):
        return min(x), max(x)+1
    else:
        raise Warning("General lists not considered for now.")

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

from functools import update_wrapper


class ComfyIter(object):
    """Using ComfyIter enables parsing of range exressions.
    This way, you might do selection with square brackets, e.g. D.iter[1:10, 50:100:2]
    """
    def __init__(self, iterator):
        self.iterator = iterator
        self = update_wrapper(self, iterator)

    def __getitem__(self, x):
        return self.iterator(x)


def infinite_range(start, step):
    i = start
    while True:
        yield i
        i += step


def iter_chunk_ends(start, stop, step=100):
    i_ = start
    _i = i_ + step
    while _i < stop:
        yield i_, _i
        i_ = _i
        _i += step
    yield i_, stop+1

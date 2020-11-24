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

import functools
import numpy as np
import pathlib
import collections

import opentims
from opentims.iterators import ComfyIter
from opentims.slice_ops import parse_idx


class OpenTIMS:
    def __init__ (self, analysis_directory):
        """Initialize TimsData.

            Args:
                analysis_directory (str, unicode string): path to the folder containing 'analysis.tdf'
        """
        analysis_directory = pathlib.Path(analysis_directory)
        assert analysis_directory.exists(), f"There is no such location: {analysis_directory}"
        self.handle = opentims.opentims_cpp.TimsDataHandle(str(analysis_directory))

        self.min_frame = self.handle.min_frame_id()
        self.max_frame = self.handle.max_frame_id()
        self.rts = self.frame2rt(range(self.min_frame, self.max_frame+1))

        # self.iter = ComfyIter(self.iter_arrays)
        self.peaks_cnt = self.handle.no_peaks_total()
        self.columns = ('frame','scan','tof','intensity','mz','dt','rt')
        self.columns_dtypes = (np.uint32,np.uint32,np.uint32,np.uint32,np.double,np.double,np.double)
        self._required_columns = ('scan','tof','intensity') # these are required for C++ code to work: changing that part is not easy.

        self.ms_types = np.array([self.handle.get_frame(i).msms_type 
                                  for i in range(self.min_frame, self.max_frame+1)])

    def __repr__(self):
        return f"OpenTIMS({self.peaks_cnt} peaks)"


    def __del__ (self):
        if hasattr(self, 'handle'):
            del self.handle


    def frame2rt(self, frames):
        frames = np.r_[frames]
        assert frames.min() >= self.min_frame, f"Minimal frame {frames.min()} <= truly minimal {self.min_frame}."
        assert frames.max() <= self.max_frame, f"Maximal frame {frames.max()} <= truly maximal {self.max_frame}."
        return np.array([self.handle.get_frame(i).time for i in frames])


    def peaks_per_frame_cnts(self, frames, convert=True):
        """Return the numbers of peaks in chosen frames.

        Args:
            frames (list): frames to extract the number of peaks from.

        Returns;
            np.array: Number of peaks in each frame.
        """
        if convert:
            frames = np.array(frames, dtype=np.uint32)
        return self.handle.no_peaks_in_frames(frames)


    def _get_empty_arrays(self, size, selected_columns):
        """Return empty numpy arrays that will be filled with delicious data."""
        return [np.empty(shape=size if c in selected_columns or c in self._required_columns else 0,
                         dtype=d)
                for c,d in zip(self.columns, self.columns_dtypes)]


    def _get_dict(self, frames, columns):
        frames = np.array(frames, dtype=np.uint32)
        size   = self.peaks_per_frame_cnts(frames, convert=False)
        arrays = self._get_empty_arrays(size, columns)
        self.handle.extract_frames(frames, *arrays)
        return {c:a for c,a in zip(self.columns, arrays) if c in columns}


    def _get_dict_slices(self, frames_slice, columns):
        start = self.min_frame if frames_slice.start is None else frames_slice.start
        stop  = self.max_frame if frames_slice.stop is None else frames_slice.stop 
        step  = 1 if frames_slice.step is None else frames_slice.step
        size = self.handle.no_peaks_in_slice(start, stop, step)
        arrays = self._get_empty_arrays(size, columns)
        self.handle.extract_frames_slice(start, stop, step, *arrays)
        return {c:a for c,a in zip(self.columns, arrays) if c in columns}        

    def query(self, frames, columns=("rt", "frame", "dt", "scan", "mz", "tof", "intensity")):
        """Get data from a selection of frames.

        Args:
            frames (int, iterable, slice): Frames to choose. Passing an integer results in extracting that one frame.
            columns (tuple): which columns to extract? By default supplying all possible data and their tranformations.

        Returns:
            dict: columnt to numpy array mapping.
        """
        assert all(c in self.columns for c in columns), f"Accepted column names: {self.columns}"

        if isinstance(frames, int):
            frames = [frames]

        if isinstance(frames, slice):
            return self._get_dict_slices(frames, columns)
        else:
            return self._get_dict(frames, columns)


    def query_iter(self, frames, columns=("rt", "frame", "dt", "scan", "mz", "tof", "intensity")):
        """Iterate data from a selection of frames.

        Args:
            frames (int, iterable, slice): Frames to choose. Passing an integer results in extracting that one frame.
            columns (tuple): which columns to extract? By default supplying all possible data and their tranformations.

        Yields:
            dict: columnt to numpy array mapping.
        """
        for fr in frames:
            yield self.query(fr, columns)


    def rt_query(self, min_rt, max_rt, columns=("rt", "frame", "dt", "scan", "mz", "tof", "intensity")):
        """Get data from a selection of frames based on retention times.

        Get all frames corresponding to retention times in a set "[min_rt, max_rt)".


        Args:
            min_rt (float): Minimal retention time.
            max_rt (float): Maximal retention time to choose.
            columns (tuple): which columns to extract? By default: supplying all data.

        Returns:
            dict: column to numpy array mapping.
        """
        min_frame, max_frame = np.searchsorted(self.rts, (min_rt, max_rt))+1 #TODO: check border conditions!!!
        return self.query(slice(min_frame, max_frame), columns)


    def rt_query_iter(self, min_rt, max_rt, columns=("rt", "frame", "dt", "scan", "mz", "tof", "intensity")):
        """Iterate data from a selection of frames based on retention times.

        Get all frames corresponding to retention times in a set "[min_rt, max_rt)".


        Args:
            min_rt (float): Minimal retention time.
            max_rt (float): Maximal retention time to choose.
            columns (tuple): which columns to extract? By default: supplying all data.

        Yields:
            dict: column to numpy array mapping.
        """
        min_frame, max_frame = np.searchsorted(self.rts, (min_rt, max_rt))+1 #TODO: check border conditions!!!
        yield from self.query_iter(range(min_frame, max_frame+1), columns)


    def frame_array(self, frame):
        """Get a 2D array of data for a given frame.

        Args:
            frame (int, iterable, slice): Frame to output.

        Returns:
            np.array: Array with 4 columns: frame numbers, scan numbers, time of flights, and intensities in the selected frame.
        """
        try:
            fr = self.handle.get_frame(frame)
        except IndexError:
            raise IndexError(f"Frame {frame} is not between {self.min_frame} and {self.max_frame}.")
        X = np.empty(shape=(fr.num_peaks, 4), dtype=np.uint32, order='F')
        if fr.num_peaks > 0:
            fr.save_to_pybuffer(X)
        return X


    def frame_arrays(self, frames):
        """Get raw data from a selection of frames.

        Contains only those types that share the underlying type (uint32), which consists of 'frame','scan','tof', and 'intensity'. 

        Args:
            frames (iterable): Frames to chose.
        Returns:
            np.array: raw data from the selection of frames.
        """
        frames = np.array(frames).astype(np.uint32)
        try:
            peaks_cnt = self.handle.no_peaks_in_frames(frames)
            X = np.empty(shape=(peaks_cnt,4), order='F', dtype=np.uint32)
            self.handle.extract_frames(frames, X)
            return X
        except IndexError:
            raise IndexError(f"Some frames are not between {self.min_frame} and {self.max_frame}.")


    def frame_arrays_slice(self, frames_slice):
        """Get raw data from a slice of frames.

        Contains only those types that share the underlying type (uint32), which consists of 'frame','scan','tof', and 'intensity'. 

        Args:
            frames_slice (slice): A slice defining which frames to chose.
        Returns:
            np.array: raw data from the selection of frames.
        """
        start = self.min_frame if frames_slice.start is None else frames_slice.start
        stop  = self.max_frame if frames_slice.stop is None else frames_slice.stop 
        step  = 1 if frames_slice.step is None else frames_slice.step
        peaks_cnt = self.handle.no_peaks_in_slice(start, stop, step)
        X = np.empty(shape=(peaks_cnt, 4), order='F', dtype=np.uint32)
        self.handle.extract_frames_slice(start, stop, step, X)
        return X    


    def iter_arrays(self, frames):
        """Iterate over arrays.

        You can call this method explicitly, but it is more comfortable to use the equivalent self.iter class.

        Args:
            frames (slice,iterable,range): Selection of arrays to extract, specified by: frame and scan numbers.  
        
        Yields:
            np.array: Long representation of the underlying data. The four columns contain: the frame number,
            the scan number, the time of flight index, and the intensity of the selected data-points.
        """
        if isinstance(frames, slice):
            start = 1 if frames.start is None else frames.start
            if frames.stop is None:
                raise IndexError('Need to provide the number of the last frame. Otherwise, use "timspy".')
            if frames.step is None:
                frames = range(start, frames.stop)
            else:
                frames = range(start, frames.stop, frames.step)
        elif isinstance(frames, collections.abc.Iterable):
            pass
        else:
            raise TypeError("x is not a 'slice', nor anything 'iterable'.")
        for f in frames:
            frame = self.frame_array(f)
            if len(frame): 
                yield frame


    def __getitem__(self, frames):
        """Get raw data array for given frames and scans.

        Contains only those types that share the underlying type (uint32), which consists of 'frame','scan','tof', and 'intensity'.
        This function is purely syntactic sugar.
        You should not believe this to be any faster than the query method.

        Args:
            frames (int, slice, iterable): Frames to be collected.

        Returns:
            np.array: Array with 4 columns: frame numbers, scan numbers, time of flights, and intensities in the selected frame.
        """
        if isinstance(frames, int):
            return self.frame_array(frames)
        if isinstance(frames, slice):
            return self.frame_arrays_slice(frames)
        return self.frame_arrays(frames)


    @property
    def MS1_frames(self):
        """Get the indices of frames corresponding to precursors (unfragmented molecules)."""
        return np.arange(self.min_frame, self.max_frame+1)[self.ms_types == 0]

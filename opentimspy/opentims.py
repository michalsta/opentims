#    OpenTIMS: an open-source library for opening Bruker's TimsTOF data files.
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

import collections
import functools
import hashlib
import numpy as np
import pathlib

import opentimspy

from .sql import tables_names, table2dict

all_columns = ('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')
all_columns_dtype = (np.uint32,)*4 + (np.double,)*3

def hash_frame(X,
               columns=('frame','scan','tof','intensity'),
               algo=hashlib.blake2b):
    hashes = []
    for c in columns:
        h = algo()
        h.update(X[c])
        hashes.append(h.digest())
    return hashes



class OpenTIMS:
    def __init__ (self, analysis_directory):
        """Initialize OpenTIMS.

            Args:
                analysis_directory (str, unicode string): path to the folder containing 'analysis.tdf' and 'analysis.tdf_raw'.
        """
        self.analysis_directory = pathlib.Path(analysis_directory)
        self.handle = opentimspy.opentimspy_cpp.TimsDataHandle(str(analysis_directory))
        self.frames = self.table2dict("Frames")
        # make sure it is all sorted by retention time / frame number
        sort_order = np.argsort(self.frames["Id"])
        for column in self.frames:
            self.frames[column] = self.frames[column][sort_order]
        self.GlobalMetadata = self.table2dict("GlobalMetadata")
        self.GlobalMetadata = dict(zip(self.GlobalMetadata['Key'], self.GlobalMetadata['Value']))
        if int(self.GlobalMetadata['TimsCompressionType']) != 2:
            raise RuntimeError(f"Unsupported TimsCompressionType: {self.GlobalMetadata['TimsCompressionType']}. Updating your acquisition software *might* solve the problem.")

        self.min_frame = self.frames['Id'][0]
        self.max_frame = self.frames['Id'][-1]
        # self.ms_types = np.array([self.handle.get_frame(i).msms_type
                                  # for i in range(self.min_frame, self.max_frame+1)])
        self.ms_types = self.frames['MsMsType']
        self.ms1_frames = np.arange(self.min_frame, self.max_frame+1)[self.ms_types == 0]
        self.frames_no = self.max_frame - self.min_frame+1
        self._ms1_mask = np.zeros(self.frames_no, dtype=bool)
        self._ms1_mask[self.ms1_frames-1] = True
        self.min_scan = 1
        self.max_scan = self.frames['NumScans'].max()
        self.min_intensity = 0
        self.max_intensity = self.frames['MaxIntensity'].max()
        self.retention_times = self.frames['Time'] # it's in seconds!
        self.min_retention_time = self.retention_times[0]
        self.max_retention_time = self.retention_times[-1]
        self.min_inv_ion_mobility = float(self.GlobalMetadata['OneOverK0AcqRangeLower'])
        self.max_inv_ion_mobility = float(self.GlobalMetadata['OneOverK0AcqRangeUpper'])
        self.min_mz = float(self.GlobalMetadata['MzAcqRangeLower'])
        self.max_mz = float(self.GlobalMetadata['MzAcqRangeUpper'])
        # self.min_frame = self.handle.min_frame_id()
        # self.max_frame = self.handle.max_frame_id()
        # self.retention_times = self.frame2retention_time(range(self.min_frame, self.max_frame+1))# it's in seconds!
        self.peaks_cnt = self.handle.no_peaks_total()
        self.all_columns = all_columns
        self.all_columns_dtypes = all_columns_dtype

    def __len__(self):
        return self.peaks_cnt


    def __repr__(self):
        return f"OpenTIMS({self.peaks_cnt} peaks)"


    def __del__ (self):
        if hasattr(self, 'handle'):
            del self.handle

    def tables_names(self):
        return tables_names(self.analysis_directory/"analysis.tdf")

    def table2dict(self, name):
        return table2dict(self.analysis_directory/"analysis.tdf", name)

    def frame2retention_time(self, frames):
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


    def _get_empty_arrays(self, size, selected_columns=all_columns):
        """Return a dictionary of empty numpy arrays to be filled with raw data. Some are left empty and thus not filled."""
        assert all(c in self.all_columns for c in selected_columns), f"Accepted column names: {self.all_columns}"

        return {col: np.empty(shape=size if col in selected_columns else 0,
                              dtype=dtype)
                for col, dtype in zip(self.all_columns,
                                      self.all_columns_dtypes)}


    def query(self, frames, columns=all_columns):
        """Get data from a selection of frames.

        Args:
            frames (int, iterable): Frames to choose. Passing an integer results in extracting that one frame.
            columns (tuple): which columns to extract? Defaults to all possible columns.
        Returns:
            dict: columns to numpy array mapping.
        """
        assert all(c in self.all_columns for c in columns), f"Accepted column names: {self.all_columns}"

        try:
            frames = np.r_[frames].astype(np.uint32)
            size   = self.peaks_per_frame_cnts(frames, convert=False)
            arrays = self._get_empty_arrays(size, columns)
            # now, pack the arrays with data
            self.handle.extract_frames(frames, **arrays)
        except RuntimeError as e:
            if e.args[0] == "Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor":
                raise RuntimeError("Please install 'opentims_bruker_bridge' if you want to use Bruker's conversion methods.")
            else:
                raise
        return {c: arrays[c] for c in columns}


    def query_iter(self,
                   frames,
                   columns=all_columns):
        """Iterate data from a selection of frames.

        Args:
            frames (int, iterable, slice): Frames to choose. Passing an integer results in extracting that one frame.
            columns (tuple): which columns to extract? Defaults to all possible columns.
        Yields:
            dict: columnt to numpy array mapping.
        """
        for fr in np.r_[frames]:
            yield self.query(fr, columns)


    def rt_query(self,
                 min_retention_time,
                 max_retention_time,
                 columns=all_columns):
        """Get data from a selection of frames based on retention times.

        Get all frames corresponding to retention times in a set "[min_retention_time, max_retention_time)".
        Retention time is in seconds.


        Args:
            min_retention_time (float): Minimal retention time (in seconds).
            max_retention_time (float): Maximal retention time to choose (in seconds).
            columns (tuple): which columns to extract? Defaults to all possible columns.

        Returns:
            dict: column to numpy array mapping.
        """
        min_frame, max_frame = np.searchsorted(self.retention_times,
                                               (min_retention_time,
                                                max_retention_time)) + 1 #TODO: check border conditions!!!
        return self.query(slice(min_frame, max_frame), columns)


    def rt_query_iter(self,
                      min_retention_time,
                      max_retention_time,
                      columns=all_columns):
        """Iterate data from a selection of frames based on retention times.

        Get all frames corresponding to retention times in a set "[min_retention_time, max_retention_time)".


        Args:
            min_retention_time (float): Minimal retention time.
            max_retention_time (float): Maximal retention time to choose.
            columns (tuple): which columns to extract? Defaults to all possible columns.

        Yields:
            dict: column to numpy array mapping.
        """
        min_frame, max_frame = np.searchsorted(self.retention_times,
                                               (min_retention_time,
                                                max_retention_time)) + 1 #TODO: check border conditions!!!
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

    def get_separate_frames(self, frame_ids, columns = all_columns):
        assert all(c in self.all_columns for c in columns), f"Accepted column names: {self.all_columns}"
        if not isinstance(frame_ids, list):
            frame_ids = list(frame_ids)
        col_b = [col in columns for col in all_columns]
        A = self.handle.extract_separate_frames(frame_ids, *col_b)
        ret = {}
        for ii in range(len(frame_ids)):
            X = {}
            for jj in range(len(all_columns)):
                if col_b[jj]:
                    X[all_columns[jj]] = A[jj][ii]
            ret[frame_ids[ii]] = X
        return ret


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


    def get_hash(self,
                 columns=('frame','scan','tof','intensity'),
                 algo=hashlib.blake2b):
        """Calculate a data-set-wide hash.

        Defaults to raw data that are all uint32.

        Args:
            columns (list): Columns for which to calculate the hash.
            algo (object): Class with a call method for restarting hash calculations and an update method that accepts data.
        Returns:
            binary str: A hash.
        """
        h = algo()
        for X in self.query_iter(frames=slice(self.min_frame,
                                              self.max_frame+1),
                                 columns=columns):
            for c in columns:
                h.update(X[c])
        return h.digest()


    def get_hashes(self,
                   columns=('frame','scan','tof','intensity'),
                   algo=hashlib.blake2b):
        """Calculate a hashes for each frame.

        Defaults to raw data that are all uint32.

        Args:
            columns (list): Columns for which to calculate the hash.
            algo (object): Class with a call method for restarting hash calculations and an update method that accepts data.
        Returns:
            list: Frame specifc hashes.
        """
        return [hash_frame(X, columns, algo)
                for X in self.query_iter(frames=slice(self.min_frame,
                                                      self.max_frame+1),
                                         columns=columns)]

    @functools.lru_cache(maxsize=1)
    def framesTIC(self):
        """Get the Total Ion Current for each frame.

        Returns:
            np.array: Total Ion Current values per each frame (sums of intensities for each frame).
        """
        res = np.empty(shape=self.max_frame, dtype=np.uint32)
        self.handle.per_frame_TIC(res)
        return res

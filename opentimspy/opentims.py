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
from __future__ import annotations

import functools
import hashlib
import pathlib
import sqlite3
import typing
from functools import cached_property

import numpy as np
import numpy.typing as npt
import opentimspy

from .dimension_translations import (
    cast_to_numpy_arrays,
    translate_values_frame_sorted,
    translate_values_frames_not_guaranteed_sorted,
)
from .sql import table2dict, table2keyed_dict, tables_names

all_columns = (
    "frame",
    "scan",
    "tof",
    "intensity",
    "mz",
    "inv_ion_mobility",
    "retention_time",
)
all_columns_dtype = (np.uint32,) * 4 + (np.double,) * 3
column_to_dtype = dict(zip(all_columns, all_columns_dtype))

FRAMES_TYPE = npt.NDArray[np.uint32]
SCANS_TYPE = npt.NDArray[np.uint32]
TOFS_TYPE = npt.NDArray[np.uint32]
INTENSITIES_TYPE = npt.NDArray[np.uint32]

RETENTION_TIMES_TYPE = npt.NDArray[np.float64]
INV_ION_MOBILITIES_TYPE = npt.NDArray[np.float64]
MZS_TYPE = npt.NDArray[np.float64]


def hash_frame(X, columns=("frame", "scan", "tof", "intensity"), algo=hashlib.blake2b):
    hashes = []
    for c in columns:
        h = algo()
        h.update(X[c])
        hashes.append(h.digest())
    return hashes


FramesType = typing.Union[int, typing.Iterable[int]]
COLUMNS_TYPE = typing.Union[str, typing.Tuple[str, ...]]


# TODO: make lazy evaluation for loading sqlite data frames
class OpenTIMS:
    def __init__(self, analysis_directory: str | pathlib.Path):
        """Initialize OpenTIMS.

        Args:
            analysis_directory (str, unicode string): path to the folder containing 'analysis.tdf' and 'analysis.tdf_raw'.
        """
        self.handle = None
        self.analysis_directory = pathlib.Path(analysis_directory)
        if not self.analysis_directory.exists():
            raise RuntimeError(f"No such directory: {str(self.analysis_directory)}")
        if not self.analysis_directory.is_dir():
            raise RuntimeError(f"Is not a directory: {str(self.analysis_directory)}")
        if not (self.analysis_directory / 'analysis.tdf').exists():
            raise RuntimeError(f"Missing: {str(self.analysis_directory / 'analysis.tdf')}")
        if not (self.analysis_directory / 'analysis.tdf_bin').exists():
            raise RuntimeError(f"Missing: {str(self.analysis_directory / 'analysis.tdf_bin')}")
        self.handle = opentimspy.opentimspy_cpp.TimsDataHandle(str(analysis_directory))
        self.GlobalMetadata = self.table2dict("GlobalMetadata")
        self.GlobalMetadata = dict(
            zip(self.GlobalMetadata["Key"], self.GlobalMetadata["Value"])
        )
        if int(self.GlobalMetadata["TimsCompressionType"]) != 2:
            raise RuntimeError(
                f"Unsupported TimsCompressionType: {self.GlobalMetadata['TimsCompressionType']}. Updating your acquisition software *might* solve the problem."
            )
        self.peaks_cnt = self.handle.no_peaks_total()
        self.all_columns = all_columns
        self.all_columns_dtypes = all_columns_dtype

    @property
    def min_inv_ion_mobility(self) -> float:
        return float(self.GlobalMetadata["OneOverK0AcqRangeLower"])

    @property
    def max_inv_ion_mobility(self) -> float:
        return float(self.GlobalMetadata["OneOverK0AcqRangeUpper"])

    @property
    def min_mz(self) -> float:
        return float(self.GlobalMetadata["MzAcqRangeLower"])

    @property
    def max_mz(self) -> float:
        return float(self.GlobalMetadata["MzAcqRangeUpper"])

    @cached_property
    def frames(self) -> dict[str, npt.NDArray]:
        frames = self.table2dict("Frames")
        # make sure it is all sorted by retention time / frame number
        sort_order = np.argsort(frames["Id"])
        for column in frames:
            frames[column] = frames[column][sort_order]
        return frames

    @property
    def retention_times(self) -> RETENTION_TIMES_TYPE:
        """Unit: seconds."""
        return self.frames["Time"]

    @property
    def min_retention_time(self) -> int:
        return self.retention_times[0]

    @property
    def max_retention_time(self) -> int:
        return self.retention_times[-1]

    @property
    def min_frame(self) -> int:
        return self.frames["Id"][0]

    @property
    def max_frame(self) -> int:
        return self.frames["Id"][-1]

    @property
    def frames_no(self) -> int:
        return self.max_frame - self.min_frame + 1

    @property
    def min_scan(self) -> int:
        return 0

    @cached_property
    def max_scan(self) -> int:
        return self.frames["NumScans"].max() - 1

    @property
    def min_intensity(self) -> int:
        return 0

    @cached_property
    def max_intensity(self) -> int:
        return self.frames["MaxIntensity"].max()

    @property
    def ms_types(self) -> npt.NDArray[np.uint32]:
        return self.frames["MsMsType"]

    @cached_property
    def ms1_frames(self) -> FRAMES_TYPE:
        return np.arange(self.min_frame, self.max_frame + 1)[self.ms_types == 0]

    @cached_property
    def _ms1_mask(self) -> npt.NDArray[np.bool_]:
        _ms1_mask = np.zeros(self.frames_no, dtype=bool)
        _ms1_mask[self.ms1_frames - 1] = True
        return _ms1_mask

    @cached_property
    def ms2_frames(self) -> FRAMES_TYPE:
        return np.arange(self.min_frame, self.max_frame + 1)[~self._ms1_mask]

    @cached_property
    def frame_properties(self) -> dict[str, npt.NDArray]:
        return self.table2keyed_dict("Frames")

    def __len__(self):
        return self.peaks_cnt

    def __repr__(self):
        return f"OpenTIMS({self.peaks_cnt:_} peaks)"

    def __del__(self):
        self.close()

    def close(self):
        if not self.handle is None:
            del self.handle
            self.handle = None

    def __enter__(self):
        return self

    def __exit__(self, type, value, traceback):
        self.close()

    def get_sql_connection(self):
        return sqlite3.connect(self.analysis_directory / "analysis.tdf")

    def tables_names(self):
        return tables_names(self.analysis_directory / "analysis.tdf")

    def table2dict(self, name: str):
        return table2dict(self.analysis_directory / "analysis.tdf", name)

    def table2keyed_dict(self, name: str):
        with self.get_sql_connection() as sqlcon:
            return table2keyed_dict(sqlcon, name)

    def frame2retention_time(self, frames: FramesType):
        frames = np.r_[frames]
        assert (
            frames.min() >= self.min_frame
        ), f"Minimal frame {frames.min()} <= truly minimal {self.min_frame}."
        assert (
            frames.max() <= self.max_frame
        ), f"Maximal frame {frames.max()} <= truly maximal {self.max_frame}."
        return np.array([self.handle.get_frame(i).time for i in frames])

    def peaks_per_frame_cnts(self, frames: FramesType, convert=True):
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
        assert all(
            c in self.all_columns for c in selected_columns
        ), f"Accepted column names: {self.all_columns}"

        return {
            col: np.empty(shape=size if col in selected_columns else 0, dtype=dtype)
            for col, dtype in zip(self.all_columns, self.all_columns_dtypes)
        }

    def query(self, frames: FramesType = None, columns: COLUMNS_TYPE = all_columns):
        """Get data from a selection of frames.

        Args:
            frames (int, iterable, None): Frames to choose. Passing an integer results in extracting that one frame. Default: all of them.
            columns (tuple): which columns to extract? Defaults to all possible columns.
        Returns:
            dict: columns to numpy array mapping.
        """
        if isinstance(columns, str):
            columns = (columns,)

        columns = tuple(columns)
        assert all(
            c in self.all_columns for c in columns
        ), f"Accepted column names: {self.all_columns}"

        if frames is None:
            frames = self.frames['Id']

        try:
            frames = np.r_[frames].astype(np.uint32)
            size = self.peaks_per_frame_cnts(frames, convert=False)
            arrays = self._get_empty_arrays(size, columns)
            # now, pack the arrays with data
            self.handle.extract_frames(frames, **arrays)
        except RuntimeError as e:
            if (
                e.args[0]
                == "Default conversion method must be selected BEFORE opening any TimsDataHandles - or it must be passed explicitly to the constructor"
            ):
                raise RuntimeError(
                    "Please install 'opentims_bruker_bridge' if you want to use Bruker's conversion methods."
                )
            else:
                raise
        return {c: arrays[c] for c in columns}

    def query_iter(self, frames: FramesType = None, columns: COLUMNS_TYPE = all_columns):
        """Iterate data from a selection of frames.

        Args:
            frames (int, iterable, slice, None): Frames to choose. Passing an integer results in extracting that one frame. Default: all of them.
            columns (tuple): which columns to extract? Defaults to all possible columns.
        Yields:
            dict: columnt to numpy array mapping.
        """
        if frames is None:
            for frame_id in self.frames['Id']:
                yield self.query(frame_id, columns)
        else:
            for fr in np.r_[frames]:
                yield self.query(fr, columns)

    def __iter__(self):
        yield from self.query_iter()

    def rt_query(
        self,
        min_retention_time: float,
        max_retention_time: float,
        columns: COLUMNS_TYPE = all_columns,
    ):
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
        min_frame, max_frame = (
            np.searchsorted(
                self.retention_times, (min_retention_time, max_retention_time)
            )
            + 1
        )  # TODO: check border conditions!!!
        return self.query(slice(min_frame, max_frame), columns)

    def rt_query_iter(
        self, min_retention_time: float, max_retention_time: float, columns=all_columns
    ):
        """Iterate data from a selection of frames based on retention times.

        Get all frames corresponding to retention times in a set "[min_retention_time, max_retention_time)".


        Args:
            min_retention_time (float): Minimal retention time.
            max_retention_time (float): Maximal retention time to choose.
            columns (tuple): which columns to extract? Defaults to all possible columns.

        Yields:
            dict: column to numpy array mapping.
        """
        min_frame, max_frame = (
            np.searchsorted(
                self.retention_times, (min_retention_time, max_retention_time)
            )
            + 1
        )  # TODO: check border conditions!!!
        yield from self.query_iter(range(min_frame, max_frame + 1), columns)

    def frame_array(self, frame: int):
        """Get a 2D array of data for a given frame.

        Args:
            frame (int, iterable, slice): Frame to output.

        Returns:
            np.array: Array with 4 columns: frame numbers, scan numbers, time of flights, and intensities in the selected frame.
        """
        try:
            fr = self.handle.get_frame(frame)
        except IndexError:
            raise IndexError(
                f"Frame {frame} is not between {self.min_frame} and {self.max_frame}."
            )
        X = np.empty(shape=(fr.num_peaks, 4), dtype=np.uint32, order="F")
        if fr.num_peaks > 0:
            fr.save_to_pybuffer(X)
        return X

    def frame_arrays(self, frames: FramesType):
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
            X = np.empty(shape=(peaks_cnt, 4), order="F", dtype=np.uint32)
            self.handle.extract_frames(frames, X)
            return X
        except IndexError:
            raise IndexError(
                f"Some frames are not between {self.min_frame} and {self.max_frame}."
            )

    def frame_arrays_slice(self, frames_slice):
        """Get raw data from a slice of frames.

        Contains only those types that share the underlying type (uint32), which consists of 'frame','scan','tof', and 'intensity'.

        Args:
            frames_slice (slice): A slice defining which frames to chose.
        Returns:
            np.array: raw data from the selection of frames.
        """
        start = self.min_frame if frames_slice.start is None else frames_slice.start
        stop = self.max_frame if frames_slice.stop is None else frames_slice.stop
        step = 1 if frames_slice.step is None else frames_slice.step
        peaks_cnt = self.handle.no_peaks_in_slice(start, stop, step)
        X = np.empty(shape=(peaks_cnt, 4), order="F", dtype=np.uint32)
        self.handle.extract_frames_slice(start, stop, step, X)
        return X

    def get_separate_frames(self, frame_ids, columns: COLUMNS_TYPE = all_columns):
        assert all(
            c in self.all_columns for c in columns
        ), f"Accepted column names: {self.all_columns}"
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

    def __getitem__(self, frames: FramesType):
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

    def get_hash(
        self,
        columns: COLUMNS_TYPE = ("frame", "scan", "tof", "intensity"),
        algo=hashlib.blake2b,
    ):
        """Calculate a data-set-wide hash.

        Defaults to raw data that are all uint32.

        Args:
            columns (list): Columns for which to calculate the hash.
            algo (object): Class with a call method for restarting hash calculations and an update method that accepts data.
        Returns:
            binary str: A hash.
        """
        h = algo()
        for X in self.query_iter(
            frames=slice(self.min_frame, self.max_frame + 1), columns=columns
        ):
            for c in columns:
                h.update(X[c])
        return h.digest()

    def get_hashes(
        self,
        columns: COLUMNS_TYPE = ("frame", "scan", "tof", "intensity"),
        algo: typing.Callable = hashlib.blake2b,
    ):
        """Calculate a hashes for each frame.

        Defaults to raw data that are all uint32.

        Args:
            columns (list): Columns for which to calculate the hash.
            algo (object): Class with a call method for restarting hash calculations and an update method that accepts data.
        Returns:
            list: Frame specifc hashes.
        """
        return [
            hash_frame(X, columns, algo)
            for X in self.query_iter(
                frames=slice(self.min_frame, self.max_frame + 1), columns=columns
            )
        ]

    def retention_time_to_frame(
        self,
        retention_time: float | npt.NDArray[float] | list[float],
        _buffer: int = 1,
    ) -> np.array:
        """Transform retention times into their corresponding frame numbers.

        We check if retention times are within sensible bounds.

        Arguments:
            retention_time (np.array): An array of retention times.

        Returns:
            np.array: integers, numbers of respective frames (Tims pushes).
        """
        retention_time = np.array(
            retention_time
        )  # if someone passes a float or a pandas.Series
        assert all(
            retention_time <= self.max_retention_time + _buffer
        ), "Some retention times were higher than the latest one."
        res = np.searchsorted(self.retention_times, retention_time)
        return res + 1

    def MS1_retention_time_to_frame(
        self,
        retention_time: np.float | npt.NDArray[float] | list[float],
        _buffer: int = 1,
    ) -> np.array:
        """Transform MS1 retention times into their corresponding frame numbers.

        This is to be sure that we get values only in MS1.
        We check if retention times are within sensible bounds.

        Arguments:
            retention_time (np.array): An array of retention times.
        Returns:
            np.array: integers, numbers of respective frames (Tims pushes).
        """
        # TODO: this allocates extra space and sucks...
        retention_time = np.array(
            retention_time
        )  # if someone passes a float or a pandas.Series
        all_ms1_rts = self.retention_times[self.ms1_frames - 1]
        assert all(
            retention_time <= all_ms1_rts[-1] + _buffer
        ), "Some retention times were higher than the last MS1 one."
        res = np.searchsorted(all_ms1_rts, retention_time)
        return self.ms1_frames[res]

    # TODO: this should be numbized or something: we make a copy of frame-1.
    def frame_to_retention_time(self, frame: FrameType) -> np.array:
        """Transform frames into their corresponding retention times.

        We check if frames are within sensible bounds.

        Arguments:
            frame (np.array): An array of frames.

        Returns:
            np.array: retention time when a frame finishes [second].
        """
        #        assert all(frame >= self.min_frame), "Some frames were below the minimal one."
        #        assert all(frame <= self.max_frame), "Some frames were above the maximal one."
        return self.retention_times[frame - 1]

    def __scan_to_inv_ion_mobility_assertions(
        self,
        scan: np.array,
        frame: np.array,
    ) -> None:
        pass

    #        assert all(scan >= self.min_scan), "Some scans were below the minimal one."
    #        assert all(scan <= self.max_scan), "Some scans were above the maximal one."
    #        assert all(frame >= self.min_frame), "Some frames were below the minimal one."
    #        assert all(frame <= self.max_frame), "Some frames were above the maximal one."

    def scan_to_inv_ion_mobility(
        self,
        scan: np.array,
        frame: np.array,
    ) -> np.array:
        """Transform scans into their corresponding inverse ion mobilities.

        We check if scans are within sensible bounds.

        This works in O(nlog(n)).
        Not happy, well, there's always 'scan_to_inv_ion_mobility_frame_sorted'.

        Arguments:
            scan (np.array): An array of scans.
            frame (np.array): An array of integer scans.

        Returns:
            np.array: inverse ion mobilities [1/k0].
        """
        scan, frame = cast_to_numpy_arrays(scan, frame)
        self.__scan_to_inv_ion_mobility_assertions(scan, frame)
        return translate_values_frames_not_guaranteed_sorted(
            x=scan,
            frame=frame,
            bruker_translator_foo=self.handle.scan_to_inv_mobility,
            x_dtype=np.uint32,
            result_dtype=np.double,
        )

    def scan_to_inv_ion_mobility_frame_sorted(
        self,
        scan: np.array,
        frame: np.array,
    ) -> np.array:
        """Transform scans into their corresponding inverse ion mobilities.

        We check if scans are within sensible bounds.

        This works in O(n).

        Arguments:
            scan (np.array): An array of scans.
            frame (np.array): An array of integer scans.

        Returns:
            np.array: inverse ion mobilities [1/k0].
        """
        scan, frame = cast_to_numpy_arrays(scan, frame)
        self.__scan_to_inv_ion_mobility_assertions(scan, frame)
        return translate_values_frame_sorted(
            x_frame_sorted=scan,
            frame_sorted=frame,
            bruker_translator_foo=self.handle.scan_to_inv_mobility,
            x_dtype=np.uint32,
            result_dtype=np.double,
        )

    def __inv_ion_mobility_to_scan_assertions(
        self,
        inv_ion_mobility: np.array,
        frame: np.array,
        _buffer: float = 0.0,
    ) -> None:
        pass
        # assert all(inv_ion_mobility >= self.min_inv_ion_mobility - _buffer), "Some inverse ion mobilities were below the minimal one."
        # assert all(inv_ion_mobility <= self.max_inv_ion_mobility + _buffer), "Some inverse ion mobilities were above the maximal one."
        # assert all(frame >= self.min_frame), "Some frames were below the minimal one."
        # assert all(frame <= self.max_frame), "Some frames were above the maximal one."

    def inv_ion_mobility_to_scan(
        self,
        inv_ion_mobility: np.array,
        frame: np.array,
        _buffer: float = 0.0,
    ) -> np.array:
        """Transform inverse ion mobilities into their corresponding scan values.

        We check if scans are within sensible bounds.
        Scans correspond to individual emptyings of the second TIMS trap.

        This works in O(nlog(n)).
        Not happy, well, there's always 'inv_ion_mobility_to_scan_frame_sorted'.

        Arguments:
            inv_ion_mobility (np.array): Inverse ion mobility values to translate.
            frame (np.array): An array of integer scans.

        Returns:
            np.array: inverse ion mobilities [1/k0].
        """
        inv_ion_mobility, frame = cast_to_numpy_arrays(inv_ion_mobility, frame)
        self.__inv_ion_mobility_to_scan_assertions(inv_ion_mobility, frame, _buffer)
        return translate_values_frames_not_guaranteed_sorted(
            x=inv_ion_mobility,
            frame=frame,
            bruker_translator_foo=self.handle.inv_mobility_to_scan,
            x_dtype=np.double,
            result_dtype=np.uint32,
        )

    def inv_ion_mobility_to_scan_frame_sorted(
        self,
        inv_ion_mobility: np.array,
        frame: np.array,
        _buffer: float = 0.0,
    ) -> np.array:
        """Transform inverse ion mobilities into their corresponding scan values, assuming frames are sorted.

        We check if scans are within sensible bounds.
        Scans correspond to individual emptyings of the second TIMS trap.

        This works in O(n).

        Arguments:
            inv_ion_mobility (np.array): Inverse ion mobility values to translate.
            frame (np.array): An array of integer scans.

        Returns:
            np.array: inverse ion mobilities [1/k0].
        """
        inv_ion_mobility, frame = cast_to_numpy_arrays(inv_ion_mobility, frame)
        self.__inv_ion_mobility_to_scan_assertions(inv_ion_mobility, frame, _buffer)
        return translate_values_frame_sorted(
            x_frame_sorted=inv_ion_mobility,
            frame_sorted=frame,
            bruker_translator_foo=self.handle.inv_mobility_to_scan,
            x_dtype=np.double,
            result_dtype=np.uint32,
        )

    def __tof_to_mz_assertions(
        self,
        tof: np.array,
        frame: np.array,
    ) -> None:
        assert all(frame >= self.min_frame), "Some frames were below the minimal one."
        assert all(frame <= self.max_frame), "Some frames were above the maximal one."

    def tof_to_mz(self, tof: np.array, frame: np.array) -> np.array:
        """Transform time of flight indices (tof) into their corresponding mass to charge ratios (m/z).

        Caution!
        We do not check if the values are sensible, i.e. if they correspond to meaningful outputs.

        This works in O(nlog(n)).
        Not happy, well, there's always 'tof_to_mz_frame_sorted'.

        Arguments:
            tof (np.array): An array of time of flight integers.
            frame (np.array): An array of integer scans.

        Returns:
            np.array: array of doubles with m/z values.
        """
        tof, frame = cast_to_numpy_arrays(tof, frame)
        self.__tof_to_mz_assertions(tof, frame)
        return translate_values_frames_not_guaranteed_sorted(
            x=tof,
            frame=frame,
            bruker_translator_foo=self.handle.tof_to_mz,
            x_dtype=np.uint32,
            result_dtype=np.double,
        )

    def tof_to_mz_frame_sorted(self, tof: np.array, frame: np.array) -> np.array:
        """Transform time of flight indices (tof) into their corresponding mass to charge ratios (m/z).

        Caution!
        We do not check if the values are sensible, i.e. if they correspond to meaningful outputs.

        This works in O(n).

        Arguments:
            tof (np.array): An array of time of flight integers.
            frame (np.array): An array of integer scans.

        Returns:
            np.array: array of doubles with m/z values.
        """
        tof, frame = cast_to_numpy_arrays(tof, frame)
        self.__tof_to_mz_assertions(tof, frame)
        return translate_values_frame_sorted(
            x_frame_sorted=tof,
            frame_sorted=frame,
            bruker_translator_foo=self.handle.tof_to_mz,
            x_dtype=np.uint32,
            result_dtype=np.double,
        )

    def __mz_to_tof_assertions(
        self,
        mz: np.array,
        frame: np.array,
        _buffer: float,
    ) -> None:
        pass
        # assert all(mz >= self.min_mz - _buffer), "Some m/z values were below the minimal one."
        # assert all(mz <= self.max_mz + _buffer), "Some m/z values were above the maximal one."
        # assert all(frame >= self.min_frame), "Some frames were below the minimal one."
        # assert all(frame <= self.max_frame), "Some frames were above the maximal one."

    def mz_to_tof(
        self,
        mz: np.array,
        frame: np.array,
        _buffer: float = 0.0,
    ) -> np.array:
        """Transform mass to charge ratios (m/z) into their corresponding time of flight indices (tof).

        We check if m/z values are within sensible bounds.
        Time of flight indices are somehow proportional to time of flights.
        We are figuring out how.

        This works in O(nlog(n)).
        Not happy, well, there's always 'mz_to_tof_frame_sorted'.

        Arguments:
            mz (np.array): An array of m/z floats.

        Returns:
            np.array: integer time of flight indices.
        """
        mz, frame = cast_to_numpy_arrays(mz, frame)
        self.__mz_to_tof_assertions(mz, frame, _buffer)
        return translate_values_frames_not_guaranteed_sorted(
            x=mz,
            frame=frame,
            bruker_translator_foo=self.handle.mz_to_tof,
            x_dtype=np.double,
            result_dtype=np.uint32,
        )

    def mz_to_tof_frame_sorted(
        self,
        mz: np.array,
        frame: np.array,
        _buffer: float = 0.0,
    ) -> np.array:
        """Transform mass to charge ratios (m/z) into their corresponding time of flight indices (tof).

        We check if m/z values are within sensible bounds.
        Time of flight indices are somehow proportional to time of flights.
        We are figuring out how.

        This works in O(n).

        Arguments:
            mz (np.array): An array of m/z floats.

        Returns:
            np.array: integer time of flight indices.
        """
        mz, frame = cast_to_numpy_arrays(mz, frame)
        self.__mz_to_tof_assertions(mz, frame, _buffer)
        return translate_values_frame_sorted(
            x_frame_sorted=mz,
            frame_sorted=frame,
            bruker_translator_foo=self.handle.mz_to_tof,
            x_dtype=np.double,
            result_dtype=np.uint32,
        )

    @functools.lru_cache(maxsize=1)
    def framesTIC(self):
        """Get the Total Ion Current for each frame.

        Returns:
            np.array: Total Ion Current values per each frame (sums of intensities for each frame).
        """
        res = np.empty(shape=self.max_frame, dtype=np.uint32)
        self.handle.per_frame_TIC(res)
        return res

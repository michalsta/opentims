"""Parallel decoding: repeated frames and damaged data."""
import shutil
from pathlib import Path

import numpy as np
import pytest

import opentimspy
from opentimspy import OpenTIMS, conversion_method

data_path = Path(__file__).parent / "test.d"
columns = ("frame", "scan", "tof", "intensity", "mz", "inv_ion_mobility", "retention_time")


@pytest.fixture
def four_threads():
    opentimspy.set_num_threads(4)
    yield
    opentimspy.set_num_threads(0)  # back to all detected cores


def test_extract_separate_frames_fills_every_repeat(four_threads):
    ids = [1, 2, 1, 1, 2] * 10
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as D:
        arrays = D.handle.extract_separate_frames(ids, *([True] * len(columns)))
        for ii, frame in enumerate(ids):
            expected = D.query(frame, columns=columns)
            for jj, col in enumerate(columns):
                assert np.array_equal(arrays[jj][ii], expected[col])


def test_get_separate_frames_with_repeated_frames(four_threads):
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as D:
        repeated = D.get_separate_frames([1, 1, 2, 1] * 10)
        once = D.get_separate_frames([1, 2])
    assert list(repeated) == [1, 2]
    for frame in once:
        for col in columns:
            assert np.array_equal(repeated[frame][col], once[frame][col])


def test_query_with_repeated_frames(four_threads):
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as D:
        repeated = D.query([2, 1, 2], columns=columns)
        parts = [D.query(frame, columns=columns) for frame in (2, 1, 2)]
    for col in columns:
        assert np.array_equal(repeated[col], np.concatenate([p[col] for p in parts]))


@pytest.fixture
def damaged_data(tmp_path):
    damaged = tmp_path / "damaged.d"
    shutil.copytree(data_path, damaged)
    bin_path = damaged / "analysis.tdf_bin"
    data = bytearray(bin_path.read_bytes())
    data[8:40] = b"\xff" * 32  # overwrite the start of frame 1's compressed data
    bin_path.write_bytes(bytes(data))
    return damaged


@pytest.mark.parametrize("method", ["query", "get_separate_frames"])
def test_damaged_data_raises_instead_of_crashing(damaged_data, four_threads, method):
    with OpenTIMS(damaged_data, cm=conversion_method.OpenSource) as D:
        with pytest.raises(RuntimeError, match="Error uncompressing frame"):
            getattr(D, method)([1, 2])

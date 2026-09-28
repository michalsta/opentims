"""Regression tests: locale independence, user-supplied arrays, corrupted datasets."""
import locale
import os
import shutil
import sqlite3
import sys
from pathlib import Path

import numpy as np
import pytest

from opentimspy import OpenTIMS, conversion_method

data_path = Path(__file__).parent / "test.d"


@pytest.fixture(scope="module")
def ot():
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as handle:
        yield handle


def modified_dataset(tmp_path, *sql):
    """Copy test.d and apply SQL statements to its analysis.tdf."""
    target = tmp_path / "modified.d"
    shutil.copytree(data_path, target)
    with sqlite3.connect(target / "analysis.tdf") as conn:
        for stmt in sql:
            conn.execute(stmt)
    return target


# --- locale ---

def test_open_neither_changes_nor_depends_on_locale():
    old = locale.setlocale(locale.LC_ALL)
    for candidate in ("pl_PL.UTF-8", "pl_PL.utf8", "de_DE.UTF-8", "de_DE.utf8", "Polish_Poland.1250"):
        try:
            locale.setlocale(locale.LC_ALL, candidate)
            break
        except locale.Error:
            continue
    else:
        pytest.skip("no locale with a decimal comma available")
    try:
        assert locale.localeconv()["decimal_point"] == ","
        with OpenTIMS(data_path, cm=conversion_method.OpenSource) as handle:
            assert locale.setlocale(locale.LC_NUMERIC) != "C"
            q = handle.query(1, columns=("inv_ion_mobility", "retention_time"))
        # "0.600000" parsed as 0 would make the converter factory throw;
        # these are the values pinned in test_opensource.py
        assert np.isclose(q["inv_ion_mobility"][0], 1.5640522875816993)
        assert np.isclose(q["retention_time"][0], 0.644238)
    finally:
        locale.setlocale(locale.LC_ALL, old)


# --- slicing ---

@pytest.mark.parametrize("key", [slice(None), slice(1, None)])
def test_open_ended_slice_includes_last_frame(ot, key):
    assert set(np.unique(ot[key][:, 0])) == set(ot.frames["Id"])


# --- user-supplied output arrays ---

def test_query_rejects_strided_output(ot):
    big = np.zeros((len(ot), 2), dtype=np.uint32)
    with pytest.raises(ValueError, match="contiguous"):
        ot.query(columns={"tof": big[:, 0]})
    assert not big.any()

def test_query_rejects_readonly_output(ot):
    arr = np.zeros(len(ot), dtype=np.uint32)
    arr.flags.writeable = False
    with pytest.raises(ValueError, match="read-only"):
        ot.query(columns={"tof": arr})
    assert not arr.any()

def test_query_rejects_short_output_even_unsanitized(ot):
    with pytest.raises(ValueError, match="too small"):
        ot.query(columns={"tof": np.zeros(len(ot) - 1, dtype=np.uint32)}, _sanitize=False)

def test_query_rejects_wrong_dtype_even_unsanitized(ot):
    with pytest.raises(TypeError, match="dtype"):
        ot.query(columns={"mz": np.zeros(len(ot), dtype=np.float32)}, _sanitize=False)

def test_query_accepts_valid_output(ot):
    arr = np.zeros(len(ot), dtype=np.uint32)
    ot.query(columns={"tof": arr})
    assert np.array_equal(arr, ot.query(columns="tof")["tof"])


# --- converter inputs ---

def test_converters_handle_strided_input(ot):
    q = ot.query(columns=("frame", "scan", "tof"))
    for method, x in [(ot.tof_to_mz, q["tof"]), (ot.scan_to_inv_ion_mobility, q["scan"])]:
        assert np.array_equal(method(x[::2], q["frame"][::2]),
                              method(x[::2].copy(), q["frame"][::2].copy()))


# --- retention time helpers ---

def test_retention_time_to_frame_scalar(ot):
    assert ot.retention_time_to_frame(ot.min_retention_time) == ot.min_frame

def test_retention_time_to_frame_within_buffer_past_end(ot):
    assert ot.retention_time_to_frame([ot.max_retention_time + 0.5])[0] == ot.max_frame

def test_ms1_retention_time_to_frame_within_buffer_past_end(ot):
    last_ms1_rt = ot.retention_times[ot.ms1_frames[-1] - 1]
    assert ot.MS1_retention_time_to_frame([last_ms1_rt + 0.5])[0] == ot.ms1_frames[-1]
    assert ot.MS1_retention_time_to_frame(last_ms1_rt) == ot.ms1_frames[-1]


# --- corrupted datasets: exceptions, not crashes or silent garbage ---

@pytest.mark.parametrize("sql", [
    "UPDATE Frames SET TimsId = 1000000000 WHERE Id = 2",  # beyond end of tdf_bin
    "UPDATE Frames SET NumPeaks = 3 WHERE Id = 2",         # fewer than stored
    "UPDATE Frames SET NumPeaks = 7 WHERE Id = 2",         # more than stored
])
def test_corrupted_frame_raises(tmp_path, sql):
    with OpenTIMS(modified_dataset(tmp_path, sql), cm=conversion_method.OpenSource) as handle:
        assert len(handle.query(1)["tof"]) == 5
        with pytest.raises(RuntimeError, match="Frame 2"):
            handle.query(2)
        with pytest.raises(RuntimeError, match="Frame 2"):
            handle.frame_array(2)
        # multithreaded path: must propagate, not std::terminate
        with pytest.raises(RuntimeError, match="Frame 2"):
            handle.get_separate_frames([1, 2] * 50)

def test_failed_extraction_leaves_frame_consistent():
    with OpenTIMS(data_path, cm=conversion_method.NoConversion) as handle:
        expected = handle.query(1, columns="tof")["tof"]
        with pytest.raises(RuntimeError):
            handle.query(1, columns="mz")
        handle.query(2, columns="tof")  # reuses the shared decompression buffer
        assert np.array_equal(handle.query(1, columns="tof")["tof"], expected)


@pytest.mark.skipif(not sys.platform.startswith("linux"), reason="counts /proc/self/fd")
def test_failed_open_does_not_leak_file_descriptors(tmp_path):
    path = modified_dataset(tmp_path, "UPDATE GlobalMetadata SET Value = '1' WHERE Key = 'TimsCompressionType'")
    before = len(os.listdir("/proc/self/fd"))
    for _ in range(10):
        with pytest.raises(RuntimeError, match="Compression algorithm"):
            OpenTIMS(path)
    assert len(os.listdir("/proc/self/fd")) == before

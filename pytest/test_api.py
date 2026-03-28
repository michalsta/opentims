"""Tests for OpenTIMS Python API — properties, slicing, TIC, error paths."""
from pathlib import Path

import numpy as np
import pytest

from opentimspy import OpenTIMS, setup_opensource

data_path = Path(__file__).parent / "test.d"


@pytest.fixture(scope="module")
def ot():
    setup_opensource()
    with OpenTIMS(data_path) as handle:
        yield handle


# --- frame metadata properties ---

def test_min_frame(ot):
    assert ot.min_frame >= 1

def test_max_frame_gte_min(ot):
    assert ot.max_frame >= ot.min_frame

def test_frames_no(ot):
    assert ot.frames_no == ot.max_frame - ot.min_frame + 1

def test_min_scan_is_zero(ot):
    assert ot.min_scan == 0

def test_max_scan_positive(ot):
    assert ot.max_scan > 0

def test_retention_times_length(ot):
    assert len(ot.retention_times) == ot.frames_no

def test_retention_times_monotone(ot):
    assert np.all(np.diff(ot.retention_times) >= 0)

def test_ms1_ms2_cover_all_frames(ot):
    all_frames = set(range(ot.min_frame, ot.max_frame + 1))
    covered = set(ot.ms1_frames) | set(ot.ms2_frames)
    assert covered == all_frames

def test_ms1_ms2_disjoint(ot):
    assert len(set(ot.ms1_frames) & set(ot.ms2_frames)) == 0


# --- framesTIC ---

def test_framesthetic_length(ot):
    tic = ot.framesTIC()
    assert len(tic) == ot.frames_no

def test_framesthetic_dtype(ot):
    assert ot.framesTIC().dtype == np.uint32

def test_framesthetic_nonnegative(ot):
    assert np.all(ot.framesTIC() >= 0)

def test_framesthetic_nonzero(ot):
    # test data has peaks, so at least one frame should have TIC > 0
    assert np.any(ot.framesTIC() > 0)


# --- frame_array ---

def test_frame_array_shape(ot):
    X = ot.frame_array(ot.min_frame)
    assert X.ndim == 2
    assert X.shape[1] == 4

def test_frame_array_dtype(ot):
    X = ot.frame_array(ot.min_frame)
    assert X.dtype == np.uint32

def test_frame_array_invalid_raises(ot):
    with pytest.raises(IndexError):
        ot.frame_array(ot.max_frame + 1)


# --- frame_arrays_slice ---

def test_frame_arrays_slice_all(ot):
    X = ot.frame_arrays_slice(slice(None, None))
    assert X.shape[1] == 4
    assert X.dtype == np.uint32

def test_frame_arrays_slice_single(ot):
    X_single = ot.frame_arrays_slice(slice(ot.min_frame, ot.min_frame + 1))
    X_array = ot.frame_array(ot.min_frame)
    assert X_single.shape == X_array.shape
    assert np.array_equal(np.sort(X_single, axis=0), np.sort(X_array, axis=0))

def test_frame_arrays_slice_step_zero_raises(ot):
    with pytest.raises(RuntimeError):
        ot.frame_arrays_slice(slice(ot.min_frame, ot.max_frame, 0))

def test_frame_arrays_slice_empty(ot):
    # end < start yields empty result, not a crash
    X = ot.frame_arrays_slice(slice(ot.max_frame, ot.min_frame))
    assert X.shape[0] == 0


# --- query ---

def test_query_all_columns_returns_dict(ot):
    result = ot.query(columns=("frame", "scan", "tof", "intensity"))
    assert isinstance(result, dict)
    assert set(result.keys()) == {"frame", "scan", "tof", "intensity"}

def test_query_lengths_consistent(ot):
    result = ot.query(columns=("frame", "scan", "tof", "intensity"))
    lengths = {len(v) for v in result.values()}
    assert len(lengths) == 1

def test_query_frame_values_in_range(ot):
    result = ot.query(columns=("frame",))
    assert np.all(result["frame"] >= ot.min_frame)
    assert np.all(result["frame"] <= ot.max_frame)

def test_query_scan_values_in_range(ot):
    result = ot.query(columns=("scan",))
    assert np.all(result["scan"] >= ot.min_scan)
    assert np.all(result["scan"] <= ot.max_scan)

def test_query_intensity_positive(ot):
    result = ot.query(columns=("intensity",))
    assert np.all(result["intensity"] > 0)

def test_query_single_frame(ot):
    result_single = ot.query(ot.min_frame, columns=("frame", "scan", "tof", "intensity"))
    assert np.all(result_single["frame"] == ot.min_frame)


# --- query_iter ---

def test_query_iter_concatenates_to_query(ot):
    cols = ("frame", "scan", "tof", "intensity")
    from_query = ot.query(columns=cols)
    chunks = list(ot.query_iter(columns=cols))
    assert len(chunks) > 0
    concatenated = {c: np.concatenate([ch[c] for ch in chunks]) for c in cols}
    for c in cols:
        assert np.array_equal(from_query[c], concatenated[c])


# --- context manager ---

def test_context_manager_closes():
    setup_opensource()
    with OpenTIMS(data_path) as handle:
        assert handle.handle is not None
    assert handle.handle is None

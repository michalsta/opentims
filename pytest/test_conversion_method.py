from opentimspy import OpenTIMS, conversion_method, setup_opensource, bruker_bridge_present
import numpy as np
import pandas as pd
from io import StringIO
from pathlib import Path
import pytest

data_path = Path(__file__).parent / "test.d"

oss_test_data = pd.read_csv(StringIO("""
frame,scan,tof,intensity,mz,inv_ion_mobility,retention_time
1,33,268877,10,796.9985194813775,1.5640522875816993,0.644238
1,49,36849,10,99.42108214671732,1.5466230936819172,0.644238
1,53,356994,87,1236.633340924859,1.5422657952069718,0.644238
1,59,366925,10,1292.2118408784838,1.5357298474945535,0.644238
1,62,103614,80,231.8093673885497,1.5324618736383444,0.644238
2,54,399169,10,1481.0866010361667,1.5411764705882354,0.751175
2,56,205746,51,541.1613947631356,1.5389978213507627,0.751175
2,73,205657,10,540.8355715906617,1.520479302832244,0.751175
2,80,66533,10,151.46433815861303,1.5128540305010894,0.751175
2,83,315625,10,1018.2569352019063,1.5095860566448802,0.751175
"""))


# --- enum existence and values ---

def test_conversion_method_has_default():
    assert hasattr(conversion_method, "Default")

def test_conversion_method_has_bruker():
    assert hasattr(conversion_method, "Bruker")

def test_conversion_method_has_opensource():
    assert hasattr(conversion_method, "OpenSource")

def test_conversion_method_has_noconversion():
    assert hasattr(conversion_method, "NoConversion")

def test_conversion_method_values_are_distinct():
    vals = {conversion_method.Default, conversion_method.Bruker,
            conversion_method.OpenSource, conversion_method.NoConversion}
    assert len(vals) == 4


# --- default (cm=None) backward-compat: behaves like old API ---

def test_default_cm_none_opens_without_error():
    """cm=None (the default) should work exactly like the old API."""
    setup_opensource()
    with OpenTIMS(data_path) as OT:
        raw = OT.query(columns=("frame", "scan", "tof", "intensity"))
    assert len(raw["frame"]) > 0


def test_default_cm_none_matches_old_api():
    """cm=None and cm=Default should give identical results."""
    setup_opensource()
    with OpenTIMS(data_path) as OT_none:
        res_none = OT_none.query(columns=("frame", "scan", "tof", "intensity", "mz", "inv_ion_mobility", "retention_time"))
    with OpenTIMS(data_path, cm=conversion_method.Default) as OT_default:
        res_default = OT_default.query(columns=("frame", "scan", "tof", "intensity", "mz", "inv_ion_mobility", "retention_time"))
    for col in res_none:
        assert np.array_equal(res_none[col], res_default[col]), f"Mismatch in column {col}"


# --- explicit OpenSource ---

def test_explicit_opensource_returns_correct_values():
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as OT:
        frame = OT.query(columns=("frame", "scan", "tof", "intensity", "mz", "inv_ion_mobility", "retention_time"))
    pddf = pd.DataFrame(frame)
    assert np.allclose(pddf.mz, oss_test_data.mz, atol=1e-6)
    assert np.allclose(pddf.inv_ion_mobility, oss_test_data.inv_ion_mobility, atol=1e-6)
    assert np.allclose(pddf.retention_time, oss_test_data.retention_time, atol=1e-6)


def test_explicit_opensource_raw_columns_match():
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as OT:
        frame = OT.query(columns=("frame", "scan", "tof", "intensity"))
    pddf = pd.DataFrame(frame)
    assert pddf.frame.equals(oss_test_data.frame.astype(np.uint32))
    assert pddf.scan.equals(oss_test_data.scan.astype(np.uint32))
    assert pddf.tof.equals(oss_test_data.tof.astype(np.uint32))
    assert pddf.intensity.equals(oss_test_data.intensity.astype(np.uint32))


# --- NoConversion ---

def test_noconversion_raw_columns_work():
    """NoConversion should still allow reading raw uint32 columns."""
    with OpenTIMS(data_path, cm=conversion_method.NoConversion) as OT:
        frame = OT.query(columns=("frame", "scan", "tof", "intensity"))
    assert len(frame["frame"]) > 0
    assert frame["frame"].dtype == np.uint32


def test_noconversion_mz_raises():
    """NoConversion should error when requesting converted columns."""
    with OpenTIMS(data_path, cm=conversion_method.NoConversion) as OT:
        with pytest.raises(RuntimeError):
            OT.query(columns=("mz",))


def test_noconversion_inv_ion_mobility_raises():
    """NoConversion should error when requesting inv_ion_mobility."""
    with OpenTIMS(data_path, cm=conversion_method.NoConversion) as OT:
        with pytest.raises(RuntimeError):
            OT.query(columns=("inv_ion_mobility",))


# --- Bruker ---

@pytest.mark.skipif(
    not bruker_bridge_present,
    reason="Bruker bridge not installed",
)
def test_explicit_bruker_opens():
    with OpenTIMS(data_path, cm=conversion_method.Bruker) as OT:
        frame = OT.query(columns=("frame", "scan", "tof", "intensity", "mz", "inv_ion_mobility", "retention_time"))
    assert len(frame["mz"]) > 0


@pytest.mark.skipif(
    bruker_bridge_present,
    reason="Bruker bridge IS present — cannot test the error path",
)
def test_explicit_bruker_without_bridge_raises():
    """Requesting Bruker when bridge is not initialized should raise."""
    with pytest.raises(RuntimeError, match="Bruker"):
        OpenTIMS(data_path, cm=conversion_method.Bruker)


# --- mixing conversion methods across datasets ---

def test_two_datasets_different_methods():
    """Open two handles with different conversion methods simultaneously."""
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as ot_oss:
        with OpenTIMS(data_path, cm=conversion_method.NoConversion) as ot_no:
            raw_oss = ot_oss.query(columns=("frame", "scan", "tof", "intensity"))
            raw_no = ot_no.query(columns=("frame", "scan", "tof", "intensity"))
    # raw columns must be identical regardless of conversion method
    for col in ("frame", "scan", "tof", "intensity"):
        assert np.array_equal(raw_oss[col], raw_no[col]), f"Mismatch in {col}"


def test_opensource_and_noconversion_side_by_side():
    """OpenSource handle can read mz while NoConversion handle cannot."""
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as ot_oss:
        mz = ot_oss.query(columns=("mz",))
        assert len(mz["mz"]) > 0

    with OpenTIMS(data_path, cm=conversion_method.NoConversion) as ot_no:
        with pytest.raises(RuntimeError):
            ot_no.query(columns=("mz",))

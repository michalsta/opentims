"""Lookup tables for Bruker's m/z and inverse ion mobility conversion."""
from pathlib import Path

import numpy as np
import pytest

import opentimspy
from opentimspy import OpenTIMS, bruker_bridge_present, conversion_method

data_path = Path(__file__).parent / "test.d"
columns = ("mz", "inv_ion_mobility")


def test_lookup_tables_need_bruker():
    with OpenTIMS(data_path, cm=conversion_method.OpenSource) as D:
        with pytest.raises(RuntimeError, match="need Bruker's conversion"):
            D.use_mz_lookup()
        with pytest.raises(RuntimeError, match="need Bruker's conversion"):
            D.use_inv_ion_mobility_lookup()


@pytest.mark.skipif(not bruker_bridge_present, reason="Bruker bridge not present")
def test_lookup_tables_with_bruker():
    with OpenTIMS(data_path, cm=conversion_method.Bruker) as D:
        exact = {f: D.query(f, columns=columns) for f in (1, 2)}
        D.use_mz_lookup()
        D.use_inv_ion_mobility_lookup()
        frame1, frame2 = D.query(1, columns=columns), D.query(2, columns=columns)
        for col in columns:
            assert np.array_equal(frame1[col], exact[1][col])
        assert np.allclose(frame2["mz"], exact[2]["mz"], rtol=1e-6, atol=0)
        assert np.array_equal(frame2["inv_ion_mobility"], exact[2]["inv_ion_mobility"])

        try:
            opentimspy.set_num_threads(1)
            sequential = D.query([1, 2])
            opentimspy.set_num_threads(2)
            parallel = D.query([1, 2])
        finally:
            opentimspy.set_num_threads(0)
        for col in sequential:
            assert np.array_equal(sequential[col], parallel[col])

        D.use_mz_lookup(2)
        assert np.array_equal(D.query(2, columns="mz")["mz"], exact[2]["mz"])
        with pytest.raises(ValueError, match="no frame 99999"):
            D.use_mz_lookup(99999)
        assert np.array_equal(D.query(2, columns="mz")["mz"], exact[2]["mz"])

        D.use_mz_lookup(None)
        D.use_inv_ion_mobility_lookup(None)
        for frame in (1, 2):
            again = D.query(frame, columns=columns)
            for col in columns:
                assert np.array_equal(again[col], exact[frame][col])

from opentimspy import OpenTIMS, available_columns, bruker_bridge_present, pressure_compensation_strategy
import pandas as pd
import numpy as np
from io import StringIO
from pathlib import Path

test_data = pd.read_csv(StringIO("""
row_id,frame,scan,tof,intensity,mz,inv_ion_mobility,retention_time
0,1,33,268877,10,796.9921854479367,1.6011076513865186,0.644238
1,1,49,36849,10,99.41965089967422,1.5838461886003579,0.644238
2,1,53,356994,87,1236.6336533434433,1.579526227061543,0.644238
3,1,59,366925,10,1292.2121019027802,1.573042833097905,0.644238
4,1,62,103614,80,231.80534833156824,1.5697995817954047,0.644238
0,2,54,399169,10,1481.0877220591449,1.5784459491154754,0.751175
1,2,56,205746,51,541.1568097825563,1.5762850480210568,0.751175
2,2,73,205657,10,540.8309909167342,1.5578987855166206,0.751175
3,2,80,66533,10,151.46216019386975,1.550318282159824,0.751175
4,2,83,315625,10,1018.2541692375131,1.5470677622680686,0.751175
"""))

pressure_compensated_test_data = pd.read_csv(StringIO("""
frame,scan,tof,intensity,mz,inv_ion_mobility,retention_time
1,33,268877,10,796.9921854479367,1.612076096427468,0.644238
1,49,36849,10,99.41965089967422,1.5946963833751666,0.644238
1,53,356994,87,1236.6336533434433,1.5903468277858357,0.644238
1,59,366925,10,1292.2121019027802,1.583819019100734,0.644238
1,62,103614,80,231.80534833156824,1.5805535497895726,0.644238
2,54,399169,10,1481.0877220591449,1.5892591493571893,0.751175
2,56,205746,51,541.1568097825563,1.587083444932793,0.751175
2,73,205657,10,540.8309909167342,1.56857122668165,0.751175
2,80,66533,10,151.46216019386975,1.5609387928163831,0.751175
2,83,315625,10,1018.2541692375131,1.5576660051222373,0.751175
"""))

data_path = Path(__file__).parent / "test.d"

import pytest


@pytest.mark.skipif(
    not bruker_bridge_present,
    reason="Bruker bridge not present, so no Bruker conversion available",
)
def test_bruker():
    assert bruker_bridge_present
    with OpenTIMS(data_path, pcs=pressure_compensation_strategy.NoPressureCompensation) as OT:
        frame = OT.query(columns=available_columns)
    pddf = pd.DataFrame(frame)
    assert pddf.frame.equals(test_data.frame.astype(np.uint32))
    assert pddf.scan.equals(test_data.scan.astype(np.uint32))
    assert pddf.tof.equals(test_data.tof.astype(np.uint32))
    assert pddf.intensity.equals(test_data.intensity.astype(np.uint32))
    assert np.allclose(pddf.mz, test_data.mz, atol=1e-6)
    assert np.allclose(pddf.inv_ion_mobility, test_data.inv_ion_mobility, atol=1e-6)
    assert np.allclose(pddf.retention_time, test_data.retention_time, atol=1e-6)

@pytest.mark.skipif(
    not bruker_bridge_present,
    reason="Bruker bridge not present, so no Bruker conversion available",
)
def test_bruker_pressure_compensation():
    assert bruker_bridge_present
    with OpenTIMS(data_path, pcs=pressure_compensation_strategy.PerFramePressureCompensation) as OT:
        frame = OT.query(columns=available_columns)
    pddf = pd.DataFrame(frame)
    assert pddf.frame.equals(pressure_compensated_test_data.frame.astype(np.uint32))
    assert pddf.scan.equals(pressure_compensated_test_data.scan.astype(np.uint32))
    assert pddf.tof.equals(pressure_compensated_test_data.tof.astype(np.uint32))
    assert pddf.intensity.equals(pressure_compensated_test_data.intensity.astype(np.uint32))
    assert np.allclose(pddf.mz, pressure_compensated_test_data.mz, atol=1e-6)
    assert np.allclose(pddf.inv_ion_mobility, pressure_compensated_test_data.inv_ion_mobility, atol=1e-6)
    assert np.allclose(pddf.retention_time, pressure_compensated_test_data.retention_time, atol=1e-6)

if __name__ == "__main__":
    test_bruker()

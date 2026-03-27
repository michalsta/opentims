from opentimspy import OpenTIMS, setup_opensource
import pandas as pd
import numpy as np
from io import StringIO
from pathlib import Path
import pytest

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

data_path = Path(__file__).parent / "test.d"


def test_opensource():
    setup_opensource()
    with OpenTIMS(data_path) as OT:
        frame = OT.query(columns=("frame", "scan", "tof", "intensity", "mz", "inv_ion_mobility", "retention_time"))
    pddf = pd.DataFrame(frame)
    assert pddf.frame.equals(oss_test_data.frame.astype(np.uint32))
    assert pddf.scan.equals(oss_test_data.scan.astype(np.uint32))
    assert pddf.tof.equals(oss_test_data.tof.astype(np.uint32))
    assert pddf.intensity.equals(oss_test_data.intensity.astype(np.uint32))
    assert np.allclose(pddf.mz, oss_test_data.mz, atol=1e-6)
    assert np.allclose(pddf.inv_ion_mobility, oss_test_data.inv_ion_mobility, atol=1e-6)
    assert np.allclose(pddf.retention_time, oss_test_data.retention_time, atol=1e-6)


if __name__ == "__main__":
    test_opensource()

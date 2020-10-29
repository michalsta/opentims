import opentims
from test_dir.test import OpenTIMS
from test_dir.hashing import hash_frame, hash_dataset

from opentims import OpenTIMS
import pandas as pd

path = pathlib.Path('~/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()

path = pathlib.Path('~/Projects/bruker/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()
path.exists()

D = OpenTIMS(path)
X = D[1:10]
pd.DataFrame(X, columns=['frame', 'scan', 'tof', 'intensity'])

from timspy import TimsDF

D = TimsDF(path)
X = D[1:10]
import pathlib
from opentims.opentims import OpenTIMS
import pandas as pd

path = pathlib.Path('~/Projects/bruker/BrukerMIDIA/MIDIA_CE10_precursor/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()
# path = pathlib.Path('~/Projects/bruker/20190912_HeLa_Bruker_TEN_MIDIA_200ng_CE10_100ms_Slot1-9_1_488.d').expanduser().resolve()
path.exists()

D = OpenTIMS(path)
pd.DataFrame(D[1:10], columns=['frame', 'scan', 'tof', 'intensity'])
pd.DataFrame(D.query_slice(1,10))
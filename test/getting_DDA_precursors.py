# %load_ext autoreload
# %autoreload 2
# %load_ext snakeviz
import numpy as np
import opentimspy
import pandas as pd

pd.set_option("display.max_columns", None)
import matplotlib.pyplot as plt

dda_data = opentimspy.OpenTIMS(
    "/run/media/matteo/245AD66200B5FCB7/DDA/O220923_002_S1-B1_1_4957.d"
)

Precursors = pd.DataFrame(dda_data.table2dict("Precursors")).set_index("Id")
PasefFrameMsMsInfo = pd.DataFrame(dda_data.table2dict("PasefFrameMsMsInfo")).set_index(
    "Precursor"
)

columns = ("frame", "mz", "intensity")
for col in ("scan", "mz"):
    if col not in columns:
        columns += (col,)

positions = PasefFrameMsMsInfo.loc[10]
data = dda_data.query(positions.Frame, columns=columns)
data_df = pd.DataFrame(data)

width_mz = positions.IsolationWidth.max()
min_mz = positions.IsolationMz.min() - width_mz
max_mz = positions.IsolationMz.max() + width_mz
data_df.query("@max_mz <= mz <= @min_mz")


positions.Frame.isin(dda_data.ms1_frames)


test = PasefFrameMsMsInfo.groupby("Precursor").nunique()
np.all(test.iloc[:, 1:] == 1)


PasefFrameMsMsInfo.loc[[1, 2]]
np.any(PasefFrameMsMsInfo.Frame.isin(dda_data.ms1_frames))


np.diff(PasefFrameMsMsInfo.Frame.to_numpy()) > 0

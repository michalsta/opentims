"""Here we test if the parameters estimated on one data-set apply to another data-set."""


%load_ext autoreload
%autoreload 2
import numpy as np
import pathlib
import pandas as pd
pd.set_option('display.max_columns', None)

from timspy.timspy import TimsPyDF

paths = [p.parent for p in pathlib.Path('/data/bruker').glob('*/*.tdf')]
probs = [0,.01,.1,.99,.999,1]
quantiles = []
errors = []
params = -124068.17347764065, 12730.865914763803, -7.486911887053376e-07
# path = paths[0]

for path in paths:
    try:
        SE = SimpleEstimator(path)
        Y = pd.concat(SE.res(*params))
        quantiles.append(np.quantile(Y.mz - Y.mz_est, probs))
    except Exception as e:
        errors.append(e)

computed = paths[:len(quantiles)]

quantiles = np.array(quantiles)


SE = SimpleEstimator(path)
        Y = pd.concat(SE.res(*params))
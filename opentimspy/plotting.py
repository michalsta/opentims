import numpy as np
import numpy.typing as npt
# numba is optional: will be used if present
try:
    from numba import jit
except ImportError:
    jit = lambda x: x


@jit
def make_dense(
        X : npt.NDArray[np.uint32],
        Y : npt.NDArray[np.uint32],
        intens : npt.NDArray[np.uint32],
        xmax : np.uint32,
        ymax : np.uint32,
    ):
    A = np.zeros(shape=(xmax,ymax), dtype=np.uint32)
    for ii in range(len(X)):
        A[X[ii], Y[ii]] += intens[ii]

    return A

def float_to_idx(
        A : npt.NDArray[np.float64],
        min_val : np.float64,
        resolution : np.float64,
        **kwargs
        ):
    return np.uint32((A-min_val) * (1.0/resolution) + 0.5)

def int_to_idx(
        A: npt.NDArray[np.uint32],
        min_val : np.uint32,
        **kwargs
        ):
    if min_val == 0:
        return A
    return A - min_val

transformations = {
        'mz' : float_to_idx,
        'scan' : int_to_idx,
        }

def no_idxes(ax_min, ax_max, ax_res):
    if ax_res is None:
        return ax_max - ax_min
    return int((ax_max - ax_min)/ax_res)

def mk_bitmap(frame_dct, axes=['mz', 'scan'], xax_min=0.0, xax_max=2000.0, xax_res=1.0, yax_min=0, yax_max=1000, yax_res=None, intens_cutoff=0, log_scale=True):
    xax, yax = axes

    X = frame_dct[xax]
    Y = frame_dct[yax]

    FILTER = (X>=xax_min) * (X<=xax_max) * (Y>=yax_min) * (Y<=yax_max)
    X=X[FILTER]
    Y=Y[FILTER]
    I=frame_dct['intensity'][FILTER]

    XI = transformations[xax](X, min_val=xax_min, resolution=xax_res)
    YI = transformations[yax](Y, min_val=yax_min, resolution=yax_res)

    IMG = make_dense(XI, YI, I, no_idxes(xax_min, xax_max, xax_res), no_idxes(yax_min, yax_max, yax_res))

    if intens_cutoff > 0:
        IMG[IMG<intens_cutoff] = 0

    if log_scale:
        IMG = np.log10(IMG+1)

    return IMG

def resolution_offset(resolution):
    if resolution is None:
        return 0.5
    return 0.5*resolution

def do_plot(plt, frame_dct, axes=['mz', 'scan'], xax_min=0.0, xax_max=2000.0, xax_res = 1.0, yax_min=0, yax_max=1000, yax_res=None, intens_cutoff=0, log_scale = True, max_intens = 1000):

    IMG = mk_bitmap(frame_dct, axes, xax_min, xax_max, xax_res, yax_min, yax_max, yax_res, intens_cutoff, log_scale)
    if log_scale:
        max_intens = np.log10(max_intens+1)

    plt.subplots(figsize=(6,4))
    plt.margins(0,0)
    plt.imshow(
            np.transpose(IMG),
            vmax=max_intens,
            extent=(xax_min-resolution_offset(xax_res),
                    xax_max+resolution_offset(xax_res),
                    yax_max+resolution_offset(yax_res),
                    yax_min-resolution_offset(yax_res)),
#            interpolation='None')
    )
    plt.xlabel(axes[0])
    plt.ylabel(axes[1])
    plt.colorbar(label="log10(intensity)", shrink=0.4, aspect=6)


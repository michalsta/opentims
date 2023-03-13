import numpy as np
from typing import Callable, List, Union


# def is_sorted(xx: np.array) -> bool:
#     x_prev = xx[0]
#     for x in xx:
#         if x < x_prev:
#             return False
#         x = x_prev
#     return True
## 80 times slower on sorted input...
def is_sorted(xx: np.array) -> bool:
    return np.all(xx[:-1] <= xx[1:])


#type defs
TranslationInt = Union[np.array, int, List[int]]
TranslationFloat = Union[np.array, float, List[float]]
FrameType = Union[np.array, int, List[int], List[int]]


def cast_to_numpy_arrays(
    x: Union[TranslationInt, TranslationFloat],
    frame: FrameType,
):
    if isinstance(frame, int):
        frame = np.array([frame], dtype=np.uint32)
    if isinstance(frame, list):
        frame = np.array(frame, dtype=np.uint32)
    if isinstance(x, (float, int)):
        x = np.array([x])# type will be changed later: this is a scalar anyway
    if isinstance(x, list):
        x = np.array(x)# OK, this is potentially more expensive, simply use arrays to avoid that.
    return (x, frame)


def translate_values_frame_sorted(
    x_frame_sorted: np.array,
    frame_sorted: np.array,
    bruker_translator_foo: Callable,
    x_dtype,
    result_dtype,
) -> np.array:
    assert x_dtype in (np.double, np.uint32), f"Wrong x_dtype: Bruker code only uses np.double and np.uint32, not {x_dtype}."
    assert result_dtype in (np.double, np.uint32), f"Wrong result_dtype: Bruker code only uses np.double and np.uint32, not {x_dtype}."
    if x_frame_sorted.dtype != x_dtype:
        x_frame_sorted = x_frame_sorted.astype(x_dtype)
    if frame_sorted.dtype != np.uint32:
        frame_sorted = frame_sorted.astype(np.uint32)
    result = np.empty(x_frame_sorted.shape, dtype=result_dtype)
    if len(result) == 0:
        return result
    i_prev = 0
    frame_id_prev = frame_sorted[0]
    for i, frame_id in enumerate(frame_sorted):
        if frame_id != frame_id_prev:
            result[i_prev:i] = bruker_translator_foo(
                frame_id_prev,
                x_frame_sorted[i_prev:i],
            )
            i_prev = i
        frame_id_prev = frame_id
    result[i_prev:len(result)] = bruker_translator_foo(
        frame_id_prev,
        x_frame_sorted[i_prev:len(result)],
    )
    return result


def translate_values_frames_not_guaranteed_sorted(
    x: np.array,
    frame: np.array,
    **kwargs,
) -> np.array:
    frames_are_sorted = is_sorted(frame)
    if frames_are_sorted:
        frame_sorted = frame
        x_frame_sorted = x
    else:
        order = np.argsort(frame)
        frame_sorted = frame[order]
        x_frame_sorted = x[order]
    result_sorted = translate_values_frame_sorted(
        x_frame_sorted=x_frame_sorted,
        frame_sorted=frame_sorted,
        **kwargs,
    )
    if frames_are_sorted:
        result = result_sorted
    else:
        result = result_sorted[np.argsort(order)]
    return result

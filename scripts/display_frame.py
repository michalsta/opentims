#!/usr/bin/env python
import sys
from pathlib import Path
from opentimspy import OpenTIMS
from matplotlib import pyplot as plt
import numpy as np

import argparse
parser = argparse.ArgumentParser(description='Display a plot of a single frame from TDF file.')
parser.add_argument("path", help="TDF dataset path", type=Path)
parser.add_argument("frames", help="Comma-separated list of frames, including ranges. Example: 314,320-330,435. Will plot everything if omitted.", nargs='?', default="")
parser.add_argument("-s", "--save", help="Save the images to files instead of displaying them", action="store_true")
parser.add_argument("--silent", help="do not display progressbar", action='store_true')
args=parser.parse_args()

from numba import jit
@jit(nopython=True)
def mkimage(scan, mz, intensity, A):
    for ii in range(len(scan)):
        A[scan[ii], int(mz[ii])] += intensity[ii]


with OpenTIMS(args.path) as OT:

    def worker(frame_id):
        frame = OT.query(frame_id, columns='scan mz intensity'.split())
        T = np.zeros(shape=(OT.max_scan,int(OT.max_mz)), dtype=np.uint32)

        mkimage(frame['scan'], frame['mz'], frame['intensity'], T)

        T = np.log((T+1))

        plt.imshow(T)
        plt.title("Frame "+str(frame_id))
        if args.save:
            plt.savefig(f"frame_{frame_id:06d}.png", dpi=500)
        else:
            plt.show()


    frames = set()
    for frame_desc in args.frames.split(','):
        if '-' in frame_desc:
            start, end = frame_desc.split('-')
            frames.update(range(int(start), int(end)+1))
        else:
            frames.add(int(frame_desc))

    if len(frames) == 0:
        frames = D.frames['Id']

    if args.silent:
        progressbar = lambda x: x
    else:
        from tqdm import tqdm as progressbar

    if args.save:
        from multiprocessing import Pool
        P = Pool()
        multiproc = lambda x: P.imap_unordered(worker, x)
    else:
        multiproc = lambda x: map(worker, sorted(x))


    for frame_id in progressbar(multiproc(frames), total=len(frames)):
        pass

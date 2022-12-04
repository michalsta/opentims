#!/usr/bin/env python3
import sys
from pathlib import Path
from collections import namedtuple
from functools import partial
import numpy as np


Range = namedtuple("Range", "min max".split())

def rangeize(in_str, conversion):
    x, y = in_str.split('-')
    return Range(conversion(x), conversion(y))

import argparse
parser = argparse.ArgumentParser(description='Display a sum plot of select frames of a TDF file.')
parser.add_argument("path", help="TDF dataset path", type=Path)
parser.add_argument("frames", help="Comma-separated list of frames, including slices. Example: 35,12::17 will sum frames: 35 and every 17th frame starting from 12 till the end.", default=":")
parser.add_argument("-s", "--save", help="Save the image to file instead of displaying them", action="store_true")
parser.add_argument("--silent", help="Do not display progressbar", action='store_true')
parser.add_argument("-p", "--processes", help="Number of subprocesses to use. Will use as many as there are detected cores in your system if omitted.", type=int, default=None)
parser.add_argument("--mz-range", help="Custom mz range, example: 400.0-600.0", type=partial(rangeize, conversion=float), default=Range(0, 2000))
parser.add_argument("--rt-range", help="Additionally clamp to RT range (constraining further the frames argument), example: 400.0-600.0", type=partial(rangeize, conversion=float), default=None)
parser.add_argument("--scan-range", help="Custom scan range, inclusive, example: 200-300", type=partial(rangeize, conversion=int), default=None)
parser.add_argument("--frame-range", help="Custom frame range, inclusive, example: 200-300. It will further constrain the <frames> argument.", type=partial(rangeize, conversion=int), default=None)
parser.add_argument("--mz-resolution", help="Custom mz binning resolution for plot. Default: 1.0", type=float, default=1.0)
parser.add_argument("--intensity", help="Clamp all intensities below this threshold to 0 (for simple noise removal)", type=int, default=0)
parser.add_argument("--title", help="Add this title to plot", type=str, default="")
parser.add_argument("-o", "--output", help="Output file if using -s", type=Path, default=Path("plot.png"))
parser.add_argument("-t", "--transform", help="Transform the intensities before plotting", type=str, default="", choices="log10 sqrt id none None".split())



args=parser.parse_args()


from matplotlib import pyplot as plt
from opentimspy import OpenTIMS, set_num_threads, plotting
from opentimspy.misc import parse_slice
set_num_threads(1)

with OpenTIMS(args.path) as OT:
    max_intens = OT.max_intensity
    if args.scan_range is None:
        args.scan_range = Range(1, OT.max_scan+1)


    if args.frames is None:
        frames = set(OT.frames['Id'])

    else:
        frames = set()
        slicelist = list(range(OT.max_frame+1)) # Yes, inefficient and inelegant

        for frame_desc in args.frames.split(','):
            if not ":" in frame_desc:
                frames.add(int(frame_desc))
            else:
                frames.update(slicelist[parse_slice(frame_desc)])

    frames.discard(0)
    if not (args.rt_range is None):
        frames = set(frameid for frameid in frames if args.rt_range.min <= OT.frame_properties[frameid].Time <= args.rt_range.max)
    if not (args.frame_range is None):
        frames = set(frameid for frameid in frames if args.frame_range.min <= frameid <= args.frame_range.max)

    frame = OT.query(list(sorted(frames)), columns='scan mz intensity'.split())
    plotting.do_plot(
            plt,
            frame,
            axes='mz scan'.split(),
            xax_min=args.mz_range.min,
            xax_max=min(OT.max_mz+1, args.mz_range.max),
            xax_res=args.mz_resolution,
            yax_min=args.scan_range.min,
            yax_max=args.scan_range.max,
            yax_res=None,
            intens_cutoff=args.intensity,
            transform=args.transform,
            max_intens=max_intens,
            aspect='auto')
    plt.title(args.title)
    if args.save:
        plt.savefig(args.output, dpi=600, bbox_inches="tight", pad_inches=0)
    else:
        plt.show()
    plt.close()


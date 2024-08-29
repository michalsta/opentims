#!/usr/bin/env python3
import sys
from pathlib import Path
from collections import namedtuple
from functools import partial


Range = namedtuple("Range", "min max".split())

def rangeize(in_str, conversion):
    x, y = in_str.split('-')
    return Range(conversion(x), conversion(y))

import argparse
parser = argparse.ArgumentParser(description='Display a plot of a single frame from TDF file.')
parser.add_argument("path", help="TDF dataset path", type=Path)
parser.add_argument("frames", help="Comma-separated list of frames, including ranges. Example: 314,320-330,435. Will plot everything if omitted.", nargs='?', default=None)
parser.add_argument("-s", "--save", help="Save the images to files instead of displaying them", action="store_true")
parser.add_argument("--silent", help="Do not display progressbar", action='store_true')
parser.add_argument("-p", "--processes", help="Number of subprocesses to use. Will use as many as there are detected cores in your system if omitted.", type=int, default=None)
parser.add_argument("--mz-range", help="Custom mz range, example: 400.0-600.0", type=partial(rangeize, conversion=float), default=Range(0, 2000))
parser.add_argument("--scan-range", help="Custom scan range, example: 200-300", type=partial(rangeize, conversion=int), default=None)
parser.add_argument("--mz-resolution", help="Custom mz binning resolution for plot. Default: 1.0", type=float, default=1.0)
parser.add_argument("--intensity", help="Clamp all intensities below this threshold to 0 (for simple noise removal)", type=int, default=0)
parser.add_argument("-o", "--output", help="Output directory if using -s", type=Path, default=Path("."))
parser.add_argument("-t", "--transform", help="Transform the intensities before plotting", type=str, default="", choices="log10 sqrt id".split())
parser.add_argument("-m", "--movie", help="Create a movie out of saved frames, save it to provided path", type=Path, default=None)




args=parser.parse_args()

if args.movie and not args.save:
    print("If -m/--movie is present, then -s/--save is also required, and -o strongly suggested")
    sys.exit(1)

from matplotlib import pyplot as plt
from opentimspy import OpenTIMS, set_num_threads, plotting
from opentimspy.misc import parse_slice
set_num_threads(1)

with OpenTIMS(args.path) as OT:
    max_intens = OT.max_intensity
    if args.scan_range is None:
        args.scan_range = Range(1, OT.max_scan+1)

    def worker(frame_id):
        frame = OT.query(frame_id, columns='scan mz intensity'.split())
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
                max_intens=max_intens)
        plt.title("Frame "+str(frame_id))
        if args.save:
            plt.savefig(args.output / f"frame_{frame_id:06d}.png", dpi=600, bbox_inches="tight", pad_inches=0)
        else:
            plt.show()
        plt.close()


    if args.frames is None:
        frames = set(OT.frame_properties.keys())
    else:
        frames = set()
        for frame_desc in args.frames.split(','):
            if '-' in frame_desc:
                start, end = frame_desc.split('-')
                frames.update(range(int(start), int(end)+1))
            else:
                frames.add(int(frame_desc))

    if args.silent:
        progressbar = lambda x: x
    else:
        from tqdm import tqdm
        progressbar = lambda x: tqdm(x, desc=sys.argv[0], total=len(frames))

    if args.save:
        from multiprocessing import Pool
        P = Pool(args.processes)
        multiproc = lambda x: P.imap_unordered(worker, x)
    else:
        multiproc = lambda x: map(worker, sorted(x))


    for frame_id in progressbar(multiproc(frames)):
        pass

if args.movie:
    import subprocess
    subprocess.run(f'''ffmpeg -y -framerate 10 -pattern_type glob -i '{args.output / '*.png'}' -vf "pad=ceil(iw/2)*2:ceil(ih/2)*2"  -c:v libx264 -r 50 -pix_fmt yuv420p {args.movie}''', shell=True)

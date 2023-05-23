#!/usr/bin/env python3
import sys
from pathlib import Path

import opentimspy
from opentimspy import OpenTIMS

if opentimspy.bruker_bridge_present:
    all_columns = ('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')
else:
    all_columns = ('frame','scan','tof','intensity','retention_time')

import argparse

parser = argparse.ArgumentParser(description='Export a set of frames in TSV format.')
parser.add_argument("path", help="TDF dataset path", type=Path)
parser.add_argument("frames", help="Comma-separated list of frames, including ranges. Example: 314,320-330,435. Will output everything if omitted.", nargs='?', default="")
parser.add_argument("--no-hdr", help="Do not print the header.", action="store_true")
parser.add_argument("-o", "--output", help="File to output to. Will print to stdout if omitted", type=Path, default=None)
args=parser.parse_args()


frames = set()
if args.frames != "":
    for frame_desc in args.frames.split(','):
        if '-' in frame_desc:
            start, end = frame_desc.split('-')
            frames.update(range(int(start), int(end)+1))
        else:
            frames.add(int(frame_desc))


out_file = sys.stdout if args.output is None else args.output.open(mode="wt")

with OpenTIMS(args.path) as D:
    if args.frames == "":
        frames = set(D.frames['Id'])

    # prepare and print the CSV header:
    if not args.no_hdr:
        header = '"' + '"\t"'.join(all_columns) + '"'
        print(header, file=out_file)


    # Iterate over frames. This will store only one frame at a time in RAM, preventing out of memory errors.
    for frame_id in sorted(frames):
        frame = D.query(frame_id)
        peak_idx = 0
        # Frame is stored as a dict of column vectors
        while peak_idx < len(frame['frame']):
            row = [str(frame[colname][peak_idx]) for colname in all_columns]
            print('\t'.join(row), file=out_file)
            peak_idx += 1

if args.output is not None:
    out.file.close()

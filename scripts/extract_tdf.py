#!/usr/bin/env python
import sys
from pathlib import Path
from pprint import pprint

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
args=parser.parse_args()


frames = set()
for frame_desc in args.frames.split(','):
    if '-' in frame_desc:
        start, end = frame_desc.split('-')
        frames.update(range(int(start), int(end)+1))
    else:
        frames.add(int(frame_desc))

if len(frames) == 0:
    frames = D.frames['Id']

with OpenTIMS(args.path) as D:
    # prepare and print the CSV header:
    header = '"' + '"\t"'.join(all_columns) + '"'
    print(header)


    # Iterate over frames. This will store only one frame at a time in RAM, preventing out of memory errors.
    for frame_id in sorted(frames):
        frame = D.query(frame_id)
        peak_idx = 0
        # Frame is stored as a dict of column vectors
        while peak_idx < len(frame['frame']):
            row = [str(frame[colname][peak_idx]) for colname in all_columns]
            print('\t'.join(row))
            peak_idx += 1


#!/usr/bin/env python3
import sys
from tqdm import tqdm
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
parser.add_argument("-c", "--convert", help="Perform tof->mz and scan->inv_ion_mobility conversions too", action="store_true")
args=parser.parse_args()

all_columns = ('frame','scan','tof','intensity','retention_time')
if args.convert:
    all_columns = ('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')


with OpenTIMS(args.path) as D:
    frames = D.frames['Id']
    # Iterate over frames. This will store only one frame at a time in RAM, preventing out of memory errors.
    for frame_id in tqdm(sorted(frames)):
        frame = D.query(frame_id, columns=all_columns)

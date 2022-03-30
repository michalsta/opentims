#!/usr/bin/env python3
from tqdm import tqdm
from pathlib import Path

import opentimspy
from opentimspy import OpenTIMS

import argparse

parser = argparse.ArgumentParser(description='Verify the integrity of TDF dataset. Any errors could indicate corruption - or a bug in OpenTIMS.')
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
print("Success! File does not seem to be corrupted.")

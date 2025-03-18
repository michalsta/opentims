#!/usr/bin/env python3
from tqdm import tqdm
from pathlib import Path


import argparse

parser = argparse.ArgumentParser(description='Verify the integrity of TDF dataset. Any errors could indicate corruption - or a bug in OpenTIMS.')
parser.add_argument("path", help="TDF dataset path", type=Path)
parser.add_argument("-c", "--convert", help="Perform tof->mz and scan->inv_ion_mobility conversions too", action="store_true")
parser.add_argument("-d", "--digest", help="Compute and print SHA256 digest of the dataset", action="store_true")
args=parser.parse_args()

all_columns = ('frame','scan','tof','intensity','retention_time')
if args.convert:
    all_columns = ('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')

from opentimspy import OpenTIMS
import hashlib

if args.digest:
    sha256 = hashlib.sha256()

with OpenTIMS(args.path) as D:
    frames = D.frames['Id']
    # Iterate over frames. This will store only one frame at a time in RAM, preventing out of memory errors.
    for frame_id in tqdm(sorted(frames)):
        frame = D.query(frame_id, columns=all_columns)
        if args.digest:
            for key in sorted(frame.keys()):
                sha256.update(frame[key])


if args.digest:
    print(sha256.hexdigest())
else:
    print("Success! File does not seem to be corrupted.")

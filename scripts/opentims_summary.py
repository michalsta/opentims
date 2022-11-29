#!/usr/bin/env python3
from pathlib import Path


import argparse

parser = argparse.ArgumentParser(description='Verify the integrity of TDF dataset. Any errors could indicate corruption - or a bug in OpenTIMS.')
parser.add_argument("path", help="TDF dataset path", type=Path)
#parser.add_argument("-c", "--convert", help="Perform tof->mz and scan->inv_ion_mobility conversions too", action="store_true")
args=parser.parse_args()

#all_columns = ('frame','scan','tof','intensity','retention_time')
#if args.convert:
#    all_columns = ('frame','scan','tof','intensity','mz','inv_ion_mobility','retention_time')

from tqdm import tqdm
import opentimspy
from opentimspy import OpenTIMS
import numpy as np

def find_nth(L, what, n):
    for idx, x in enumerate(L):
        if x == what:
            if n == 0:
                return idx
            n -= 1

with OpenTIMS(args.path) as D:
    #print(D.frames)
    no_frames = len(D.frames['Id'])
    print(f"No frames: {no_frames}")
    rt_extent = D.frames['Time'][0] , D.frames['Time'][-1]
    rt_total_time = rt_extent[1] - rt_extent[0]
    print(f"RT extent: {rt_extent[0]} - {rt_extent[1]}, total: {rt_total_time}, ({rt_total_time/60.0} minutes)")
    print(f"RT per frame: {rt_total_time/no_frames}")
    print(f"Frames per minute: {60.0*no_frames/rt_total_time}")
    dia_cycle_len = find_nth(D.frames['MsMsType'], 0, 3) - find_nth(D.frames['MsMsType'], 0, 2)
    print(f"Length of second DIA cycle: {dia_cycle_len}")
    print(f"DIA cycles per minute: {60.0*no_frames/rt_total_time/dia_cycle_len}")
    print(f"No MS1 frames: {np.count_nonzero(D.frames['MsMsType'] == 0)}")

'''    frames = D.frames['Id']
    # Iterate over frames. This will store only one frame at a time in RAM, preventing out of memory errors.
    for frame_id in tqdm(sorted(frames)):
        frame = D.query(frame_id, columns=all_columns)
print("Success! File does not seem to be corrupted.")'''

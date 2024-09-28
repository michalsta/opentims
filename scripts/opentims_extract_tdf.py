#!/usr/bin/env python3
import sys
from pathlib import Path

import opentimspy
from opentimspy import OpenTIMS

if opentimspy.bruker_bridge_present:
    all_columns = (
        "frame",
        "scan",
        "tof",
        "intensity",
        "mz",
        "inv_ion_mobility",
        "retention_time",
    )
else:
    all_columns = ("frame", "scan", "tof", "intensity", "retention_time")

import argparse

parser = argparse.ArgumentParser(description="Export a set of frames in TSV format.")
parser.add_argument("path", help="TDF dataset path", type=Path)
parser.add_argument(
    "frames",
    help="Comma-separated list of frames, including ranges. Example: 314,320-330,435. Will output everything if omitted.",
    nargs="?",
    default="",
)
parser.add_argument("--no-hdr", help="Do not print the header.", action="store_true")
parser.add_argument(
    "-o",
    "--output",
    help="File to output to. Will print to stdout if omitted",
    type=Path,
    default=None,
)
parser.add_argument(
    "--format",
    type=str,
    help="Output format, one of: mmapped_df (fastest), hdf5, csv (default).",
    required=False,
    default="csv",
)

args = parser.parse_args()

import tqdm

frames = set()
if args.frames != "":
    for frame_desc in args.frames.split(","):
        if "-" in frame_desc:
            start, end = frame_desc.split("-")
            frames.update(range(int(start), int(end) + 1))
        else:
            frames.add(int(frame_desc))


if args.format == "mmapped_df":
    try:
        import mmapped_df
    except ImportError:
        print(
            "mmapped_df is not installed. Please install it from https://github.com/michalsta/mmapped_df, or use a different format."
        )
        sys.exit(1)
    opener = lambda: mmapped_df.DatasetWriter(args.output)
    writer = lambda f, df: f.append(**df)
elif args.format == "hdf5":
    import pandas as pd

    opener = lambda: pd.HDFStore(args.output, mode="w", complevel=9, complib="blosc")
    writer = lambda f, df: pd.DataFrame(df).to_hdf(
        f,
        key="df",
        format="table",
        data_columns=True,
        complevel=9,
        complib="blosc",
        append=True,
    )
elif args.format == "csv":
    import pandas as pd

    out_file = sys.stdout if args.output is None else args.output.open(mode="wt")
    opener = lambda: out_file
    hdr_written = False

    def writer(f, df):
        global hdr_written
        df = pd.DataFrame(df)
        df.to_csv(f, header=not hdr_written)
        hdr_written = True

else:
    raise ValueError(
        f"Invalid format: {args.format}. Please specify one of: mmapped_df, hdf5, csv."
    )


progressbar = lambda x: x
if args.output != None:
    progressbar = tqdm.tqdm

with OpenTIMS(args.path) as D, opener() as f:
    if args.frames == "":
        frames = set(D.frames["Id"])

    # Iterate over frames. This will store only one frame at a time in RAM, preventing out of memory errors.
    for frame_id in progressbar(list(sorted(frames))):
        frame = D.query(frame_id)
        writer(f, frame)

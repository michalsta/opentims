#!/usr/bin/env python3
# Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
# SPDX-License-Identifier: MIT
"""Run prebuilt benchmarks sequentially and record commands and full output."""
import argparse
import json
import os
import platform
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("datasets", nargs="+")
parser.add_argument("--upstream")
parser.add_argument("--adapted")
parser.add_argument("--frames-only", action="store_true", help="Only compare native/C++ frame readers")
parser.add_argument("--uring-depths", help="Queue depths; build compare with features cpp,uring first")
parser.add_argument("--repetitions", type=int, default=9)
parser.add_argument("--frames", type=int, default=1000)
parser.add_argument("--output", required=True)
args = parser.parse_args()
if not args.frames_only and (not args.upstream or not args.adapted):
    parser.error("provide --upstream and --adapted, or use --frames-only")
root = Path(__file__).resolve().parent
runs = []
for dataset in args.datasets:
    command = [str(root / "opentims/target/release/examples/compare"), dataset, str(args.repetitions), str(args.frames)]
    if args.uring_depths:
        command.append(args.uring_depths)
    jobs = [("native-cpp", command, {})]
    if not args.frames_only:
        for name, binary, env in [("upstream", args.upstream, {}), ("adapted-pread", args.adapted, {"OPENTIMS_IO": "pread"}), ("adapted-mmap", args.adapted, {"OPENTIMS_IO": "mmap"})]:
            jobs.append((name, [binary, dataset, str(args.frames)], env))
    for name, command, env in jobs:
        print(f"Running {name}: {dataset}", flush=True)
        proc = subprocess.run(command, env={**os.environ, **env}, text=True, capture_output=True)
        runs.append(dict(name=name, command=command, env=env, returncode=proc.returncode, stdout=proc.stdout, stderr=proc.stderr))
        Path(args.output).write_text(json.dumps(dict(platform=platform.platform(), runs=runs), indent=2) + "\n")
        print(proc.stdout, flush=True)
        if proc.returncode:
            raise SystemExit(proc.stderr)
        if name == "native-cpp" and args.uring_depths:
            for depth in args.uring_depths.split(','):
                if f"rust-uring-{int(depth)}," not in proc.stdout:
                    raise SystemExit("Missing io_uring results: rebuild compare with features cpp,uring")
    if args.frames_only:
        continue
    fingerprints = [tuple(line.split(" hash=")[1].split()[0] for line in run["stdout"].splitlines() if " hash=" in line) for run in runs[-3:]]
    if len(fingerprints[0]) != 2 or len(set(fingerprints)) != 1:
        raise SystemExit("Upstream/adapter fingerprints differ")

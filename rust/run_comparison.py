#!/usr/bin/env python3
# Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
# SPDX-License-Identifier: MIT
"""Run upstream and adapted compat-check sequentially; record output, compare fingerprints."""
import argparse
import json
import os
import platform
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("datasets", nargs="+")
parser.add_argument("--upstream", required=True)
parser.add_argument("--adapted", required=True)
parser.add_argument("--frames", type=int, default=1000)
parser.add_argument("--output", required=True)
args = parser.parse_args()
runs = []
for dataset in args.datasets:
    jobs = []
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
    fingerprints = [tuple(line.split(" hash=")[1].split()[0] for line in run["stdout"].splitlines() if " hash=" in line) for run in runs[-3:]]
    if len(fingerprints[0]) != 2 or len(set(fingerprints)) != 1:
        raise SystemExit("Upstream/adapter fingerprints differ")

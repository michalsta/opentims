#!/usr/bin/env python3
# Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
# SPDX-License-Identifier: MIT
"""Sequential matched comparison; raw samples and handoff status are persisted."""
import argparse
import itertools
import json
import os
from pathlib import Path
import platform
import random
import subprocess
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',required=True)
parser.add_argument('--frames',type=int,default=1000)
parser.add_argument('--warm-reps',type=int,default=5)
parser.add_argument('--cold-reps',type=int,default=3)
parser.add_argument('--datasets',nargs='+',default=[
    '/home/matteo/data_for_midiaID/F9477.d',
    '/home/matteo/data_for_midiaID/DDA/MHC1/G220824_007_Slot2-28_1_6732.d'])
args=parser.parse_args()
root=Path(__file__).resolve().parent
status=Path('/tmp/opentims-parallel-20261114')
status.mkdir(exist_ok=True)
binary=root/'opentims/target/release/examples/parallel_compare'
result={'platform':platform.platform(),'cpu':next(line.split(':',1)[1].strip() for line in Path('/proc/cpuinfo').read_text().splitlines() if line.startswith('model name')),
        'rustc':subprocess.check_output(['rustc','--version'],text=True).strip(),
        'affinity':sorted(os.sched_getaffinity(0)),'started_at':time.time(),'runs':[],'serial_baselines':[]}

def save():
    target=Path(args.output)
    temporary=target.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(result,indent=2)+'\n')
    temporary.replace(target)

def execute(command,case):
    start=time.time()
    with subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True) as proc:
        (status/'current.json').write_text(json.dumps({'pid':proc.pid,'case':case,'command':command,'started_at':start}))
        print(f'PID {proc.pid}: {case}',flush=True)
        stdout,stderr=proc.communicate()
        if proc.returncode:
            (status/'failure.json').write_text(json.dumps({'command':command,'stdout':stdout,'stderr':stderr,'exit_code':proc.returncode}))
            raise RuntimeError(f'{case}: exit {proc.returncode}\n{stderr}\n{stdout}')
    return stdout,stderr

for dataset in args.datasets:
    command=[str(root/'opentims/target/release/examples/compare'),dataset,'9',str(args.frames),'1,8,32,128']
    stdout,stderr=execute(command,{'serial_baseline':dataset})
    result['serial_baselines'].append({'dataset':dataset,'command':command,'stdout':stdout,'stderr':stderr})
    save()

rng=random.Random(20261114)
for cache in ['warm','cold']:
    workers=[1,2,4,8] if cache=='warm' else [1,8]
    batches=[8,32,128] if cache=='warm' else [8,128]
    groups=list(itertools.product(args.datasets,['sequential','shuffled'],workers,batches))
    rng.shuffle(groups)
    for dataset,order,worker,batch in groups:
        backends=['mmap','uring-sync','uring-pipeline'] if cache=='warm' else ['mmap','uring-pipeline']
        rng.shuffle(backends)
        for backend in backends:
            reps=args.warm_reps if cache=='warm' else args.cold_reps
            case=dict(dataset=dataset,order=order,workers=worker,batch=batch,backend=backend,cache=cache)
            command=[str(binary),dataset,backend,str(worker),str(batch),str(args.frames),order,str(reps),cache]
            stdout,stderr=execute(command,case)
            records=[json.loads(line) for line in stdout.splitlines()]
            samples=[r for r in records if 'wall_s' in r]
            assert len(samples)==reps,(case,records)
            result['runs'].append({**case,'command':command,'samples':samples,'latency':[r for r in records if r.get('latency')],
                                   'profiles':[r for r in records if r.get('profile')],'stderr':stderr})
            save()
            print(f"Completed {len(result['runs'])}/176 configurations",flush=True)
result['finished_at']=time.time()
save()
print('All configurations passed exact output validation.',flush=True)

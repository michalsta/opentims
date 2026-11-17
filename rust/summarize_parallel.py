#!/usr/bin/env python3
# Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
# SPDX-License-Identifier: MIT
"""Summarize the completed comparison without discarding individual samples."""
import argparse
import csv
import json
from pathlib import Path
from statistics import median

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('results')
parser.add_argument('--csv',required=True)
args=parser.parse_args()
data=json.loads(Path(args.results).read_text())
if 'finished_at' not in data:
    raise SystemExit('Campaign is still running')
rows=[]
for run in data['runs']:
    samples=run['samples']; times=[s['wall_s'] for s in samples]
    row={k:run[k] for k in ('dataset','cache','order','workers','batch','backend')}
    row.update(median_ms=1000*median(times),min_ms=1000*min(times),max_ms=1000*max(times),
               cpu_ms=1000*median(s['cpu_s'] for s in samples),
               million_peaks_s=samples[0]['peaks']/median(times)/1e6,
               peak_rss_mib=max(s['peak_rss_kib'] for s in samples)/1024,
               delivery_p50_ms=1000*median(s['batch_delivery_p50_s'] for s in run['latency']),
               delivery_p95_ms=1000*median(s['batch_delivery_p95_s'] for s in run['latency']),
               read_mib=median(s['read_bytes'] for s in samples)/2**20,
               major_faults=median(s['major_faults'] for s in samples),
               resident_fraction_before=median(s['resident_pages_before']/s['file_pages'] for s in samples))
    rows.append(row)
with Path(args.csv).open('w') as handle:
    writer=csv.DictWriter(handle,fieldnames=list(rows[0]))
    writer.writeheader();writer.writerows(rows)
datasets=list(dict.fromkeys(r['dataset'] for r in rows))
lookup={(r['dataset'],r['cache'],r['order'],r['workers'],r['batch'],r['backend']):r for r in rows}
for cache,orders,workers in [('warm',['sequential'],[1,2,4,8]),('cold',['sequential','shuffled'],[1,8])]:
    print('\n',cache,'batch=128')
    for dataset in datasets:
        for order in orders:
            for worker in workers:
                a=lookup[(dataset,cache,order,worker,128,'mmap')]
                b=lookup[(dataset,cache,order,worker,128,'uring-pipeline')]
                sync=lookup.get((dataset,cache,order,worker,128,'uring-sync'))
                print(Path(dataset).name,order,worker,'mmap',round(a['median_ms'],2),'sync',round(sync['median_ms'],2) if sync else '-',
                      'pipeline',round(b['median_ms'],2),'mmap/pipe',round(a['median_ms']/b['median_ms'],3),
                      'CPU ms',round(a['cpu_ms'],1),round(b['cpu_ms'],1),'RSS MiB',round(a['peak_rss_mib'],1),round(b['peak_rss_mib'],1),
                      'read MiB',round(a['read_mib'],1),round(b['read_mib'],1))
print('\nProfiles:')
for run in data['runs']:
    for profile in run['profiles']:print(Path(run['dataset']).name,profile)
print('\nCold cache resident fractions:',min(r['resident_fraction_before'] for r in rows if r['cache']=='cold'),max(r['resident_fraction_before'] for r in rows if r['cache']=='cold'))

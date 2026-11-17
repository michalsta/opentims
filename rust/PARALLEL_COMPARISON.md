<!-- Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki; SPDX-License-Identifier: MIT -->
# Parallel decoding and pipelined io_uring comparison

This experiment compares three paths with the same frame selection, normalized
intensities, output representation, decoder, per-worker Zstandard contexts and
static partition of each batch across decoding workers:

- `mmap`: workers access mapped payloads and decode them in parallel.
- `uring-sync`: the caller reads a compressed batch through io_uring, waits,
  then dispatches parallel decoding. This is the no-overlap control.
- `uring-pipeline`: a producer reads the next compressed batch while the decoder
  pool processes the current one. Exactly two owned batches circulate through
  bounded channels. All allocations are returned for reuse between passes.

The producer is an **extra userspace thread** beyond the reported decoder-worker
count. The controlling thread also exists in all three paths; it waits for the
decoder pool. No claim of equal total thread count is made. Process CPU time
includes the producer and decoder threads. Kernel io_uring workers can also
service I/O; their system-wide CPU cost is not isolated here.

The core APIs are `Reader::compressed_frame`,
`uring::Reader::read_compressed_batch_into` and
`Workspace::decode_compressed`. Completed payloads are owned by `CompressedBatch`
before being handed to another thread. No outstanding kernel request references
the batch after successful handoff. The original scalar reader APIs remain.

## Protocol

Linux x86-64; hardware, toolchain, CPU affinity and raw commands are recorded in
[benchmark-parallel-results.json](benchmark-parallel-results.json).
The first 1,000 frames of each of `F9477.d` (DIA) and
`G220824_007_Slot2-28_1_6732.d` (DDA) are selected. Shuffled runs apply the same
deterministic permutation to these IDs; they do not sample the rest of the run.

Warm sweep: 1/2/4/8 decoder workers, 8/32/128-frame batches, sequential/shuffled
access, three backends, five timed repetitions: 144 configurations.
Page-cache-cold sweep: 1/8 workers, 8/128-frame batches, both access orders,
mmap/pipeline, three timed repetitions: 32 configurations. Configurations are
run in separate subprocesses, sequentially, with randomized group/backend order.
Original serial pread/mmap/C++-FFI/io_uring baselines are also rerun and recorded.

Each subprocess verifies **every output element** against the scalar pread
reader before timing: TOFs, intensities and scan offsets, in requested order.
The same eight integration tests and Clippy checks cover the native, C++ and
io_uring paths, including owned-batch recycling and concurrent decoding.

Warmup and opening are excluded from the timed loop. Timing includes dispatch,
allocation reuse, queue synchronization, producer startup/shutdown and output
assembly. Outputs are consumed and reused one batch at a time; a complete
experiment does not retain all decoded frames. Each subprocess keeps its worker
pool and decompression contexts across repetitions.

Before each cold repetition, the mmap is dropped and `POSIX_FADV_DONTNEED` is
applied to **that dataset's binary file only**. No global cache flush is used.
The map is then reopened. `mincore` measures whole-file page residency before
timing; `/proc/self/io` read bytes and page-fault deltas are recorded. These are
OS page-cache measurements, not a claim about SSD controller or hardware caches.

Recorded metrics: wall time, process CPU time, throughput, batch-delivery p50/p95,
peak resident memory, major/minor faults, storage read bytes and cache residency.
Latency measures time between completed output batches, including waiting and
decoding; it is not single-spectrum latency. Peak RSS (`VmHWM`) covers the whole
subprocess, including validation. mmap file-backed resident pages count toward
RSS, so this is not a private-heap comparison.

Separate instrumented serial profiles measure pread I/O/copying, Zstandard
decompression and decoding/output assembly. Decoding and output writes are fused
in this implementation and are reported together. mmap demand-page costs occur
when payload bytes are touched, including within decompression; its header-access
timer alone does not measure all I/O cost.

## Results (2026-11-14)

All **176 configurations / 816 timed repetitions** completed successfully, plus
the fresh serial baseline runs. Every configuration passed exact elementwise
output validation. [parallel-summary.csv](parallel-summary.csv) contains every
configuration, including min/max time, CPU, throughput, batch latency and RSS;
the JSON retains all individual samples. The run took place on an AMD Ryzen 7
4800H with Linux 7.0.0-113042-tuxedo and Rust 1.99.0.

**Parallel decoding helps substantially. For these datasets, mmap with eight
workers and 128-frame batches was the fastest tested configuration in every
cache/access-order combination.** Pipelining improved io_uring over its no-overlap
control, but did not beat that mmap configuration.

Warm cache, sequential access, **matched 128-frame batches**, median milliseconds:

| Dataset | Decoder workers | mmap | io_uring, no overlap | io_uring, pipelined |
|---|---:|---:|---:|---:|
| DIA | 1 | 204.73 | 236.67 | 204.34 |
| DIA | 2 | 112.39 | 146.08 | 127.13 |
| DIA | 4 | 88.89 | 103.40 | 91.59 |
| DIA | 8 | **60.08** | 83.18 | 64.03 |
| DDA | 1 | 658.70 | 705.82 | 664.33 |
| DDA | 2 | 372.32 | 440.22 | 430.76 |
| DDA | 4 | 225.41 | 298.89 | 281.77 |
| DDA | 8 | **187.57** | 257.43 | 209.39 |

For matched mmap configurations, one to eight decoding workers gave about
**3.4× (DIA) and 3.5× (DDA)** speedup. At eight workers, pipelining cut io_uring's
no-overlap time by about 23% and 19%, respectively. Its remaining gap to mmap was
about 7% on DIA and 12% on DDA; the small DIA timing ranges overlapped.
The larger-batch configurations consistently won the throughput sweep. The
implementation still has a barrier after each decoded batch, and static
partitions can be imbalanced when frame sizes vary. This is not a claim that
the optimal decoding scheduler has been found.

Cold page cache, **matched 128-frame batches**, median milliseconds:

| Dataset | Order | Workers | mmap | io_uring pipeline |
|---|---|---:|---:|---:|
| DIA | Sequential | 1 | 233.00 | 228.53 |
| DIA | Sequential | 8 | **84.14** | 93.46 |
| DIA | Shuffled | 1 | 292.76 | **228.77** |
| DIA | Shuffled | 8 | **105.74** | 116.05 |
| DDA | Sequential | 1 | 796.06 | **691.11** |
| DDA | Sequential | 8 | **274.85** | 291.13 |
| DDA | Shuffled | 1 | 958.75 | **740.99** |
| DDA | Shuffled | 8 | **277.89** | 305.37 |

There is a real niche for overlap here: with **one decoding worker and cold,
shuffled reads**, the pipeline reduced time by about **22% on both datasets**.
This uses an extra I/O producer thread, so it is not equal total-thread-count
evidence. With eight decoding workers, mmap remained faster. Every cold sample
reported **zero resident file pages** before timing and nonzero storage reads;
the measured binary reads were roughly 56–61 MiB for DIA and 206–209 MiB for DDA
in the configurations above. Hardware/device caches were not controlled.

Warm sequential access, eight workers / 128-frame batches:

| Dataset | Backend | Median process CPU ms | Peak RSS MiB |
|---|---|---:|---:|
| DIA | mmap | 323.9 | 202.8 |
| DIA | Pipeline | 376.9 | 189.9 |
| DDA | mmap | 1159.4 | 369.4 |
| DDA | Pipeline | 1397.4 | 240.4 |

Pipeline CPU consumption was higher. mmap's larger RSS includes mapped file
pages and is not evidence of a larger private heap. Shuffled access can retain
larger frame-buffer capacities; inspect the full CSV for those memory results.

The separate serial pread profile explains the warm-cache result:

| Dataset | I/O and copying ms | Zstandard ms | Decode/output ms |
|---|---:|---:|---:|
| DIA | 11.81 | 96.04 | 90.76 |
| DDA | 47.30 | 295.46 | 350.12 |

Only about 6–7% of the measured serial work was positioned I/O/copying. Most
work was decompression and decoding/output assembly. Overlapping I/O has limited
room to help when those reads already hit the cache.

These are five-repeat warm and three-repeat cold measurements on a shared host,
with no CPU affinity or frequency locking. Small gaps should not be treated as
statistical proof. No complete Sage search was benchmarked, and its adapter has
not been switched to the new batch APIs. For the tested throughput workload,
prefer parallel mmap; retain the pipeline for investigating storage-bound cases.

## Reproduce

```sh
cargo test --manifest-path rust/opentims/Cargo.toml --features cpp,uring,parallel
cargo build --release --manifest-path rust/opentims/Cargo.toml \
  --features cpp,uring,parallel --examples
python3 rust/run_parallel_comparison.py --output /tmp/parallel-results.json \
  --datasets /path/to/dia.d /path/to/dda.d
python3 rust/summarize_parallel.py /tmp/parallel-results.json \
  --csv /tmp/parallel-summary.csv
```

The runner saves results after each configuration. The historical controller
reference for this campaign is PID `159483`, with status under
`/tmp/opentims-parallel-20261114/`. `benchmarks-finished.json` records benchmark
exit status; **`complete.json` is the handoff signal after analysis and reporting**.
PID disappearance alone is not a success signal.

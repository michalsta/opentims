<!-- Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki; SPDX-License-Identifier: MIT -->
# io_uring alternative: implementation and measurements

These are the original **serial-decoding** measurements. The subsequent matched
parallel/pipelined comparison is in [PARALLEL_COMPARISON.md](PARALLEL_COMPARISON.md).

Implemented in [opentims/src/uring.rs](opentims/src/uring.rs), behind Linux-only
feature `uring`. It uses actual io_uring read submissions, without a pread
fallback. Each bounded batch reads frame headers together, then payloads
together, and runs the existing Rust decompressor/decoder on the calling thread.
Requested order, duplicate IDs, empty frames, raw counts and normalized counts
are preserved. The API reuses the ring, compressed buffers and output vectors.

This is buffered I/O with one ring per reader. There is no SQPOLL, registered
buffer pool, O_DIRECT, or pipeline overlap between decoding and the next batch.
It is a separate batch API; the existing Sage adapter has not been switched.

## Results, 2026-11-14

Host: AMD Ryzen 7 4800H, Linux `7.0.0-113042-tuxedo`, x86-64; Rust 1.99.0;
io-uring crate 0.7.16. Release builds, first 1,000 frames per dataset, nine
repetitions, warm filesystem cache, one calling thread. The kernel may service
io_uring requests using its own workers. All seven backends rotate through
execution order within each repetition. No CPU affinity or clock locking.

Every selected output was checked before timing: scan offsets, TOF indices and
normalized intensities match exactly across all backends and all queue depths.
Opening and validation are excluded from the timed extraction loop. The C++
measurement includes Rust FFI and scan-ID-to-scan-offset conversion, as before.

| Backend | DIA median ms | DDA median ms |
|---|---:|---:|
| Rust pread | 194.107 | 661.087 |
| Rust mmap | **192.858** | **626.676** |
| C++ through Rust FFI, mmap | 218.349 | 731.128 |
| Rust io_uring, depth 1 | 197.577 | 666.948 |
| Rust io_uring, depth 8 | 199.918 | 674.862 |
| Rust io_uring, depth 32 | 225.964 | 689.582 |
| Rust io_uring, depth 128 | 213.815 | 676.098 |

DIA: `F9477.d`, 20,181,212 peaks. DDA:
`G220824_007_Slot2-28_1_6732.d`, 77,112,514 peaks.
Full commands, opening times, min/max timings, throughput and host kernel are
saved in [benchmark-uring-results.json](benchmark-uring-results.json).

**This implementation did not outperform mmap or pread on these workloads.**
The fastest io_uring configuration was depth 1: about 2.4% slower than mmap on
DIA and 6.4% slower on DDA. Larger queue depths did not improve throughput.
The small DIA differences also sit within overlapping timing ranges.

These are whole-reader comparisons. The io_uring path returns a batch of frames
and keeps more output buffers live than the scalar mmap/pread/C++ paths. Its
allocation bookkeeping and cache footprint are part of the measurement. The
results do not isolate syscall cost and do not predict cold-disk, network-store,
random-access or full Sage-search performance. No global cache eviction was
performed. Use mmap for the measured warm sequential workload; keep io_uring as
an experimental alternative for storage-bound workloads and further pipelining.

## Reproduce

```sh
cargo test --manifest-path rust/opentims/Cargo.toml --features cpp,uring
cargo build --release --manifest-path rust/opentims/Cargo.toml \
  --features cpp,uring --example compare
python3 rust/run_comparison.py --frames-only --uring-depths 1,8,32,128 \
  --output /tmp/uring-results.json /path/to/dia.d /path/to/dda.d
```

Ring creation errors abort the test/benchmark rather than substituting a
different backend. Tests cover repeated/gapped IDs, empty batches and frames,
raw/normalized counts, bad frame IDs, invalid queue depths, corrupted scan
headers, EOF/short reads, successful reuse after errors, and compressed-fixture
parity. Original native/C++ tests remain enabled.

Buffer ownership is internal to the reader. All completions are collected
before buffers are inspected or resized, even if an individual operation
failed. An unrecoverable submission/wait error poisons the reader; allocations
that might still be referenced by the kernel are deliberately retained. This
also protects unwinding during submission. No user buffer is lent to the kernel.
See the upstream [io-uring submission queue safety contract](https://docs.rs/io-uring/0.7.16/io_uring/squeue/struct.SubmissionQueue.html#method.push).

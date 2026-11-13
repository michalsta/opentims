<!-- Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki; SPDX-License-Identifier: MIT -->
# Measurements, 2026-11-13

Linux x86-64, AMD Ryzen 7 4800H (8 cores / 16 threads), Rust 1.99.0.
Release builds; sequential frame access, one timed reader thread, warm file
cache. Native and C++ readers share Cargo's Zstandard 1.5.6 and SQLite builds.
No CPU affinity, frequency lock, cache eviction or isolated host was used.

Full commands and outputs: [benchmark-results.json](benchmark-results.json).
The sequential orchestration is in [run_comparison.py](run_comparison.py).

## Native Rust versus C++ binding

Every selected scan offset, TOF and normalized intensity matched exactly before
timing. Both implementations reuse output and decompression storage. C++ also
converts its per-peak scan IDs into the returned scan-offset representation.
These are end-to-end binding measurements, not isolated decoder timings.
Backend order rotates over nine repetitions; times exclude opening and checks.

| Dataset (first 1,000 frames) | Backend | Median ms | Min–max ms | Million peaks/s |
|---|---|---:|---:|---:|
| F9477, DIA, 20,181,212 peaks | Rust pread | 195.751 | 190.064–217.879 | 103.097 |
| F9477 | Rust mmap | 185.152 | 181.298–194.031 | 108.998 |
| F9477 | C++ mmap | 209.826 | 205.844–261.127 | 96.181 |
| G220824_007, DDA, 77,112,514 peaks | Rust pread | 1052.438 | 767.131–1694.014 | 73.270 |
| G220824_007 | Rust mmap | 1093.767 | 808.901–1539.933 | 70.502 |
| G220824_007 | C++ mmap | 1124.149 | 848.733–1396.263 | 68.596 |

On this DIA sample, Rust mmap took 11.8% less time than the C++ binding and 5.4%
less than Rust pread. DDA distributions overlap substantially: the small median
differences do not establish a reliable winner. Do not generalize to cold,
networked or random reads, or infer a whole-search speedup.

## timsrust API adapter

First 1,000 frames and first 1,000 spectra per dataset; seven repetitions of
each operation, warm cache, same public timsrust API. These runs are sequential
by backend, not interleaved. They have more exposure to host load drift and are
exploratory. Raw intensities, calibration and spectrum processing are preserved.

| Dataset | Backend | Frames median ms | Spectra median ms |
|---|---|---:|---:|
| F9477 (DIA) | timsrust 0.4.2 upstream | 377.093 | 295.605 |
| F9477 | OpenTIMS pread adapter | 324.937 | 199.162 |
| F9477 | OpenTIMS mmap adapter | 317.969 | 185.825 |
| G220824_007 (DDA) | timsrust 0.4.2 upstream | 1170.741 | 74.132 |
| G220824_007 | OpenTIMS pread adapter | 830.112 | 82.016 |
| G220824_007 | OpenTIMS mmap adapter | 632.173 | 52.311 |

Within each dataset, all three implementations produced the same frame and
spectrum fingerprints. Each also passed serial versus parallel spectrum parity.

| Dataset | Frame fingerprint | Spectrum fingerprint |
|---|---|---|
| F9477 | `7c8b7261693d4aeb` | `48216e97b72812c2` |
| G220824_007 | `d33d22db50b49158` | `479d6645869741d8` |

Sage's actual `sage-cloudpath` crate passed `cargo check` against the generated
adapter in an isolated copy of the adjacent Sage workspace, including its
default timsrust features. Original Sage files were not modified. This was a
compile check and reader regression test, not a complete Sage search benchmark.

Four Rust integration tests cover synthetic shapes, gapped IDs, empty scans and
frames, raw/normalized counts, saturation, open-source calibration endpoints,
corrupt scan counts, truncated packets, parallel reads, and real compressed
fixture parity. Clippy passed with warnings denied for the new core/binding.
Existing upstream timsrust dead-code/deprecation warnings remain in the adapter.

## Re-running

Build the standalone `compare` example and both versions of `compat-check` as
described in README.md. Copy each compat-check executable to a distinct path
before building the other version (Cargo uses the same binary name). Then:

```sh
python3 rust/run_comparison.py \
  --upstream /path/to/upstream-checker \
  --adapted /path/to/adapted-checker \
  --output /tmp/opentims-benchmarks.json \
  /path/to/dia.d /path/to/dda.d
```

Use an otherwise idle host and repeat the campaign before deciding to switch
Sage's default. The current evidence supports testing the mmap adapter further;
it does not justify assuming pread is faster.

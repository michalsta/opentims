<!-- Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki; SPDX-License-Identifier: MIT -->
# Experimental Rust OpenTIMS

This directory adds a native Rust reader and an owning Rust binding to the
existing C++ reader. The original C++, Python and R implementations are intact.
This is a port of the compression-type-2 frame reading path, not a replacement
R package or a port of the proprietary Bruker calibration bridge.

## Build and use

```sh
cargo test --manifest-path rust/opentims/Cargo.toml --features cpp
cargo run --release --manifest-path rust/opentims/Cargo.toml --features cpp \
  --example compare -- /path/to/run.d 9 1000
```

The last two arguments are repetitions and the maximum number of frames.
Omit the frame limit to read the entire dataset. C++ support needs a C++20
compiler and this source tree; it is disabled by default. SQLite and Zstandard
are built by Cargo dependencies, and both backends use those same libraries.
No Python, R or Bruker shared library is needed.

```rust
use opentims::{Reader, IoMode, IntensityMode, Workspace, Frame};

fn main() -> opentims::Result<()> {
let reader = Reader::open("/path/to/run.d", IoMode::Pread)?;
let mut workspace = Workspace::new()?;
let mut frame = Frame::default();
for meta in &reader.metadata.frames {
    reader.read_into(meta.id, IntensityMode::Raw, &mut workspace, &mut frame)?;
    // frame.scan_offsets, frame.tofs, frame.intensities
    // reader.metadata.calibration.mz(tof)
    // reader.metadata.calibration.inverse_mobility(scan)
}
Ok(())
}
```

Frame access uses actual database IDs, including gaps. `Reader` supports shared
concurrent access; give each worker its own `Workspace` and output `Frame`.
`read_into` reuses allocations and the Zstandard context. `read` is convenient
but creates temporary scratch space per call. Pread uses positioned reads on
Unix and `seek_read` on Windows, without a shared seek cursor. Mmap requires
the dataset to remain unmodified and untruncated for the reader's lifetime.
All readers assume immutable input datasets.

`IntensityMode::Normalized` follows C++ exactly:
`(raw * (100 / AccumulationTime) + 0.5)` converted to a saturating `u32`.
This normalizes exposure to 100 ms. `Raw` retains the acquired integer counts.
The open-source calibration is approximate; it does not provide Bruker pressure
compensation or per-frame proprietary calibration.

With feature `cpp`, `opentims::cpp::Reader::open(path)?.read(id)` returns the same
frame representation with normalized intensities. It owns and releases the
C++ handle, catches C++ exceptions, and requires a mutable borrow for reads.
It is deliberately neither `Send` nor `Sync`; use one handle per worker.
The binding currently exposes frame extraction, not the complete C++ API.

## Linux io_uring alternative

Build with feature `uring` to expose `opentims::uring::Reader`. This is a separate
reader with one ring per worker and exclusive access to reusable batch buffers:

```rust
let mut reader = opentims::uring::Reader::open("/path/to/run.d", 32)?;
let mut output = Vec::new();
reader.read_batch_into(&[1, 5, 2, 5], opentims::IntensityMode::Raw, &mut output)?;
```

The queue depth (1–1024) bounds each I/O batch. Larger ID lists are split into
batches. Header reads are submitted together, then payload reads together;
Zstandard decompression and decoding happen on the calling thread. Results
preserve input order, including duplicate IDs and empty frames. No pread
fallback is used: unavailable/disabled io_uring returns an error. This backend
does not require an async Rust runtime or change the existing Sage adapter.
Sage would need to schedule batches to benefit from this interface.

This implementation uses ordinary buffered positioned reads, without SQPOLL,
registered buffers, O_DIRECT, or overlap of decoding with the next batch's I/O.
Short reads are retried and all submitted completions are drained before normal
I/O errors are returned. On an unrecoverable submission/wait failure or panic
with requests in flight, the reader becomes unusable and its batch allocation
is intentionally retained, preventing the kernel from touching freed memory.
Normal completed read errors do not retain allocations.

```sh
cargo test --manifest-path rust/opentims/Cargo.toml --features cpp,uring
cargo build --release --manifest-path rust/opentims/Cargo.toml \
  --features cpp,uring --example compare
python3 rust/run_comparison.py --frames-only --uring-depths 1,8,32,128 \
  --output /tmp/uring-results.json /path/to/dia.d /path/to/dda.d
```

The comparison checks exact outputs before timing and rotates all backends
through the execution order. See [IO_URING.md](IO_URING.md) for measured results.
The `uring` tests require a Linux host that permits io_uring; they fail explicitly
if ring creation fails, rather than passing by silently skipping the backend.

## Parallel decoding and pipelining

For the matched mmap/parallel-decoding and io_uring/pipelined-decoding experiment,
see [PARALLEL_COMPARISON.md](PARALLEL_COMPARISON.md). Its benchmark requires
features `uring,parallel` and keeps two owned compressed batches in circulation.
`uring::Reader::read_compressed_batch_into` transfers completed payloads to an
owned `CompressedBatch`; `Workspace::decode_compressed` lets independent workers
decode those payloads while the producer reads the next batch. mmap uses the
same decoder through `Reader::compressed_frame`. Neither API changes Sage by
itself; the comparison lives in `examples/parallel_compare.rs`.

## Sage / timsrust 0.4.2

The adapter recipe copies the pinned upstream crate into a separate directory
and replaces its compression-type-2 frame decoding with native OpenTIMS.
It retains upstream metadata, precursor grouping, DDA/DIA spectrum processing,
calibration, smoothing, centroiding, and the existing public API. Upstream's
Apache-2.0 license remains in the generated directory. The Cargo registry and
Sage checkout are not edited by the recipe. Python 3.11+ is required.

```sh
python3 rust/prepare_timsrust.py \
  "$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/timsrust-0.4.2" \
  /tmp/opentims-timsrust
```

Use the actual registry source location on your machine. In Sage's workspace
Cargo.toml, add this entry to its **existing** `[patch.crates-io]` section:

```toml
timsrust = { path = "/tmp/opentims-timsrust" }
```

Alternatively pass `--config 'patch.crates-io.timsrust.path="/tmp/opentims-timsrust"'`
to Cargo. No changes to `sage-cloudpath/src/tdf.rs` are needed. Set
`OPENTIMS_IO=mmap` to choose mmap; the adapter defaults to `pread`.
The adapter intentionally supplies **raw intensities**, matching timsrust.
Its ordinary `get` API allocates scratch space per frame; the standalone native
API also supports reusable worker buffers for further integration work.

The adapter targets normal TDF compression type 2. The optional timscompress
type-3 path is not supported by this adapter. Upstream MiniTDF code remains
upstream code and does not use OpenTIMS. Existing upstream assumptions about
contiguous frame IDs in spectrum assembly remain; native frame access itself
supports gapped IDs.

## Reproduce compatibility checks

```sh
# Upstream reference:
cargo run --release --manifest-path rust/compat-check/Cargo.toml \
  -- /path/to/run.d 1000
# Adapter:
cargo run --release --manifest-path rust/compat-check/Cargo.toml \
  --config 'patch.crates-io.timsrust.path="/tmp/opentims-timsrust"' \
  -- /path/to/run.d 1000
```

Compare frame and spectrum fingerprints between runs using the same toolchain.
The checker also verifies serial/parallel spectrum fingerprints within each run.
This is a regression check, not a portable checksum format. For native/C++
comparison, `compare` checks every selected output element before timing.

## Benchmark interpretation

See [BENCHMARKS.md](BENCHMARKS.md). The benchmark compares complete frame results
with normalized intensities and scan offsets. C++ emits per-peak scan IDs, so
its wrapper additionally constructs scan offsets. Both paths retain buffers
between timed calls. Opening is measured separately; startup ordering affects
the file cache, so opening times are diagnostic, not a fair speed ranking.
Backend order rotates across repetitions. Validation warms the cache first.

These measurements do not cover cold storage, random access, whole Sage
searches, or multithreaded throughput. Pread avoids mapping and shared seek
state, but needs two system calls per frame (header and payload), and still
decompresses the entire frame. It cannot directly fetch one spectrum from
inside a Zstandard-compressed frame. Grouping spectra by frame and reusing
decoded frames may matter more than choosing pread versus mmap.

Linux x86-64 has been tested. Windows positioned reads and the platform-neutral
build definitions are implemented, but Windows and macOS have not been tested.

<!-- Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki; SPDX-License-Identifier: MIT -->
# Experimental Rust OpenTIMS

A native Rust reader for compression-type-2 TDF frames. The C++, Python and R
implementations are unchanged. This is not a port of the proprietary Bruker
calibration bridge.

## Build and use

```sh
cargo test --manifest-path rust/opentims/Cargo.toml
```

SQLite and Zstandard are built by Cargo dependencies. No Python, R or Bruker
shared library is needed.

```rust
use opentims::{Frame, IoMode, Reader, Workspace};

fn main() -> opentims::Result<()> {
    let reader = Reader::open("/path/to/run.d", IoMode::Pread)?;
    let mut workspace = Workspace::new()?;
    let mut frame = Frame::default();
    for meta in &reader.metadata.frames {
        reader.read_into(meta.id, &mut workspace, &mut frame)?;
        // frame.scan_offsets, frame.tofs, frame.intensities
    }
    Ok(())
}
```

Frames are addressed by their database IDs, gaps included. Intensities are raw
acquired counts; the C++ reader normalises them to a 100 ms accumulation time.
`Reader` can be shared between threads; give each worker its own `Workspace`
and `Frame`. `read` is the convenient form and allocates scratch space per call.
Pread uses positioned reads with no shared seek cursor. Both modes assume the
dataset is not modified or truncated while open.

`metadata.calibration` is the approximate open-source calibration, without
Bruker pressure compensation or per-frame calibration.

## Sage / timsrust 0.4.2

`prepare_timsrust.py` copies the pinned upstream timsrust crate into a new
directory and replaces its compression-type-2 frame decoding with this reader.
Everything else in timsrust (metadata, precursor grouping, spectrum processing,
calibration, public API) and its Apache-2.0 licence are kept. Python 3.11+.

```sh
python3 rust/prepare_timsrust.py \
  "$HOME/.cargo/registry/src/index.crates.io-1949cf8c6b5b557f/timsrust-0.4.2" \
  /tmp/opentims-timsrust
```

Then add `timsrust = { path = "/tmp/opentims-timsrust" }` to Sage's existing
`[patch.crates-io]`, or pass
`--config 'patch.crates-io.timsrust.path="/tmp/opentims-timsrust"'` to Cargo.
`OPENTIMS_IO=mmap` selects mmap; the default is pread. The adapter supplies raw
intensities, as timsrust does. Type-3 (timscompress) and MiniTDF stay upstream.

## Compatibility check

```sh
cargo run --release --manifest-path rust/compat-check/Cargo.toml -- /path/to/run.d 1000
cargo run --release --manifest-path rust/compat-check/Cargo.toml \
  --config 'patch.crates-io.timsrust.path="/tmp/opentims-timsrust"' -- /path/to/run.d 1000
```

Frame and spectrum fingerprints must match between the two runs (same toolchain).
`run_comparison.py` runs both checkers on several datasets and compares them.
Results: [BENCHMARKS.md](BENCHMARKS.md).

Tested on Linux x86-64 only.

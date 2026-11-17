// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
//! Run unchanged against upstream and patched timsrust. Compare fingerprints.
use rayon::prelude::*;
use std::{
    collections::hash_map::DefaultHasher,
    hash::{Hash, Hasher},
    time::Instant,
};
use timsrust::readers::{FrameReader, SpectrumReader};
fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<_> = std::env::args().collect();
    let path = args
        .get(1)
        .ok_or("usage: compat-check DATA.d [limit=1000]")?;
    let limit: usize = args.get(2).map(|s| s.parse()).transpose()?.unwrap_or(1000);
    let reader = FrameReader::new(path.as_str())?;
    let n = reader.len().min(limit);
    let mut hash = DefaultHasher::new();
    for i in 0..n {
        let f = reader.get(i)?;
        f.tof_indices.hash(&mut hash);
        f.scan_offsets.hash(&mut hash);
        f.intensities.hash(&mut hash);
    }
    let mut samples = vec![];
    for _ in 0..7 {
        let start = Instant::now();
        for i in 0..n {
            std::hint::black_box(reader.get(i)?);
        }
        samples.push(start.elapsed().as_secs_f64());
    }
    samples.sort_by(f64::total_cmp);
    println!(
        "frames={n} hash={:016x} median_ms={:.3}",
        hash.finish(),
        samples[3] * 1000.
    );
    let reader = SpectrumReader::build()
        .with_path(path.as_str())
        .finalize()?;
    let n = reader.len().min(limit);
    let mut hash = DefaultHasher::new();
    for i in 0..n {
        let s = reader.get(i)?;
        format!("{s:?}").hash(&mut hash);
    }
    let mut samples = vec![];
    for _ in 0..7 {
        let start = Instant::now();
        for i in 0..n {
            std::hint::black_box(reader.get(i)?);
        }
        samples.push(start.elapsed().as_secs_f64());
    }
    samples.sort_by(f64::total_cmp);
    // Sage reads spectra concurrently. Check that path as well.
    let parallel: Vec<_> = (0..n)
        .into_par_iter()
        .map(|i| reader.get(i).map(|s| format!("{s:?}")))
        .collect();
    let mut parallel_hash = DefaultHasher::new();
    for s in parallel {
        s?.hash(&mut parallel_hash);
    }
    assert_eq!(hash.finish(), parallel_hash.finish());
    println!(
        "spectra={n} hash={:016x} median_ms={:.3}; parallel parity passed",
        hash.finish(),
        samples[3] * 1000.
    );
    Ok(())
}

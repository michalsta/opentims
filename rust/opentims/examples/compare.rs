// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
use opentims::{cpp, Frame, IntensityMode, IoMode, Reader, Result, Workspace};
use std::{hint::black_box, time::Instant};
fn main() -> Result<()> {
    let args: Vec<_> = std::env::args().collect();
    let path = args.get(1).ok_or(
        "usage: compare DATA.d [repetitions=7] [max_frames=all] [uring_depths=1,8,32,128]",
    )?;
    let reps: usize = args.get(2).map(|s| s.parse()).transpose()?.unwrap_or(7);
    if reps == 0 {
        return Err("repetitions must be positive".into());
    }
    let limit: usize = args
        .get(3)
        .map(|s| s.parse())
        .transpose()?
        .unwrap_or(usize::MAX);
    let start = Instant::now();
    let pread = Reader::open(path, IoMode::Pread)?;
    let pread_open = start.elapsed();
    let start = Instant::now();
    let mmap = Reader::open(path, IoMode::Mmap)?;
    let mmap_open = start.elapsed();
    let start = Instant::now();
    let mut cpp = cpp::Reader::open(path)?;
    let cpp_open = start.elapsed();
    let backends = vec![
        ("rust-pread".to_string(), pread_open),
        ("rust-mmap".to_string(), mmap_open),
        ("cpp-mmap".to_string(), cpp_open),
    ];
    #[cfg(all(feature = "uring", target_os = "linux"))]
    let (backends, mut rings) = {
        let mut backends = backends;
        let mut rings = Vec::new();
        for depth in args
            .get(4)
            .map(String::as_str)
            .unwrap_or("1,8,32,128")
            .split(',')
        {
            let depth: u32 = depth.parse()?;
            let start = Instant::now();
            let reader = opentims::uring::Reader::open(path, depth)?;
            backends.push((format!("rust-uring-{depth}"), start.elapsed()));
            rings.push((reader, Vec::new()));
        }
        (backends, rings)
    };
    let ids: Vec<_> = pread
        .metadata
        .frames
        .iter()
        .take(limit)
        .map(|f| f.id)
        .collect();
    let peaks: u64 = pread
        .metadata
        .frames
        .iter()
        .take(limit)
        .map(|f| u64::from(f.num_peaks))
        .sum();
    if ids.is_empty() {
        return Err("no frames selected".into());
    }
    let mut workspace = Workspace::new()?;
    let mut out = Frame::default();
    // Full comparison outside timing also warms the file cache equally.
    for &id in &ids {
        let a = pread.read(id, IntensityMode::Normalized)?;
        assert_eq!(
            a,
            mmap.read(id, IntensityMode::Normalized)?,
            "mmap mismatch frame {id}"
        );
        assert_eq!(a, cpp.read(id)?, "C++ mismatch frame {id}");
    }
    #[cfg(all(feature = "uring", target_os = "linux"))]
    for (ring, output) in &mut rings {
        for chunk in ids.chunks(ring.queue_depth()) {
            ring.read_batch_into(chunk, IntensityMode::Normalized, output)?;
            for (&id, actual) in chunk.iter().zip(output.iter()) {
                assert_eq!(
                    *actual,
                    pread.read(id, IntensityMode::Normalized)?,
                    "io_uring mismatch frame {id}"
                );
            }
        }
    }
    println!(
        "frames={}, peaks={}, repetitions={}, warm-cache, one thread",
        ids.len(),
        peaks,
        reps
    );
    let mut samples = vec![vec![]; backends.len()];
    for rep in 0..reps {
        for turn in 0..backends.len() {
            let backend = (rep + turn) % backends.len();
            let start = Instant::now();
            if backend < 3 {
                for &id in &ids {
                    match backend {
                        0 => pread.read_into(
                            id,
                            IntensityMode::Normalized,
                            &mut workspace,
                            &mut out,
                        )?,
                        1 => {
                            mmap.read_into(id, IntensityMode::Normalized, &mut workspace, &mut out)?
                        }
                        _ => cpp.read_into(id, &mut out)?,
                    }
                    black_box(&out);
                }
            } else {
                #[cfg(all(feature = "uring", target_os = "linux"))]
                {
                    let (ring, output) = &mut rings[backend - 3];
                    for chunk in ids.chunks(ring.queue_depth()) {
                        ring.read_batch_into(chunk, IntensityMode::Normalized, output)?;
                        black_box(&output);
                    }
                }
            }
            samples[backend].push(start.elapsed().as_secs_f64());
        }
    }
    println!("backend,open_ms,median_ms,min_ms,max_ms,million_peaks_per_second");
    for ((name, open), mut times) in backends.into_iter().zip(samples) {
        times.sort_by(f64::total_cmp);
        let median = times[times.len() / 2];
        println!(
            "{name},{:.3},{:.3},{:.3},{:.3},{:.3}",
            open.as_secs_f64() * 1000.,
            median * 1000.,
            times[0] * 1000.,
            times[times.len() - 1] * 1000.,
            peaks as f64 / median / 1e6
        );
    }
    Ok(())
}

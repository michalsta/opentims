// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
//! Linux-only comparison; one subprocess per configuration isolates peak RSS.
#[cfg(not(target_os = "linux"))]
fn main() {
    panic!("parallel_compare requires Linux");
}
#[cfg(target_os = "linux")]
fn main() -> opentims::Result<()> {
    linux::run()
}

#[cfg(target_os = "linux")]
mod linux {
    use opentims::{
        uring::{self, CompressedBatch},
        Frame, IntensityMode, IoMode, Reader, Result, Workspace,
    };
    use rayon::prelude::*;
    use std::{fs::File, os::fd::AsRawFd, path::Path, sync::mpsc::sync_channel, time::Instant};

    struct Decoder {
        pool: rayon::ThreadPool,
        scratch: Vec<Workspace>,
        output: Vec<Frame>,
    }
    impl Decoder {
        fn new(workers: usize) -> Result<Self> {
            Ok(Self {
                pool: rayon::ThreadPoolBuilder::new()
                    .num_threads(workers)
                    .build()?,
                scratch: (0..workers)
                    .map(|_| Workspace::new())
                    .collect::<Result<_>>()?,
                output: vec![],
            })
        }
        fn mmap(&mut self, reader: &Reader, ids: &[u32]) -> Result<()> {
            self.output.resize_with(ids.len(), Frame::default);
            let chunk = ids.len().div_ceil(self.scratch.len());
            self.pool.install(|| {
                self.scratch
                    .par_iter_mut()
                    .zip(self.output.par_chunks_mut(chunk))
                    .enumerate()
                    .try_for_each(|(worker, (scratch, output))| {
                        for (j, out) in output.iter_mut().enumerate() {
                            let (meta, payload) =
                                reader.compressed_frame(ids[worker * chunk + j])?;
                            scratch.decode_compressed(
                                meta,
                                payload,
                                IntensityMode::Normalized,
                                out,
                            )?;
                        }
                        Ok(())
                    })
            })
        }
        fn compressed(&mut self, batch: &CompressedBatch) -> Result<()> {
            self.output
                .resize_with(batch.metadata.len(), Frame::default);
            let chunk = batch.metadata.len().div_ceil(self.scratch.len());
            self.pool.install(|| {
                self.scratch
                    .par_iter_mut()
                    .zip(self.output.par_chunks_mut(chunk))
                    .enumerate()
                    .try_for_each(|(worker, (scratch, output))| {
                        for (j, out) in output.iter_mut().enumerate() {
                            let i = worker * chunk + j;
                            scratch.decode_compressed(
                                &batch.metadata[i],
                                batch.payload(i),
                                IntensityMode::Normalized,
                                out,
                            )?;
                        }
                        Ok(())
                    })
            })
        }
        fn consume(&self, ids: &[u32], reference: Option<&Reader>) -> Result<()> {
            if let Some(reference) = reference {
                for (&id, actual) in ids.iter().zip(&self.output) {
                    if *actual != reference.read(id, IntensityMode::Normalized)? {
                        return Err(format!("output mismatch frame {id}").into());
                    }
                }
            }
            std::hint::black_box(&self.output);
            Ok(())
        }
    }

    #[allow(clippy::too_many_arguments)] // Explicit benchmark inputs, identical across paths.
    fn pass(
        backend: &str,
        mmap: &Reader,
        ring: &mut uring::Reader,
        ids: &[u32],
        size: usize,
        decoder: &mut Decoder,
        buffers: &mut Vec<CompressedBatch>,
        reference: Option<&Reader>,
        mut latency: Option<&mut Vec<f64>>,
    ) -> Result<()> {
        let mut last = Instant::now();
        let mut delivered = || {
            if let Some(samples) = latency.as_deref_mut() {
                samples.push(last.elapsed().as_secs_f64());
            }
            last = Instant::now();
        };
        match backend {
            "mmap" => {
                for chunk in ids.chunks(size) {
                    decoder.mmap(mmap, chunk)?;
                    decoder.consume(chunk, reference)?;
                    delivered();
                }
            }
            "uring-sync" => {
                for chunk in ids.chunks(size) {
                    ring.read_compressed_batch_into(chunk, &mut buffers[0])?;
                    decoder.compressed(&buffers[0])?;
                    decoder.consume(chunk, reference)?;
                    delivered();
                }
            }
            "uring-pipeline" => {
                std::thread::scope(|scope| -> Result<()> {
                    let (ready_tx, ready_rx) = sync_channel::<CompressedBatch>(1);
                    let (free_tx, free_rx) = sync_channel::<CompressedBatch>(2);
                    for batch in buffers.drain(..) {
                        free_tx.send(batch)?;
                    }
                    let producer = scope.spawn(move || -> Result<Vec<CompressedBatch>> {
                        for chunk in ids.chunks(size) {
                            let mut batch = free_rx.recv()?;
                            ring.read_compressed_batch_into(chunk, &mut batch)?;
                            ready_tx.send(batch)?;
                        }
                        // Keep returned allocations for the next pass, avoiding reallocations.
                        Ok(vec![free_rx.recv()?, free_rx.recv()?])
                    });
                    let result = (|| -> Result<()> {
                        let mut offset = 0;
                        for batch in &ready_rx {
                            decoder.compressed(&batch)?;
                            let count = batch.metadata.len();
                            decoder.consume(&ids[offset..offset + count], reference)?;
                            delivered();
                            offset += count;
                            free_tx.send(batch)?;
                        }
                        if offset != ids.len() {
                            return Err("pipeline ended before all frames arrived".into());
                        }
                        Ok(())
                    })();
                    drop(ready_rx);
                    drop(free_tx);
                    let produced = producer.join().map_err(|_| "I/O producer panicked")?;
                    result?;
                    *buffers = produced?;
                    Ok(())
                })?;
            }
            _ => return Err("backend must be mmap, uring-sync or uring-pipeline".into()),
        }
        Ok(())
    }

    fn usage() -> libc::rusage {
        let mut out = std::mem::MaybeUninit::uninit();
        // SAFETY: getrusage initializes the complete output on success.
        let rc = unsafe { libc::getrusage(libc::RUSAGE_SELF, out.as_mut_ptr()) };
        assert_eq!(rc, 0);
        unsafe { out.assume_init() }
    }
    fn cpu(u: &libc::rusage) -> f64 {
        u.ru_utime.tv_sec as f64
            + u.ru_utime.tv_usec as f64 / 1e6
            + u.ru_stime.tv_sec as f64
            + u.ru_stime.tv_usec as f64 / 1e6
    }
    fn read_bytes() -> u64 {
        std::fs::read_to_string("/proc/self/io")
            .unwrap()
            .lines()
            .find_map(|line| {
                line.strip_prefix("read_bytes: ")
                    .map(|s| s.parse().unwrap())
            })
            .unwrap()
    }
    fn peak_rss() -> u64 {
        std::fs::read_to_string("/proc/self/status")
            .unwrap()
            .lines()
            .find_map(|line| {
                line.strip_prefix("VmHWM:")
                    .map(|s| s.split_whitespace().next().unwrap().parse().unwrap())
            })
            .unwrap()
    }
    fn evict(file: &File) -> Result<()> {
        // Advisory eviction of this dataset's clean file pages only; no global drop_caches.
        let rc = unsafe { libc::posix_fadvise(file.as_raw_fd(), 0, 0, libc::POSIX_FADV_DONTNEED) };
        if rc != 0 {
            return Err(std::io::Error::from_raw_os_error(rc).into());
        }
        Ok(())
    }
    fn resident_pages(file: &File) -> Result<(usize, usize)> {
        let map = unsafe { memmap2::Mmap::map(file)? };
        let page = unsafe { libc::sysconf(libc::_SC_PAGESIZE) } as usize;
        let mut residency = vec![0u8; map.len().div_ceil(page)];
        let rc = unsafe {
            libc::mincore(
                map.as_ptr() as *mut libc::c_void,
                map.len(),
                residency.as_mut_ptr(),
            )
        };
        if rc != 0 {
            return Err(std::io::Error::last_os_error().into());
        }
        Ok((
            residency.iter().filter(|&&v| v & 1 != 0).count(),
            residency.len(),
        ))
    }

    pub fn run() -> Result<()> {
        let args: Vec<_> = std::env::args().collect();
        if args.len() != 9 {
            return Err(
                "usage: parallel_compare DATA.d BACKEND WORKERS BATCH FRAMES ORDER REPS CACHE"
                    .into(),
            );
        }
        let path = Path::new(&args[1]);
        let backend = &args[2];
        let workers: usize = args[3].parse()?;
        let batch: usize = args[4].parse()?;
        let count: usize = args[5].parse()?;
        let order = &args[6];
        let reps: usize = args[7].parse()?;
        let cache = &args[8];
        if workers == 0 || batch == 0 || batch > 1024 || reps == 0 || count == 0 {
            return Err("invalid zero/oversized configuration".into());
        }
        if !["warm", "cold"].contains(&cache.as_str()) {
            return Err("cache must be warm or cold".into());
        }
        let mut mmap = Some(Reader::open(path, IoMode::Mmap)?);
        let mut ring = uring::Reader::open(path, batch as u32)?;
        let mut ids: Vec<_> = mmap
            .as_ref()
            .unwrap()
            .metadata
            .frames
            .iter()
            .take(count)
            .map(|f| f.id)
            .collect();
        if ids.is_empty() {
            return Err("empty frame selection".into());
        }
        let peaks: u64 = mmap
            .as_ref()
            .unwrap()
            .metadata
            .frames
            .iter()
            .take(count)
            .map(|f| u64::from(f.num_peaks))
            .sum();
        if order == "shuffled" {
            let mut state = 0x4f50454e54494d53u64;
            for i in (1..ids.len()).rev() {
                state ^= state << 13;
                state ^= state >> 7;
                state ^= state << 17;
                ids.swap(i, state as usize % (i + 1));
            }
        } else if order != "sequential" {
            return Err("order must be sequential or shuffled".into());
        }
        let mut decoder = Decoder::new(workers)?;
        let mut buffers = vec![CompressedBatch::default(), CompressedBatch::default()];
        {
            let reference = Reader::open(path, IoMode::Pread)?;
            pass(
                backend,
                mmap.as_ref().unwrap(),
                &mut ring,
                &ids,
                batch,
                &mut decoder,
                &mut buffers,
                Some(&reference),
                None,
            )?;
        }
        let file = File::open(path.join("analysis.tdf_bin"))?;
        for repetition in 0..reps {
            if cache == "cold" {
                drop(mmap.take());
                evict(&file)?;
                mmap = Some(Reader::open(path, IoMode::Mmap)?);
            }
            let (resident, total_pages) = resident_pages(&file)?;
            let io_before = read_bytes();
            let before = usage();
            let start = Instant::now();
            let mut latency = Vec::with_capacity(ids.len().div_ceil(batch));
            pass(
                backend,
                mmap.as_ref().unwrap(),
                &mut ring,
                &ids,
                batch,
                &mut decoder,
                &mut buffers,
                None,
                Some(&mut latency),
            )?;
            let wall = start.elapsed().as_secs_f64();
            let after = usage();
            let io = read_bytes() - io_before;
            latency.sort_by(f64::total_cmp);
            let p50 = latency[latency.len() / 2];
            let p95 = latency[(latency.len() * 95 / 100).min(latency.len() - 1)];
            println!("{{\"latency\":true,\"rep\":{repetition},\"batch_delivery_p50_s\":{p50},\"batch_delivery_p95_s\":{p95}}}");
            println!("{{\"rep\":{repetition},\"wall_s\":{wall},\"cpu_s\":{},\"peak_rss_kib\":{},\"major_faults\":{},\"minor_faults\":{},\"read_bytes\":{io},\"resident_pages_before\":{resident},\"file_pages\":{total_pages},\"frames\":{},\"peaks\":{peaks}}}",cpu(&after)-cpu(&before),peak_rss(),after.ru_majflt-before.ru_majflt,after.ru_minflt-before.ru_minflt,ids.len());
        }
        // Instrumented serial profile is separate from timed throughput samples.
        if backend == "mmap"
            && workers == 1
            && batch == 8
            && order == "sequential"
            && cache == "warm"
        {
            let mut scratch = Workspace::new()?;
            let mut output = Frame::default();
            let (mut access, mut decompression, mut decode) = (0., 0., 0.);
            for &id in &ids {
                let start = Instant::now();
                let (meta, payload) = mmap.as_ref().unwrap().compressed_frame(id)?;
                access += start.elapsed().as_secs_f64();
                let (d, c) = scratch.profile_compressed(
                    meta,
                    payload,
                    IntensityMode::Normalized,
                    &mut output,
                )?;
                decompression += d;
                decode += c;
            }
            println!("{{\"profile\":true,\"mmap_header_access_s\":{access},\"decompression_s\":{decompression},\"decode_and_output_s\":{decode}}}");
            // Positioned-read profile makes copying/I/O visible separately. mmap
            // demand faults necessarily remain charged to accesses inside zstd.
            use std::os::unix::fs::FileExt;
            let (mut io, mut decomp, mut decoded) = (0., 0., 0.);
            let mut compressed = Vec::new();
            let mut header = [0u8; 8];
            for &id in &ids {
                let (meta, payload) = mmap.as_ref().unwrap().compressed_frame(id)?;
                if meta.num_peaks == 0 {
                    continue;
                }
                let start = Instant::now();
                compressed.resize(payload.len(), 0);
                file.read_exact_at(&mut header, meta.offset)?;
                file.read_exact_at(&mut compressed, meta.offset + 8)?;
                io += start.elapsed().as_secs_f64();
                let (d, c) = scratch.profile_compressed(
                    meta,
                    &compressed,
                    IntensityMode::Normalized,
                    &mut output,
                )?;
                decomp += d;
                decoded += c;
            }
            println!("{{\"profile\":true,\"pread_io_s\":{io},\"decompression_s\":{decomp},\"decode_and_output_s\":{decoded}}}");
        }
        Ok(())
    }
}

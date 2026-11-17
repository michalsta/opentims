// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
//! Linux batched positioned reads. No pread fallback: unavailable io_uring is an error.
use crate::{decode, Frame, IntensityMode, Metadata, Result, Workspace};
use io_uring::{opcode, types, IoUring};
use std::{fs::File, io, mem::ManuallyDrop, os::fd::AsRawFd, path::Path};

/// One ring per reader/worker. Exclusive access protects the ring and scratch space.
/// Each batch issues all headers, then all payloads, then decodes in request order.
pub struct Reader {
    ring: IoUring,
    file: File,
    pub metadata: Metadata,
    file_len: u64,
    depth: usize,
    buffers: ManuallyDrop<Vec<Vec<u8>>>,
    in_flight: bool,
    workspace: Workspace,
}
impl Reader {
    pub fn open(path: impl AsRef<Path>, depth: u32) -> Result<Self> {
        if !(1..=1024).contains(&depth) {
            return Err("io_uring queue depth must be between 1 and 1024".into());
        }
        let ring = IoUring::new(depth)?;
        let metadata = Metadata::open(&path)?;
        let file = File::open(path.as_ref().join("analysis.tdf_bin"))?;
        let file_len = file.metadata()?.len();
        Ok(Self {
            ring,
            file,
            metadata,
            file_len,
            depth: depth as usize,
            buffers: ManuallyDrop::new(vec![Vec::new(); depth as usize]),
            in_flight: false,
            workspace: Workspace::new()?,
        })
    }

    pub fn queue_depth(&self) -> usize {
        self.depth
    }

    /// Read any number of frames, in bounded batches; IDs may repeat or have gaps.
    /// Output buffers and compressed storage are reused across calls.
    pub fn read_batch_into(
        &mut self,
        ids: &[u32],
        mode: IntensityMode,
        out: &mut Vec<Frame>,
    ) -> Result<()> {
        if self.in_flight {
            return Err("io_uring reader is poisoned after a submission failure".into());
        }
        out.resize_with(ids.len(), Frame::default);
        for (ids, output) in ids.chunks(self.depth).zip(out.chunks_mut(self.depth)) {
            let metas = self.read_payloads(ids)?;
            for (i, (meta, out)) in metas.iter().zip(output).enumerate() {
                out.tofs.clear();
                out.intensities.clear();
                out.scan_offsets.clear();
                if meta.num_peaks == 0 {
                    out.scan_offsets.resize(meta.num_scans as usize + 1, 0);
                    continue;
                }
                let words = u64::from(meta.num_scans) + 2 * u64::from(meta.num_peaks);
                if words > u32::MAX as u64 {
                    return Err("frame exceeds supported word count".into());
                }
                let bytes = usize::try_from(words * 4)?;
                self.workspace.decoded.resize(bytes, 0);
                if self
                    .workspace
                    .zstd
                    .decompress_to_buffer(&self.buffers[i][..], &mut self.workspace.decoded[..])?
                    != bytes
                {
                    return Err("decompressed size mismatch".into());
                }
                decode(&self.workspace.decoded, meta, mode, out)?;
            }
        }
        Ok(())
    }

    fn read_payloads(&mut self, ids: &[u32]) -> Result<Vec<crate::FrameMeta>> {
        let metas = ids
            .iter()
            .map(|&id| self.metadata.frame(id).cloned())
            .collect::<Result<Vec<_>>>()?;
        let mut ranges = Vec::with_capacity(ids.len());
        for meta in &metas {
            if meta.num_peaks == 0 {
                ranges.push((0, 0));
                continue;
            }
            if meta.num_scans == 0 {
                return Err("frame has peaks but no scans".into());
            }
            if meta.offset > self.file_len || self.file_len - meta.offset < 8 {
                return Err("truncated frame header".into());
            }
            ranges.push((meta.offset, 8));
        }
        self.read_many(&ranges)?;
        for (i, meta) in metas.iter().enumerate() {
            if meta.num_peaks == 0 {
                continue;
            }
            let header = &self.buffers[i];
            let size = u32::from_le_bytes(header[..4].try_into()?) as usize;
            if u32::from_le_bytes(header[4..8].try_into()?) != meta.num_scans {
                return Err("frame scan count mismatch".into());
            }
            if size < 8 || size as u64 > self.file_len - meta.offset {
                return Err("invalid frame packet length".into());
            }
            ranges[i] = (meta.offset + 8, size - 8);
        }
        self.read_many(&ranges)?;
        Ok(metas)
    }

    /// Transfer completed payloads into an owned batch. No kernel request remains
    /// when this returns successfully, so decoding can proceed on other threads.
    pub fn read_compressed_batch_into(
        &mut self,
        ids: &[u32],
        batch: &mut CompressedBatch,
    ) -> Result<()> {
        if self.in_flight {
            return Err("io_uring reader is poisoned".into());
        }
        if ids.len() > self.depth {
            return Err("compressed batch exceeds queue depth".into());
        }
        batch.payloads.resize_with(self.depth, Vec::new);
        std::mem::swap(&mut *self.buffers, &mut batch.payloads);
        // On error leave pending buffers owned by the ring, never by the caller.
        let metas = self.read_payloads(ids)?;
        std::mem::swap(&mut *self.buffers, &mut batch.payloads);
        batch.metadata = metas;
        Ok(())
    }

    // No borrowed output buffer escapes to the kernel. The reader owns all buffers.
    fn read_many(&mut self, ranges: &[(u64, usize)]) -> Result<()> {
        let mut progress = vec![0; ranges.len()];
        for (buffer, &(_, size)) in self.buffers.iter_mut().zip(ranges) {
            buffer.resize(size, 0);
        }
        loop {
            let active: Vec<_> = ranges
                .iter()
                .enumerate()
                .filter_map(|(i, &(_, size))| (progress[i] < size).then_some(i))
                .collect();
            if active.is_empty() {
                return Ok(());
            }
            let mut results = vec![None; ranges.len()];
            // Set before publishing any SQE: panic/error paths must retain kernel buffers.
            self.in_flight = true;
            for &i in &active {
                let remaining = ranges[i].1 - progress[i];
                let len = u32::try_from(remaining.min(i32::MAX as usize))?;
                let sqe = opcode::Read::new(
                    types::Fd(self.file.as_raw_fd()),
                    self.buffers[i][progress[i]..].as_mut_ptr(),
                    len,
                )
                .offset(ranges[i].0 + progress[i] as u64)
                .build()
                .user_data(i as u64);
                // SAFETY: buffers are owned, never resized/read while in flight, and
                // retained on fatal submission errors or unwinding. File stays open.
                unsafe {
                    self.ring
                        .submission()
                        .push(&sqe)
                        .map_err(|_| "io_uring submission queue unexpectedly full")?;
                }
            }
            let mut completed = 0;
            while completed < active.len() {
                match self.ring.submit_and_wait(1) {
                    Err(e) if e.kind() == io::ErrorKind::Interrupted => continue,
                    Err(e) => return Err(e.into()),
                    Ok(_) => {}
                }
                for cqe in self.ring.completion() {
                    let i = usize::try_from(cqe.user_data())?;
                    if !active.contains(&i) || results[i].is_some() {
                        return Err("invalid or duplicate io_uring completion".into());
                    }
                    results[i] = Some(cqe.result());
                    completed += 1;
                }
            }
            // All published operations completed, including failed ones. Buffers safe.
            self.in_flight = false;
            for i in active {
                let result = results[i].ok_or("missing io_uring completion")?;
                if result < 0 {
                    let error = io::Error::from_raw_os_error(-result);
                    if error.kind() == io::ErrorKind::Interrupted {
                        continue;
                    }
                    return Err(error.into());
                }
                if result == 0 {
                    return Err(io::Error::from(io::ErrorKind::UnexpectedEof).into());
                }
                progress[i] += result as usize;
                if progress[i] > ranges[i].1 {
                    return Err("io_uring read exceeds requested buffer".into());
                }
            }
            // Short reads are retried at their new offsets; EOF is reported above.
        }
    }
}
impl Drop for Reader {
    fn drop(&mut self) {
        if !self.in_flight {
            // SAFETY: every submitted request has completed; no kernel references remain.
            unsafe {
                ManuallyDrop::drop(&mut self.buffers);
            }
        }
        // On unrecoverable submit/wait errors (or panic while publishing SQEs),
        // intentionally retain the bounded batch allocation. Closing a ring alone
        // is not proof that its kernel requests stopped touching userspace memory.
        // Subsequent reads are rejected. Normal I/O errors drain CQEs and do not leak.
    }
}

/// Owned compressed frames, reusable through a bounded producer/consumer queue.
#[derive(Default)]
pub struct CompressedBatch {
    pub metadata: Vec<crate::FrameMeta>,
    payloads: Vec<Vec<u8>>,
}
impl CompressedBatch {
    pub fn payload(&self, index: usize) -> &[u8] {
        &self.payloads[index]
    }
}

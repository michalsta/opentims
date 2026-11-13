// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
//! Read-only compression-type-2 TDF reader. Do not modify/truncate files while open.
use rusqlite::{Connection, OpenFlags};
use std::{collections::BTreeMap, fs::File, path::Path};

pub type Error = Box<dyn std::error::Error + Send + Sync>;
pub type Result<T> = std::result::Result<T, Error>;
#[cfg(feature = "cpp")]
pub mod cpp;

#[derive(Clone, Copy, Debug)]
pub enum IoMode {
    Pread,
    Mmap,
}
#[derive(Clone, Copy, Debug)]
pub enum IntensityMode {
    Raw,
    Normalized,
}
#[derive(Clone, Debug)]
pub struct FrameMeta {
    pub id: u32,
    pub num_scans: u32,
    pub num_peaks: u32,
    pub msms_type: u32,
    pub accumulation_time: f64,
    pub time: f64,
    pub offset: u64,
}
#[derive(Default, Debug, PartialEq)]
pub struct Frame {
    pub scan_offsets: Vec<usize>,
    pub tofs: Vec<u32>,
    pub intensities: Vec<u32>,
}
#[derive(Clone, Debug)]
pub struct Calibration {
    mz_intercept: f64,
    mz_slope: f64,
    im_intercept: f64,
    im_slope: f64,
}
impl Calibration {
    pub fn mz(&self, tof: u32) -> f64 {
        let x = self.mz_intercept + self.mz_slope * tof as f64;
        x * x
    }
    pub fn inverse_mobility(&self, scan: f64) -> f64 {
        self.im_intercept + self.im_slope * scan
    }
}
#[derive(Debug)]
pub struct Metadata {
    pub frames: Vec<FrameMeta>,
    pub calibration: Calibration,
}
impl Metadata {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let db = Connection::open_with_flags(
            path.as_ref().join("analysis.tdf"),
            OpenFlags::SQLITE_OPEN_READ_ONLY,
        )?;
        let globals: BTreeMap<String, String> = db
            .prepare("SELECT Key, Value FROM GlobalMetadata")?
            .query_map([], |r| Ok((r.get(0)?, r.get(1)?)))?
            .collect::<std::result::Result<_, _>>()?;
        if globals.get("TimsCompressionType").map(String::as_str) != Some("2") {
            return Err("only TimsCompressionType=2 is supported".into());
        }
        let frames: Vec<FrameMeta> = db.prepare("SELECT Id, NumScans, NumPeaks, MsMsType, AccumulationTime, Time, TimsId FROM Frames ORDER BY Id")?
            .query_map([], |r| Ok(FrameMeta { id:r.get(0)?, num_scans:r.get(1)?, num_peaks:r.get(2)?, msms_type:r.get(3)?, accumulation_time:r.get(4)?, time:r.get(5)?, offset:r.get(6)? }))?
            .collect::<std::result::Result<_,_>>()?;
        let val = |key: &str| -> Result<f64> {
            Ok(globals
                .get(key)
                .ok_or_else(|| format!("missing {key}"))?
                .parse()?)
        };
        let mut lo = val("MzAcqRangeLower")?;
        let mut hi = val("MzAcqRangeUpper")?;
        if globals.get("AcquisitionSoftware").map(String::as_str) == Some("Bruker otofControl") {
            lo -= 5.;
            hi += 5.;
        }
        let tof = val("DigitizerNumSamples")?;
        let im_lo = val("OneOverK0AcqRangeLower")?;
        let im_hi = val("OneOverK0AcqRangeUpper")?;
        let scans = frames.iter().map(|f| f.num_scans).max().unwrap_or(0) as f64;
        if ![lo, hi, tof, im_lo, im_hi, scans]
            .iter()
            .all(|x| x.is_finite() && *x > 0.)
            || hi <= lo
            || im_hi <= im_lo
        {
            return Err("invalid calibration metadata".into());
        }
        Ok(Self {
            frames,
            calibration: Calibration {
                mz_intercept: lo.sqrt(),
                mz_slope: (hi.sqrt() - lo.sqrt()) / tof,
                im_intercept: im_hi,
                im_slope: (im_lo - im_hi) / scans,
            },
        })
    }
    pub fn frame(&self, id: u32) -> Result<&FrameMeta> {
        self.frames
            .binary_search_by_key(&id, |f| f.id)
            .map(|i| &self.frames[i])
            .map_err(|_| format!("unknown frame {id}").into())
    }
}
#[derive(Debug)]
enum Source {
    Pread(File),
    Mmap(memmap2::Mmap),
}
#[derive(Debug)]
pub struct Reader {
    pub metadata: Metadata,
    source: Source,
    file_len: u64,
}
/// Per-worker reusable buffers and Zstandard context. Do not share between workers.
pub struct Workspace {
    compressed: Vec<u8>,
    decoded: Vec<u8>,
    zstd: zstd::bulk::Decompressor<'static>,
}
impl Workspace {
    pub fn new() -> Result<Self> {
        Ok(Self {
            compressed: vec![],
            decoded: vec![],
            zstd: zstd::bulk::Decompressor::new()?,
        })
    }
}
impl Reader {
    pub fn open(path: impl AsRef<Path>, mode: IoMode) -> Result<Self> {
        let metadata = Metadata::open(&path)?;
        let file = File::open(path.as_ref().join("analysis.tdf_bin"))?;
        let file_len = file.metadata()?.len();
        let source = match mode {
            IoMode::Pread => Source::Pread(file),
            IoMode::Mmap => {
                // SAFETY: the input is read-only. The documented contract prohibits external mutation.
                Source::Mmap(unsafe { memmap2::Mmap::map(&file)? })
            }
        };
        Ok(Self {
            metadata,
            source,
            file_len,
        })
    }
    pub fn read(&self, id: u32, mode: IntensityMode) -> Result<Frame> {
        let mut workspace = Workspace::new()?;
        let mut frame = Frame::default();
        self.read_into(id, mode, &mut workspace, &mut frame)?;
        Ok(frame)
    }
    pub fn read_into(
        &self,
        id: u32,
        mode: IntensityMode,
        w: &mut Workspace,
        out: &mut Frame,
    ) -> Result<()> {
        let meta = self.metadata.frame(id)?;
        out.tofs.clear();
        out.intensities.clear();
        out.scan_offsets.clear();
        if meta.num_peaks == 0 {
            out.scan_offsets.resize(meta.num_scans as usize + 1, 0);
            return Ok(());
        }
        if meta.num_scans == 0 {
            return Err("frame has peaks but no scans".into());
        }
        if meta.offset > self.file_len || self.file_len - meta.offset < 8 {
            return Err("truncated frame header".into());
        }
        let mut header = [0; 8];
        match &self.source {
            Source::Pread(f) => read_at(f, &mut header, meta.offset)?,
            Source::Mmap(m) => {
                header.copy_from_slice(&m[meta.offset as usize..meta.offset as usize + 8])
            }
        }
        let size = u32::from_le_bytes(header[..4].try_into()?) as usize;
        if u32::from_le_bytes(header[4..].try_into()?) != meta.num_scans {
            return Err("frame scan count mismatch".into());
        }
        if size < 8 || size as u64 > self.file_len - meta.offset {
            return Err("invalid frame packet length".into());
        }
        let words = u64::from(meta.num_scans) + 2 * u64::from(meta.num_peaks);
        if words > u32::MAX as u64 {
            return Err("frame exceeds supported word count".into());
        }
        let bytes = usize::try_from(words.checked_mul(4).ok_or("frame size overflow")?)?;
        w.decoded.resize(bytes, 0);
        let payload = match &self.source {
            Source::Pread(f) => {
                w.compressed.resize(size - 8, 0);
                read_at(f, &mut w.compressed, meta.offset + 8)?;
                &w.compressed[..]
            }
            Source::Mmap(m) => &m[meta.offset as usize + 8..meta.offset as usize + size],
        };
        if w.zstd.decompress_to_buffer(payload, &mut w.decoded[..])? != bytes {
            return Err("decompressed size mismatch".into());
        }
        decode(&w.decoded, meta, mode, out)
    }
}
fn read_at(file: &File, mut dst: &mut [u8], mut offset: u64) -> std::io::Result<()> {
    while !dst.is_empty() {
        #[cfg(unix)]
        let result = {
            use std::os::unix::fs::FileExt;
            file.read_at(dst, offset)
        };
        #[cfg(windows)]
        let result = {
            use std::os::windows::fs::FileExt;
            file.seek_read(dst, offset)
        };
        let n = match result {
            Err(e) if e.kind() == std::io::ErrorKind::Interrupted => continue,
            x => x?,
        };
        if n == 0 {
            return Err(std::io::ErrorKind::UnexpectedEof.into());
        }
        offset += n as u64;
        dst = &mut dst[n..];
    }
    Ok(())
}
fn decode(data: &[u8], meta: &FrameMeta, mode: IntensityMode, out: &mut Frame) -> Result<()> {
    let words = data.len() / 4;
    let word = |i: usize| {
        u32::from_le_bytes([
            data[i],
            data[i + words],
            data[i + 2 * words],
            data[i + 3 * words],
        ])
    };
    let mut pos = meta.num_scans as usize;
    let mut count = 0;
    out.tofs.reserve(meta.num_peaks as usize);
    out.intensities.reserve(meta.num_peaks as usize);
    out.scan_offsets.push(0);
    for scan in 0..meta.num_scans {
        let n = if scan + 1 == meta.num_scans {
            meta.num_peaks - count
        } else {
            word(scan as usize + 1) / 2
        };
        if n > meta.num_peaks - count {
            return Err("scan headers exceed NumPeaks".into());
        }
        let mut tof = u32::MAX;
        for _ in 0..n {
            tof = tof.wrapping_add(word(pos));
            let raw = word(pos + 1);
            pos += 2;
            out.tofs.push(tof);
            out.intensities.push(match mode {
                IntensityMode::Raw => raw,
                IntensityMode::Normalized => {
                    (raw as f64 * (100. / meta.accumulation_time) + 0.5) as u32
                }
            });
        }
        count += n;
        out.scan_offsets.push(count as usize);
    }
    Ok(())
}

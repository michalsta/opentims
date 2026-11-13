// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
use opentims::{IntensityMode, IoMode, Reader};
use std::{
    path::PathBuf,
    sync::atomic::{AtomicU64, Ordering},
};
static SERIAL: AtomicU64 = AtomicU64::new(0);
struct Dataset(PathBuf);
impl Drop for Dataset {
    fn drop(&mut self) {
        let _ = std::fs::remove_dir_all(&self.0);
    }
}
fn dataset(count: u32) -> Dataset {
    let path = std::env::temp_dir().join(format!(
        "opentims-rust-{}-{}",
        std::process::id(),
        SERIAL.fetch_add(1, Ordering::Relaxed)
    ));
    std::fs::create_dir(&path).unwrap();
    let db = rusqlite::Connection::open(path.join("analysis.tdf")).unwrap();
    db.execute_batch("CREATE TABLE GlobalMetadata(Key TEXT,Value TEXT); INSERT INTO GlobalMetadata VALUES ('TimsCompressionType','2'),('MzAcqRangeLower','100'),('MzAcqRangeUpper','1600'),('DigitizerNumSamples','1000'),('OneOverK0AcqRangeLower','0.5'),('OneOverK0AcqRangeUpper','1.5'); CREATE TABLE Frames(Id INTEGER,NumScans INTEGER,NumPeaks INTEGER,MsMsType INTEGER,AccumulationTime REAL,Time REAL,TimsId INTEGER);").unwrap();
    db.execute("INSERT INTO Frames VALUES (7,3,3,0,50,1.5,0)", [])
        .unwrap();
    db.execute("INSERT INTO Frames VALUES (19,3,0,0,100,2,0)", [])
        .unwrap();
    // Three scans: two peaks, no peaks, one peak. TOFs reset each scan.
    let words = [3, count, 0, 1, 10, 4, 21, 10, u32::MAX];
    let mut planes = vec![];
    for byte in 0..4 {
        for word in words {
            planes.push(word.to_le_bytes()[byte]);
        }
    }
    let compressed = zstd::bulk::compress(&planes, 1).unwrap();
    let mut packet = ((compressed.len() + 8) as u32).to_le_bytes().to_vec();
    packet.extend(3u32.to_le_bytes());
    packet.extend(compressed);
    std::fs::write(path.join("analysis.tdf_bin"), packet).unwrap();
    Dataset(path)
}
#[test]
fn decoding_gapped_ids_empty_scans_normalization_and_cpp_parity() {
    let d = dataset(4);
    for mode in [IoMode::Pread, IoMode::Mmap] {
        let r = Reader::open(&d.0, mode).unwrap();
        assert_eq!(r.metadata.calibration.mz(0), 100.0);
        assert_eq!(r.metadata.calibration.mz(1000), 1600.0);
        assert_eq!(r.metadata.calibration.inverse_mobility(0.0), 1.5);
        assert_eq!(r.metadata.calibration.inverse_mobility(3.0), 0.5);
        let raw = r.read(7, IntensityMode::Raw).unwrap();
        assert_eq!(raw.tofs, vec![0, 4, 9]);
        assert_eq!(raw.scan_offsets, vec![0, 2, 2, 3]);
        assert_eq!(raw.intensities, vec![10, 21, u32::MAX]);
        let corrected = r.read(7, IntensityMode::Normalized).unwrap();
        assert_eq!(corrected.intensities, vec![20, 42, u32::MAX]);
        assert_eq!(
            r.read(19, IntensityMode::Raw).unwrap().scan_offsets,
            vec![0, 0, 0, 0]
        );
        assert!(r.read(8, IntensityMode::Raw).is_err());
        #[cfg(feature = "cpp")]
        assert_eq!(
            corrected,
            opentims::cpp::Reader::open(&d.0).unwrap().read(7).unwrap()
        );
    }
}
#[test]
fn corrupt_scan_counts_and_truncated_packets() {
    let d = dataset(8);
    assert!(Reader::open(&d.0, IoMode::Pread)
        .unwrap()
        .read(7, IntensityMode::Raw)
        .is_err());
    std::fs::write(d.0.join("analysis.tdf_bin"), [0; 7]).unwrap();
    for mode in [IoMode::Pread, IoMode::Mmap] {
        assert!(Reader::open(&d.0, mode)
            .unwrap()
            .read(7, IntensityMode::Raw)
            .is_err());
    }
}
#[test]
fn concurrent_reads_use_independent_workspaces() {
    let d = dataset(4);
    let r = Reader::open(&d.0, IoMode::Pread).unwrap();
    std::thread::scope(|s| {
        for _ in 0..4 {
            let r = &r;
            s.spawn(move || {
                for _ in 0..20 {
                    assert_eq!(r.read(7, IntensityMode::Raw).unwrap().tofs, vec![0, 4, 9]);
                }
            });
        }
    });
}
#[test]
fn real_compressed_fixture_parity() {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../pytest/test.d");
    let r = Reader::open(&path, IoMode::Pread).unwrap();
    let m = Reader::open(&path, IoMode::Mmap).unwrap();
    #[cfg(feature = "cpp")]
    let mut cpp = opentims::cpp::Reader::open(&path).unwrap();
    for f in &r.metadata.frames {
        let frame = r.read(f.id, IntensityMode::Normalized).unwrap();
        assert_eq!(frame, m.read(f.id, IntensityMode::Normalized).unwrap());
        #[cfg(feature = "cpp")]
        assert_eq!(frame, cpp.read(f.id).unwrap());
    }
}

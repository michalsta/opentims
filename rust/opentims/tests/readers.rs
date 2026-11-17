// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
use opentims::{IoMode, Reader};
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
fn decoding_gapped_ids_and_empty_scans() {
    let d = dataset(4);
    for mode in [IoMode::Pread, IoMode::Mmap] {
        let r = Reader::open(&d.0, mode).unwrap();
        assert_eq!(r.metadata.calibration.mz(0), 100.0);
        assert_eq!(r.metadata.calibration.mz(1000), 1600.0);
        assert_eq!(r.metadata.calibration.inverse_mobility(0.0), 1.5);
        assert_eq!(r.metadata.calibration.inverse_mobility(3.0), 0.5);
        let raw = r.read(7).unwrap();
        assert_eq!(raw.tofs, vec![0, 4, 9]);
        assert_eq!(raw.scan_offsets, vec![0, 2, 2, 3]);
        assert_eq!(raw.intensities, vec![10, 21, u32::MAX]);
        assert_eq!(r.read(19).unwrap().scan_offsets, vec![0, 0, 0, 0]);
        assert!(r.read(8).is_err());
    }
}
#[test]
fn corrupt_scan_counts_and_truncated_packets() {
    let d = dataset(8);
    assert!(Reader::open(&d.0, IoMode::Pread).unwrap().read(7).is_err());
    std::fs::write(d.0.join("analysis.tdf_bin"), [0; 7]).unwrap();
    for mode in [IoMode::Pread, IoMode::Mmap] {
        assert!(Reader::open(&d.0, mode).unwrap().read(7).is_err());
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
                    assert_eq!(r.read(7).unwrap().tofs, vec![0, 4, 9]);
                }
            });
        }
    });
}
#[test]
fn real_compressed_fixture_matches_cpp_opentims() {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../pytest/test.d");
    // From Python opentims (C++ reader). AccumulationTime is 100 ms, so its
    // normalised intensities equal raw ones. Scans 0-based.
    type Peaks = (u32, [usize; 5], [u32; 5], [u32; 5]); // frame id, scans, tofs, intensities
    let expected: [Peaks; 2] = [
        (
            1,
            [33, 49, 53, 59, 62],
            [268877, 36849, 356994, 366925, 103614],
            [10, 10, 87, 10, 80],
        ),
        (
            2,
            [54, 56, 73, 80, 83],
            [399169, 205746, 205657, 66533, 315625],
            [10, 51, 10, 10, 10],
        ),
    ];
    for mode in [IoMode::Pread, IoMode::Mmap] {
        let r = Reader::open(&path, mode).unwrap();
        assert_eq!(r.metadata.frames.len(), expected.len());
        for (id, scans, tofs, intensities) in expected {
            let frame = r.read(id).unwrap();
            let peak_scans: Vec<usize> = frame
                .scan_offsets
                .windows(2)
                .enumerate()
                .flat_map(|(scan, w)| std::iter::repeat_n(scan, w[1] - w[0]))
                .collect();
            assert_eq!(peak_scans, scans);
            assert_eq!(frame.tofs, tofs);
            assert_eq!(frame.intensities, intensities);
        }
    }
}

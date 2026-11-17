#!/usr/bin/env python3
# Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
# SPDX-License-Identifier: MIT
"""Prepare an isolated timsrust 0.4.2 adapter; never modify the Cargo cache.

Usage: python3 rust/prepare_timsrust.py /path/to/timsrust-0.4.2 /output/path
The generated crate retains upstream Apache-2.0 licensing and spectrum logic.
"""
import pathlib
import json
import shutil
import sys
import tomllib

source, dest = map(pathlib.Path, sys.argv[1:])
manifest = tomllib.loads((source / "Cargo.toml").read_text())
assert manifest["package"]["name"] == "timsrust"
assert manifest["package"]["version"] == "0.4.2"
if dest.exists():
    raise SystemExit("Destination exists; choose a new directory")
shutil.copytree(source, dest)
core = pathlib.Path(__file__).resolve().parent / "opentims"
with (dest / "Cargo.toml").open("a") as f:
    f.write('\n[dependencies.opentims]\npath = ' + json.dumps(str(core)) + '\n')
path = dest / "src/io/readers/frame_reader.rs"
text = path.read_text()
def replace(old, new):
    global text
    if text.count(old) != 1:
        raise SystemExit("Unexpected upstream source; cannot safely apply adapter")
    text = text.replace(old, new)
replace("    tdf_bin_reader: TdfBlobReader,", "    opentims: opentims::Reader,")
replace("        let tdf_bin_reader = TdfBlobReader::new(&path)?;", '''        let resolved = path.to_timstof_path().map_err(|e| FrameReaderError::OpenTims(e.to_string()))?;
        let binary = resolved.tdf_bin().map_err(|e| FrameReaderError::OpenTims(e.to_string()))?;
        let mode = match std::env::var("OPENTIMS_IO").as_deref() {
            Ok("mmap") => opentims::IoMode::Mmap,
            Ok("pread") | Err(_) => opentims::IoMode::Pread,
            Ok(_) => return Err(FrameReaderError::OpenTims("OPENTIMS_IO must be pread or mmap".into())),
        };
        let opentims = opentims::Reader::open(binary.parent().unwrap(), mode)
            .map_err(|e| FrameReaderError::OpenTims(e.to_string()))?;''')
replace("            tdf_bin_reader,", "            opentims,")
start = text.index("        let offset = self.get_binary_offset(index);", text.index("    fn get_from_compression_type_2("))
end = text.index("        Ok(frame)", start)
text = text[:start] + '''        let raw = self.opentims.read(frame.index.try_into().map_err(|_| FrameReaderError::CorruptFrame)?)
            .map_err(|e| FrameReaderError::OpenTims(e.to_string()))?;
        frame.scan_offsets = raw.scan_offsets;
        frame.tof_indices = raw.tofs;
        frame.intensities = raw.intensities;
''' + text[end:]
replace("pub enum FrameReaderError {", '''pub enum FrameReaderError {
    #[error("OpenTIMS: {0}")]
    OpenTims(String),''')
text = text.replace("TdfBlob, TdfBlobReader, TdfBlobReaderError", "TdfBlob, TdfBlobReaderError")
path.write_text("// Modified for OpenTIMS frame IO; upstream spectrum processing retained.\n" + text)
print(dest.resolve())

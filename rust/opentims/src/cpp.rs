// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
//! Owning C++ binding. Reads require exclusive access to the handle's scratch buffers.
use crate::{Frame, Metadata, Result};
use std::{
    ffi::{c_char, c_void, CStr, CString},
    marker::PhantomData,
    path::Path,
    ptr::NonNull,
    rc::Rc,
};
extern "C" {
    fn ot_open(path: *const c_char) -> *mut c_void;
    fn ot_close(handle: *mut c_void);
    fn ot_error() -> *const c_char;
    fn ot_read(
        handle: *mut c_void,
        id: u32,
        capacity: usize,
        scans: *mut u32,
        tofs: *mut u32,
        intensities: *mut u32,
    ) -> i32;
}
pub struct Reader {
    handle: NonNull<c_void>,
    pub metadata: Metadata,
    scans: Vec<u32>,
    _not_send_sync: PhantomData<Rc<()>>,
}
fn error() -> crate::Error {
    unsafe {
        CStr::from_ptr(ot_error())
            .to_string_lossy()
            .into_owned()
            .into()
    }
}
impl Reader {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        let metadata = Metadata::open(&path)?;
        let path = CString::new(
            path.as_ref()
                .to_str()
                .ok_or("C++ backend requires a UTF-8 path")?,
        )?;
        let handle = NonNull::new(unsafe { ot_open(path.as_ptr()) }).ok_or_else(error)?;
        Ok(Self {
            handle,
            metadata,
            scans: vec![],
            _not_send_sync: PhantomData,
        })
    }
    pub fn read(&mut self, id: u32) -> Result<Frame> {
        let mut out = Frame::default();
        self.read_into(id, &mut out)?;
        Ok(out)
    }
    pub fn read_into(&mut self, id: u32, out: &mut Frame) -> Result<()> {
        let meta = self.metadata.frame(id)?;
        let n = meta.num_peaks as usize;
        self.scans.resize(n, 0);
        out.tofs.resize(n, 0);
        out.intensities.resize(n, 0);
        // SAFETY: live uniquely borrowed handle, all three buffers hold exactly n elements.
        if unsafe {
            ot_read(
                self.handle.as_ptr(),
                id,
                n,
                self.scans.as_mut_ptr(),
                out.tofs.as_mut_ptr(),
                out.intensities.as_mut_ptr(),
            )
        } != 0
        {
            return Err(error());
        }
        out.scan_offsets.clear();
        out.scan_offsets.push(0);
        let mut i = 0;
        for scan in 0..meta.num_scans {
            while i < n && self.scans[i] == scan {
                i += 1;
            }
            out.scan_offsets.push(i);
        }
        Ok(())
    }
}
impl Drop for Reader {
    fn drop(&mut self) {
        unsafe { ot_close(self.handle.as_ptr()) }
    }
}

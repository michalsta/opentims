<!-- Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki; SPDX-License-Identifier: MIT -->
# Measurements, 2026-11-13

timsrust 0.4.2 through its public API, upstream against the OpenTIMS adapter;
first 1,000 frames and spectra, warm cache, median of seven, one Linux x86-64 host.

| Dataset | Backend | Frames ms | Spectra ms |
|---|---|---:|---:|
| F9477 (DIA) | upstream | 377 | 296 |
| F9477 | adapter, pread | 325 | 199 |
| F9477 | adapter, mmap | 318 | 186 |
| G220824_007 (DDA) | upstream | 1171 | 74 |
| G220824_007 | adapter, pread | 830 | 82 |
| G220824_007 | adapter, mmap | 632 | 52 |

Frame and spectrum fingerprints were identical across all three per dataset.
Not a whole Sage search benchmark.

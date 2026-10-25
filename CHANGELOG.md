# Changelog

All notable changes to OpenTIMS: the C++ core (opentims++), the Python
package (opentimspy) and the R package (opentimsr), which share one version
number. R versions carry no pre-release suffix, so 1.2.0b4 was opentimsr 1.2.0.

Each release's section becomes the notes of its GitHub release, and
`src/opentimsr/NEWS.md` points here, so R's `news()` shows it too.

## 1.2.2 (unreleased)

### Core

- Extraction decodes frames in parallel, and decodes a frame requested several
  times only once.
- Optional lookup tables for Bruker's m/z and 1/K0 conversion, much faster
  when converting many peaks; calls into Bruker's library are serialised.
- Compressed frame headers are read without unaligned loads (undefined
  behaviour reported by CRAN's sanitizer checks).
- Errors from parallel decoding keep their message when R loads both libc++
  and libstdc++ (CRAN's fedora-clang check reported "c++ exception (unknown
  reason)").
- Repeated frames are extracted correctly in every column, and zstd flag
  overrides are honoured.
- The public header no longer includes SQLite, so consumers of the shared
  library need no SQLite headers.

### Python

- `get_separate_frames()` decodes each frame once.
- `query()` and `frame_arrays()` reject fractional, negative and out-of-range
  frame ids instead of truncating them.
- SQLite connections are closed, `framesTIC` is cached per instance, and
  invalid input raises exceptions instead of failing asserts.
- Docstrings of the conversion and retention-time functions describe what the
  code does.
- Requires Python 3.9 or later; pybind11 is no longer a runtime dependency.

### R

- New `get_separate_frames()`.
- Frames are decoded straight into R vectors.
- `download_bruker_proprietary_code()` reuses a library already downloaded
  (new argument `overwrite`) and never rewrites one in place: downloading it a
  second time in a session no longer crashes R (#21).
- Addressed CRAN's review of 2026-10-06: Title and Description wording, the
  `length()`, `range()` and `[` methods are exported, and examples that
  download Bruker's library use `\donttest{}` and write to `tempdir()`.

## 1.2.1 (2026-10-03)

### Core

- Frame extraction is thread-safe; cached frames no longer alias one buffer,
  and failed extraction joins its workers and restores the threading state.
- Frame ids are no longer assumed to run 1..N.
- Hardened against corrupted data: decompressed sizes are checked before
  allocation, unexpected dataset contents raise exceptions instead of failing
  asserts, and slice counters cannot wrap.
- Locale-independent number parsing, without side effects on the global
  locale.
- Frame counts are parsed as 64-bit on Windows; dlopen'ed SQLite symbols are
  resolved thread-safely; an SQLite busy timeout lets concurrent readers open
  the same dataset.
- Corrected intensities are clamped into the uint32 range.
- Conversion failures raise exceptions with Bruker's diagnostics.
- A C++ test suite, run in CI with sanitizers, Valgrind and fuzzing.

### Python

- Frame owners are kept alive while frame references exist.
- `query()` with `_sanitize=False` fills user-provided arrays.

### R

- Links the system zstd and no longer bundles zstd or SQLite; SQLite access
  goes through RSQLite.
- Mateusz Łącki is the maintainer.
- Frame numbers and slices are validated with clear errors; `range()` with
  `by = 0` no longer loops forever.
- `OpenTIMS()` expands `~` and relative paths, and opens `analysis.tdf`
  read-only.
- `download_bruker_proprietary_code()` stops clearly on platforms without
  Bruker's library.
- Released on GitHub as a source tarball and a Windows binary.

## 1.2.0 (2026-04-22)

- Built-in open-source m/z and 1/K0 converters, selectable per dataset, as an
  alternative to Bruker's library.
- Reworked build system: static and shared C++ libraries with CMake and
  pkg-config files; SQLite loaded from Python's own module or linked.
- Reworked R bindings with tests.
- Datasets whose frame ids do not start at 1 are supported.
- Asserts replaced by exceptions.

Earlier releases are described only by the git history.

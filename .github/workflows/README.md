# CI memo

What runs in CI, when, and why each job exists. If you add, remove or
retrigger a job, update this file in the same change.

## When things run

| Workflow | Runs on | Notes |
|---|---|---|
| `r_check.yml` (R CMD check) | pushes and PRs touching the R package, the C++ core, `pytest/test.d`, `CHANGELOG.md` or itself; release | the quick R checks |
| `run_tests.yml` (named "Build") | pushes to `master`, `main`, `devel`, `tests`, `ci-test`; release | pytest |
| `cpp_tests.yml` | pushes to the same branches; by hand | not part of the release |
| `build_wheels.yml` (also named "Build") | pushes to `ci-test`; release | |
| `cran_flavours.yml` | **weekly (Mon 04:00 UTC), release, by hand, and pushes that change it or `.github/scripts/fedora_clang_check.sh`**. **Not on ordinary pushes, by decision**: 14 slow jobs. | `gh workflow run cran_flavours.yml` |
| `publish.yml` (Release) | `v*` tags (**publishes**); by hand without a tag (dry run) | calls `build_wheels`, `run_tests`, `r_check`, `cran_flavours` |

So a change to the R package or the C++ core is **not** checked on CRAN's
setups until the next weekly run or release. Before tagging a release after
such a change, run `cran_flavours.yml` by hand, so a failure there doesn't
stop the release halfway.

`workflow_dispatch` only works for workflows on the default branch. To try
a workflow change on a branch, rely on a push trigger. The pytest, wheel and
C++ workflows run on a push to `ci-test`.

## `r_check.yml`: R CMD check

The quick R check on GitHub's runners, using r-lib's actions and binary
dependencies. zstd comes from apt or Homebrew.

| Job | Why |
|---|---|
| Build source tarball (Linux) | `R CMD build` resolves the symlinks, which Windows checkouts break. Every other job checks this tarball, as CRAN would receive it. |
| Windows / release, `--as-cran` and plain | CRAN builds Windows binaries; Rtools differs from Linux toolchains. Plain `R CMD check` is what users and other repositories run. |
| windows-11-arm / release / `--as-cran` | Windows on ARM; experimental (`continue-on-error`) while R's support matures. |
| macOS arm64 / release, `--as-cran` and plain | macOS with Homebrew's zstd: what users installing from source do. CRAN's own macOS setup is in `cran_flavours.yml`. |
| Ubuntu / release, `--as-cran` and plain | the baseline Linux check |
| Ubuntu / devel / `--as-cran` | R-devel, as CRAN checks submissions |
| Ubuntu / R 4.3 / `--as-cran` | `DESCRIPTION` declares `R (>= 4.3.0)`, so the oldest supported R must work. |

## `cran_flavours.yml`: CRAN's check machines

Each job mirrors a machine on CRAN's check page, or one of CRAN's "additional
issues" checks. All run `--as-cran` and fail on WARNING or ERROR. The
sanitizer and valgrind jobs also fail on any report in the test output,
which `R CMD check` itself ignores.

| Job | Mirrors | Why |
|---|---|---|
| fedora-clang | `r-devel-linux-x86_64-fedora-clang`, rebuilt from its published recipe by `.github/scripts/fedora_clang_check.sh` | Only a match for this machine catches libc++ and libstdc++ loaded in one process. That broke 1.2.0 on CRAN (`c++ exception (unknown reason)`); this job reproduces it. Bump `LLVM_VERSION` and `PANDOC_VERSION` when CRAN does. |
| gcc16 (R-hub) | `r-devel-linux-x86_64-fedora-gcc` | CRAN's newest gcc, with its warnings, on Fedora |
| debian-clang (R-hub) | `r-devel-linux-x86_64-debian-clang` | clang on Debian; CRAN's check incoming tests run on Debian |
| gcc-asan (R-hub) | CRAN's gcc ASAN/UBSAN additional issue | memory errors and undefined behaviour with gcc. CRAN's UBSan check reported a misaligned load in 1.2.0. |
| clang-asan, clang-ubsan (R-hub) | CRAN's clang-ASAN and clang-UBSAN additional issues | the same with clang and libc++, which instrument differently |
| valgrind (R-hub) | CRAN's valgrind additional issue | leaks and invalid reads in uninstrumented code (dependencies included). It found the RSQLite leak worked around in 1.2.3. Its timing NOTE is expected. |
| r-devel-san (Rocker) | the UBSan flags of CRAN's memtests (`-fsanitize=undefined,bounds-strict`) | `bounds-strict` checks array bounds that R-hub's gcc-asan does not; a second, independently built sanitizer R |
| macOS arm64 / release, oldrel, devel | `r-release-macos-arm64`, `r-oldrel-macos-arm64`, and the macOS builder's R-devel | CRAN's R, with zstd and pkg-config from CRAN's macOS recipes (static, `/opt/R/arm64`), not Homebrew. That is how CRAN builds the binaries users download. Checks that zstd was linked statically and nothing came from Homebrew. |
| macOS x86_64 / release, oldrel | `r-release-macos-x86_64`, `r-oldrel-macos-x86_64` | CRAN still builds Intel binaries; the same setup under `/opt/R/x86_64` |

Not mirrored: the macOS and Xcode versions of CRAN's machines (GitHub's
runners are newer), M1-SAN, and the PDF manual (`--no-manual`; check it
locally before submitting). R-hub images that were considered and left
out: BLAS variants (no BLAS used), rhel, centos and s390x (not CRAN
flavours), donttest (`--as-cran` runs `\donttest` already), and clang23
(fedora-clang covers libc++).

## `run_tests.yml`: Python tests (named "Build")

| Job | Why |
|---|---|
| pytest on Linux x86/ARM, Windows x86/ARM, macOS Intel/arm64, Python 3.9–3.14 | every platform and Python version wheels are built for; `requires-python = ">=3.9"` |
| [clang] on Linux, x86 and ARM | the default compiler is gcc there. clang catches non-portable C++. |
| Bruker bridge steps (PyPI and git versions) | the proprietary converter path through `so_manager` and dlopen, which open-source tests never reach |

## `build_wheels.yml`: wheels (also named "Build")

| Job | Why |
|---|---|
| Build wheels on six runners | cibuildwheel builds and tests the wheels released to PyPI. CPython and PyPy, minus versions below 3.10 and `cp310-win_arm64`. |
| Build source distribution | the sdist on PyPI, for platforms without a wheel |

## `cpp_tests.yml`: C++ core

| Job | Why |
|---|---|
| Release builds: gcc and clang on Linux x86, gcc on Linux ARM, clang on macOS arm64 and Intel, MSVC on Windows x86 and ARM | the core on every platform the wheels target |
| MSVC Debug | the debug CRT's asserts and checked iterators |
| ASan+UBSan (gcc, clang, macOS), ASan (MSVC) | memory errors and undefined behaviour in the core itself, without R or Python in the way |
| TSan (gcc, clang) | data races in the parallel frame decoding |
| Hardened libstdc++ / libc++ | debug-mode standard libraries catch out-of-range and invalid-iterator use, at full speed |
| Valgrind memcheck | leaks and uninitialised reads that the sanitizers miss |
| libFuzzer (frame decoding) | malformed and corrupted frame data, the input most likely to come from outside |
| Installed library | builds, installs and consumes the shared library through CMake and pkg-config, as OpenMS and other packagers do |

Each sanitizer family needs its own build directory. `vm.mmap_rnd_bits` is
lowered for ASan and TSan on recent kernels.

## `publish.yml`: Release

Runs only for a `v*` tag, or as a dry run by hand. It checks that the tag,
`pyproject.toml` and `DESCRIPTION` versions agree and that `CHANGELOG.md` has
a dated section. It then runs every workflow above except `cpp_tests.yml`,
builds the R Windows binary (tested by running the test suite against it)
and a symlink-free repository archive, and publishes in this order: GitHub
release as a draft, PyPI, then the draft made public. Don't rename the file:
PyPI's trusted publisher is bound to it.

## Update

This update fixes the problems that CRAN's checks found in opentimsr 1.2.0:

* r-devel-linux-x86_64-fedora-clang, ERROR in tests (test_threads.R): an
  error raised while decoding frames in parallel reached R as "c++ exception
  (unknown reason)". The error was carried between threads in a
  std::exception_ptr; when libc++ and libstdc++ are both loaded in one
  process (as on that check machine, where R links ICU and thereby
  libstdc++), libc++'s exception_ptr can interpret libstdc++'s exception
  data and rethrow an unrecognisable exception. The package now passes the
  error message between threads and throws a fresh exception on the calling
  thread.
* gcc-UBSAN and M1-SAN: "load of misaligned address" in opentims.cpp. Frame
  headers in the memory-mapped data file are now read with memcpy.

Both were reproduced locally on 1.2.0 (an R build using clang and libc++,
with Rcpp also built against libc++; and a gcc -fsanitize=undefined build)
and are gone in 1.2.2.

Other changes are listed in NEWS.md. Among them, download_bruker_proprietary_code()
no longer overwrites a library that is already downloaded (overwriting one
that was loaded crashed R); it gained an argument, overwrite, defaulting to
FALSE.

## Test environments

* local Arch Linux, R 4.6.1 (gcc 16): R CMD check and R CMD check --as-cran
* local Arch Linux, R 4.6.1, clang with libc++: tests
* local Arch Linux, R 4.6.1, gcc with -fsanitize=undefined: tests
* GitHub Actions: R CMD check --as-cran on Windows x64 and arm64, macOS
  arm64 (R release) and Ubuntu (R release, devel and 4.3); plain R CMD check
  on Windows, macOS and Ubuntu (R release)

## R CMD check results

0 errors | 0 warnings | 1 note

* Possibly misspelled words in DESCRIPTION: 'Bruker's' is a company name,
  'timsTOF' is the name of Bruker's mass spectrometers, 'tdf' is part of the
  file names analysis.tdf and analysis.tdf_bin, 'Łącki et al.' is the author
  citation, and 'mobilities' is the plural of ion mobility; all are spelled
  correctly.
* Installed size (about 6Mb on Linux): reported as a NOTE by R 4.3 and only
  as INFO by current R, so it is not counted above. Almost all of it is
  debug information from the default -g compiler flag; the shared library
  without it is about 0.4Mb.

## Reverse dependencies

There are none.

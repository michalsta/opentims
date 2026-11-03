## Update

A small follow-up to 1.2.2:

* OpenTIMS() on a folder without analysis.tdf now stops with "No
  analysis.tdf in <folder>" before opening a database connection. Before,
  RSQLite failed with "Could not connect to database" and leaked the handle
  of the failed connection, which valgrind reports as definitely lost (the
  package's tests exercise this case).

Other changes are listed in NEWS.md; none of them touch the R package's
code.

## Test environments

* local Arch Linux, R 4.6.1 (gcc 16): R CMD check and R CMD check --as-cran
* local Arch Linux, R 4.6.1, clang with libc++: tests
* local Arch Linux, R 4.6.1, gcc with -fsanitize=undefined: tests
* GitHub Actions: R CMD check --as-cran on Windows x64 and arm64, macOS
  arm64 (R release) and Ubuntu (R release, devel and 4.3); plain R CMD check
  on Windows, macOS and Ubuntu (R release)
* GitHub Actions, CRAN's Linux flavours: r-devel fedora-clang (rebuilt from
  CRAN's recipe: LLVM with libc++), and the R-hub images for fedora-gcc,
  debian-clang, gcc and clang ASan+UBSan, and valgrind

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

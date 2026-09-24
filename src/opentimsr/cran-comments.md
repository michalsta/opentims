## Resubmission

opentimsr was archived on 2023-03-29 for misrepresentation of authorship
and copyright holders of the included 'zstd' sources, and for compiling a
bundled copy of 'zstd' instead of using the installed library.

This version addresses both points:

* 'zstd' is no longer bundled. A configure script locates the system
  library (via pkg-config, falling back to -lzstd) and fails with
  installation instructions if it is missing; Windows links -lzstd from
  Rtools. 'libzstd' is listed in SystemRequirements.
* The bundled 'sqlite' sources have been removed as well; all SQLite access
  now goes through RSQLite.
* The only remaining third-party code is the header-only 'mio' library
  (MIT). Its author is listed in Authors@R as contributor and copyright
  holder, and its copyright notice and license text are reproduced in
  inst/COPYRIGHTS (referenced from the Copyright field in DESCRIPTION).

The maintainer has changed from Michał Piotr Startek to Mateusz Krzysztof
Łącki, as agreed between the two authors.

Other changes: all examples now run on a small dataset shipped in
inst/extdata, except the two that download Bruker's proprietary library.

## Test environments

* local Ubuntu 24.04, R 4.6.1 and R 4.3.3

## R CMD check results

0 errors | 0 warnings | 1 note

* New submission / archived package (see above).

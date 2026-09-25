library(opentimsr)

setup_opensource()
path.d <- system.file("extdata", "test.d", package = "opentimsr")

query_with_threads <- function(n, ...) {
  opentims_set_threads(n)
  on.exit(opentims_set_threads(2))
  D <- OpenTIMS(path.d)
  on.exit(CloseTIMS(D), add = TRUE)
  query(D, ...)
}

test_that("parallel and sequential decoding give identical results", {
  expect_identical(query_with_threads(2, frames = 1:2), query_with_threads(1, frames = 1:2))
})

test_that("a frame requested twice is returned twice", {
  both <- query_with_threads(2, frames = c(1L, 1L, 2L))
  once <- query_with_threads(1, frames = 1:2)
  frame1 <- once[once$frame == 1L, ]
  expected <- rbind(frame1, frame1, once[once$frame == 2L, ])
  rownames(expected) <- NULL
  rownames(both) <- NULL
  expect_identical(both, expected)
})

test_that("corrupted frame data raises an R error with parallel decoding", {
  bad.d <- file.path(tempfile(), "bad.d")
  dir.create(bad.d, recursive = TRUE)
  file.copy(file.path(path.d, c("analysis.tdf", "analysis.tdf_bin")), bad.d)
  bin <- file.path(bad.d, "analysis.tdf_bin")
  bytes <- readBin(bin, "raw", file.size(bin))
  bytes[9:40] <- as.raw(0xFF)  # overwrite the start of frame 1's compressed data
  writeBin(bytes, bin)

  opentims_set_threads(2)
  D <- OpenTIMS(bad.d)
  on.exit(CloseTIMS(D))
  expect_error(query(D, frames = 1:2), "Error uncompressing frame")
})

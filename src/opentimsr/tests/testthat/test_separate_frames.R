library(opentimsr)

setup_opensource()
path.d <- system.file("extdata", "test.d", package = "opentimsr")
D <- OpenTIMS(path.d)

test_that("each frame matches query() for that frame", {
  for (columns in list(opentimsr:::all_columns, c("mz", "frame", "intensity"))) {
    frames <- get_separate_frames(D, c(1, 2), columns)
    expect_identical(names(frames), c("1", "2"))
    for (f in names(frames))
      expect_identical(frames[[f]], query(D, as.integer(f), columns))
  }
})

test_that("a frame requested more than once is returned once, in first-request order", {
  frames <- get_separate_frames(D, c(2, 1, 2, 2))
  expect_identical(names(frames), c("2", "1"))
})

test_that("parallel and sequential decoding give identical results", {
  opentims_set_threads(1)
  sequential <- get_separate_frames(D, c(1, 2))
  opentims_set_threads(2)
  expect_identical(get_separate_frames(D, c(1, 2)), sequential)
})

test_that("unknown columns are rejected", {
  expect_error(get_separate_frames(D, 1, "nonsense"), "Wrong column names")
})

test_that("a frame without peaks gives an empty data frame", {
  empty.d <- file.path(tempfile(), "test.d")
  dir.create(empty.d, recursive = TRUE)
  file.copy(file.path(path.d, c("analysis.tdf", "analysis.tdf_bin")), empty.d)
  sql_conn <- DBI::dbConnect(RSQLite::SQLite(), file.path(empty.d, "analysis.tdf"))
  DBI::dbExecute(sql_conn, "UPDATE Frames SET NumPeaks = 0 WHERE Id = 2")
  DBI::dbDisconnect(sql_conn)

  E <- OpenTIMS(empty.d)
  on.exit(CloseTIMS(E))
  frames <- get_separate_frames(E, c(1, 2))
  expect_identical(nrow(frames[["2"]]), 0L)
  expect_identical(frames[["2"]], query(E, 2L))
  expect_identical(frames[["1"]], query(E, 1L))
})

CloseTIMS(D)

library(opentimsr)

setup_opensource()
D <- OpenTIMS(test_path("test.d"))

raw_columns <- c("frame", "scan", "tof", "intensity")

# --- frame metadata ---

test_that("frame bounds are consistent", {
  expect_gte(D@min_frame, 1L)
  expect_gte(D@max_frame, D@min_frame)
  expect_equal(nrow(D@frames), D@max_frame - D@min_frame + 1L)
})

test_that("max_scan is positive", {
  expect_gt(D@max_scan, 0L)
})

test_that("retention times have one entry per frame and are monotone", {
  rts <- retention_times(D)
  expect_length(rts, nrow(D@frames))
  expect_true(all(diff(rts) >= 0))
})

test_that("MS1 frames are a subset of all frames", {
  expect_true(all(MS1(D) %in% D@frames$Id))
})

test_that("peaks_per_frame_cnts sums to length", {
  expect_equal(sum(peaks_per_frame_cnts(D)), length(D))
})

test_that("min_max_measurements has min and max rows", {
  mm <- min_max_measurements(D)
  expect_equal(mm$stat, c("min", "max"))
  expect_true(all(mm$frame[1] <= mm$frame[2]))
})

# --- query ---

test_that("query returns requested columns", {
  result <- query(D, frames = c(1L, 2L), columns = raw_columns)
  expect_s3_class(result, "data.frame")
  expect_equal(colnames(result), raw_columns)
})

test_that("query frame and scan values are in range", {
  result <- query(D, frames = D@frames$Id, columns = c("frame", "scan"))
  expect_true(all(result$frame >= D@min_frame & result$frame <= D@max_frame))
  expect_true(all(result$scan >= 0L & result$scan <= D@max_scan))
})

test_that("query intensities are positive", {
  result <- query(D, frames = D@frames$Id, columns = "intensity")
  expect_true(all(result$intensity > 0L))
})

test_that("query of a single frame returns only that frame", {
  result <- query(D, frames = D@min_frame, columns = raw_columns)
  expect_true(all(result$frame == D@min_frame))
})

test_that("query rejects unknown columns", {
  expect_error(query(D, frames = 1L, columns = "nonsense"), "Wrong column names")
})

# --- query_slice ---

test_that("query_slice over all frames matches query", {
  from_slice <- query_slice(D, columns = raw_columns)
  from_query <- query(D, frames = D@frames$Id, columns = raw_columns)
  expect_equal(from_slice, from_query)
})

test_that("query_slice of a single frame matches query", {
  from_slice <- query_slice(D, D@min_frame, D@min_frame, columns = raw_columns)
  from_query <- query(D, frames = D@min_frame, columns = raw_columns)
  expect_equal(from_slice, from_query)
})

test_that("empty query_slice returns no rows", {
  result <- query_slice(D, D@max_frame, D@min_frame, columns = raw_columns)
  expect_equal(nrow(result), 0L)
})

# --- rt_query ---

test_that("rt_query over the full retention time range matches query_slice", {
  rts <- retention_times(D)
  expect_equal(rt_query(D, min(rts), max(rts), columns = raw_columns),
               query_slice(D, columns = raw_columns))
})

test_that("rt_query outside the data raises", {
  expect_error(rt_query(D, -2, -1), "does not hold any data")
})

# --- OpenTIMS methods ---

test_that("[ returns peaks of the requested frame", {
  result <- D[D@min_frame]
  expect_true(all(result$frame == D@min_frame))
})

test_that("[ rejects frames out of range", {
  expect_error(D[D@max_frame + 1L])
})

test_that("range covers all peaks", {
  result <- range(D, D@min_frame, D@max_frame + 1L)
  expect_equal(nrow(as.data.frame(result)), length(D))
})

# --- SQLite tables ---

test_that("tables_names lists Frames and GlobalMetadata", {
  expect_true(all(c("Frames", "GlobalMetadata") %in% tables_names(D)))
})

test_that("table2df returns the Frames table", {
  frames <- table2df(D, "Frames")
  expect_equal(frames$Frames$Id, D@frames$Id)
})

CloseTIMS(D)

test_that("OpenTIMS rejects a missing folder", {
  expect_error(OpenTIMS(file.path(tempdir(), "no_such_folder.d")), "no_such_folder.d", fixed = TRUE)
})

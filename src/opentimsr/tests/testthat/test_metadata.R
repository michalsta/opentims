library(opentimsr)

# Copy test.d to a temporary folder and run `sql` against its analysis.tdf.
modified_test_d <- function(sql) {
  path.d <- file.path(tempfile(), "test.d")
  dir.create(path.d, recursive = TRUE)
  file.copy(file.path(test_path("test.d"), c("analysis.tdf", "analysis.tdf_bin")), path.d)
  sql_conn <- DBI::dbConnect(RSQLite::SQLite(), file.path(path.d, "analysis.tdf"))
  on.exit(DBI::dbDisconnect(sql_conn))
  DBI::dbExecute(sql_conn, sql)
  path.d
}

test_that("unsupported TimsCompressionType is rejected", {
  setup_opensource()
  path.d <- modified_test_d(
    "UPDATE GlobalMetadata SET Value = '1' WHERE Key = 'TimsCompressionType'")
  expect_error(OpenTIMS(path.d), "is not \\(yet\\) supported")
})

test_that("missing m/z calibration metadata is rejected", {
  setup_opensource()
  path.d <- modified_test_d(
    "DELETE FROM GlobalMetadata WHERE Key = 'MzAcqRangeLower'")
  expect_error(OpenTIMS(path.d), "invalid calibration metadata")
})

test_that("missing ion mobility calibration metadata is rejected", {
  setup_opensource()
  path.d <- modified_test_d(
    "DELETE FROM GlobalMetadata WHERE Key = 'OneOverK0AcqRangeUpper'")
  expect_error(OpenTIMS(path.d), "invalid calibration metadata")
})

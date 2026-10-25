# download_bruker_proprietary_code() is tested against a local file:// URL, so
# neither network access nor Bruker's library is needed.

bruker_platform <- Sys.info()[["sysname"]] %in% c("Linux", "Windows") &&
  R.version$arch %in% c("x86_64", "x86-64", "amd64")

fake_bruker_server <- function(contents) {
  src <- file.path(tempfile(), "server")
  dir.create(file.path(src, "win64"), recursive = TRUE)
  writeLines(contents, file.path(src, "libtimsdata.so"))
  writeLines(contents, file.path(src, "win64", "timsdata.dll"))
  src <- normalizePath(src, winslash = "/")
  paste0(if (startsWith(src, "/")) "file://" else "file:///", src, "/")
}

download_quietly <- function(...) {
  suppressMessages(download_bruker_proprietary_code(..., quiet = TRUE))
}

test_that("an existing library is reused unless overwrite = TRUE", {
  skip_if_not(bruker_platform, "Bruker's library is not available on this platform")
  target <- tempfile()
  dir.create(target)

  path <- download_quietly(target, net_url = fake_bruker_server("first"))
  expect_identical(readLines(path), "first")

  expect_identical(download_quietly(target, net_url = fake_bruker_server("second")), path)
  expect_identical(readLines(path), "first")

  download_quietly(target, net_url = fake_bruker_server("third"), overwrite = TRUE)
  expect_identical(readLines(path), "third")
  expect_identical(list.files(target), basename(path))
})

test_that("a failed download leaves no partial file behind", {
  skip_if_not(bruker_platform, "Bruker's library is not available on this platform")
  target <- tempfile()
  dir.create(target)
  missing <- paste0(fake_bruker_server("x"), "nonexistent/")

  expect_error(suppressWarnings(download_quietly(target, net_url = missing)))
  expect_identical(list.files(target), character(0))
})

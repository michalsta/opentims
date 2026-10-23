library(opentimsr)

path.d <- system.file("extdata", "test.d", package = "opentimsr")
cols <- c("mz", "inv_ion_mobility")

test_that("lookup tables need Bruker's conversion and a valid frame", {
  setup_opensource()
  D <- OpenTIMS(path.d)
  on.exit(CloseTIMS(D))
  expect_error(use_mz_lookup(D), "need Bruker's conversion")
  expect_error(use_inv_ion_mobility_lookup(D), "need Bruker's conversion")
  expect_error(use_mz_lookup(D, -1), "single positive frame number")
  expect_error(use_mz_lookup(D, c(1, 2)), "single positive frame number")
})

# Bruker's library is proprietary and not available on CRAN; set
# OPENTIMSR_TEST_BRUKER_SO to its path (see download_bruker_proprietary_code) to run this.
bruker_so <- Sys.getenv("OPENTIMSR_TEST_BRUKER_SO")

test_that("lookup tables with Bruker's conversion", {
  skip_if_not(nzchar(bruker_so) && file.exists(bruker_so), "OPENTIMSR_TEST_BRUKER_SO not set")
  setup_bruker_so(bruker_so)
  on.exit(setup_opensource())
  D <- OpenTIMS(path.d)
  on.exit(CloseTIMS(D), add = TRUE)

  exact <- list(query(D, 1L, cols), query(D, 2L, cols))
  use_mz_lookup(D)
  use_inv_ion_mobility_lookup(D)
  expect_identical(query(D, 1L, cols), exact[[1]])
  expect_equal(query(D, 2L, cols)$mz, exact[[2]]$mz, tolerance = 1e-6)
  expect_identical(query(D, 2L, cols)$inv_ion_mobility, exact[[2]]$inv_ion_mobility)

  opentims_set_threads(1)
  sequential <- query(D, 1:2)
  opentims_set_threads(2)
  expect_identical(query(D, 1:2), sequential)

  use_mz_lookup(D, 2)
  expect_identical(query(D, 2L, "mz"), exact[[2]]["mz"])
  expect_error(use_mz_lookup(D, 99999), "no frame 99999")
  expect_identical(query(D, 2L, "mz"), exact[[2]]["mz"])

  use_mz_lookup(D, NULL)
  use_inv_ion_mobility_lookup(D, NULL)
  expect_identical(list(query(D, 1L, cols), query(D, 2L, cols)), exact)
})

library(opentimsr)

expected <- data.frame(
  frame     = as.integer(c(1,1,1,1,1,2,2,2,2,2)),
  scan      = as.integer(c(33,49,53,59,62,54,56,73,80,83)),
  tof       = as.integer(c(268877,36849,356994,366925,103614,399169,205746,205657,66533,315625)),
  intensity = as.integer(c(10,10,87,10,80,10,51,10,10,10)),
  mz = c(796.9985194813775, 99.42108214671732, 1236.633340924859, 1292.2118408784838,
         231.8093673885497, 1481.0866010361667, 541.1613947631356, 540.8355715906617,
         151.46433815861303, 1018.2569352019063),
  inv_ion_mobility = c(1.5640522875816993, 1.5466230936819172, 1.5422657952069718,
                       1.5357298474945535, 1.5324618736383444, 1.5411764705882354,
                       1.5389978213507627, 1.520479302832244,  1.5128540305010894,
                       1.5095860566448802),
  retention_time = c(0.644238, 0.644238, 0.644238, 0.644238, 0.644238,
                     0.751175, 0.751175, 0.751175, 0.751175, 0.751175)
)

test_that("opensource converters produce correct values", {
  setup_opensource()
  D <- OpenTIMS(test_path("test.d"))
  on.exit(CloseTIMS(D))

  result <- query(D, frames = c(1L, 2L))

  expect_equal(result$frame,            expected$frame)
  expect_equal(result$scan,             expected$scan)
  expect_equal(result$tof,              expected$tof)
  expect_equal(result$intensity,        expected$intensity)
  expect_equal(result$mz,               expected$mz,               tolerance = 1e-6)
  expect_equal(result$inv_ion_mobility, expected$inv_ion_mobility,  tolerance = 1e-6)
  expect_equal(result$retention_time,   expected$retention_time,    tolerance = 1e-6)
})

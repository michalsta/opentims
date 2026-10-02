/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <gtest/gtest.h>
#include <stdexcept>
#include <string>
#include <vector>
#include "opentims.h"
#include "tof2mz_converter.h"
#include "scan2inv_ion_mobility_converter.h"
#include "dataset_builder.h"

namespace {

class BrukerConversion : public ::testing::TestWithParam<int>
{
protected:
    TimsDataHandle handle{ottest::test_d().string(), NoPressureCompensation,
                         &ErrorTof2MzConverterFactory::instance(),
                         &ErrorScan2InvIonMobilityConverterFactory::instance()};
    BrukerTof2MzConverter mz{handle, OPENTIMS_TEST_BRUKER_MODULE};
    BrukerScan2InvIonMobilityConverter im{handle, OPENTIMS_TEST_BRUKER_MODULE};
    double doubles[2] = {-123, -123};
    uint32_t integers[2] = {123, 123};

    void convert(uint32_t frame)
    {
        const double input_d[2] = {10, 20};
        const uint32_t input_u[2] = {10, 20};
        switch(GetParam())
        {
            case 0: mz.convert(frame, doubles, input_d, 2); break;
            case 1: mz.convert(frame, doubles, input_u, 2); break;
            case 2: mz.inverse_convert(frame, integers, input_d, 2); break;
            case 3: im.convert(frame, doubles, input_d, 2); break;
            case 4: im.convert(frame, doubles, input_u, 2); break;
            case 5: im.inverse_convert(frame, integers, input_d, 2); break;
        }
    }
};

TEST_P(BrukerConversion, FailureIncludesOperationFrameAndVendorMessage)
{
    const char* operations[] = {
        "tims_index_to_mz", "tims_index_to_mz", "tims_mz_to_index",
        "tims_scannum_to_oneoverk0", "tims_scannum_to_oneoverk0", "tims_oneoverk0_to_scannum"
    };
    try
    {
        convert(2);
        FAIL() << "A failed Bruker conversion must throw";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_EQ(std::string(e.what()), std::string(operations[GetParam()]) +
                  "(frame 2) failed. Reason: Calibration unavailable for requested frame");
    }
    // In particular, failed inverse calls must not copy their temporary buffer.
    EXPECT_EQ(integers[0], 123u);
    EXPECT_EQ(integers[1], 123u);
    EXPECT_EQ(doubles[0], -123);
    EXPECT_EQ(doubles[1], -123);
}

TEST_P(BrukerConversion, SuccessPreservesConvertedOutput)
{
    ASSERT_NO_THROW(convert(1));
    if(GetParam() == 2 || GetParam() == 5)
    {
        EXPECT_EQ(integers[0], 11u);
        EXPECT_EQ(integers[1], 21u);
    }
    else
    {
        EXPECT_EQ(doubles[0], 11);
        EXPECT_EQ(doubles[1], 21);
    }
}

INSTANTIATE_TEST_SUITE_P(AllOverloads, BrukerConversion, ::testing::Range(0, 6));

TEST(BrukerErrors, ThreadedExtractionRethrowsVendorError)
{
    BrukerTof2MzConverterFactory mz_factory(OPENTIMS_TEST_BRUKER_MODULE);
    BrukerScan2InvIonMobilityConverterFactory im_factory(OPENTIMS_TEST_BRUKER_MODULE);
    TimsDataHandle handle(ottest::test_d().string(), NoPressureCompensation,
                         &mz_factory, &im_factory);
    std::vector<double> output(handle.get_frame(2).num_peaks);
    uint32_t* omitted[] = {nullptr};
    double* omitted_double[] = {nullptr};
    double* values[] = {output.data()};
    for(bool mobility : {false, true})
    {
        try
        {
            handle.extract_frames(std::vector<uint32_t>{2}, omitted, omitted, omitted, omitted,
                                  mobility ? omitted_double : values,
                                  mobility ? values : omitted_double, omitted_double);
            FAIL() << "Worker must propagate the Bruker exception";
        }
        catch(const std::runtime_error& e)
        {
            EXPECT_NE(std::string(e.what()).find("Calibration unavailable for requested frame"), std::string::npos);
        }
    }
}

} // namespace

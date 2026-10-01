/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <cctype>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "opentims.h"
#include "converters.h"
#include "tof2mz_converter.h"
#include "scan2inv_ion_mobility_converter.h"
#include "dataset_builder.h"
#include "locale_guard.h"

using namespace ottest;

namespace {

void expect_rel_near(double actual, double expected, double rel = 1e-12)
{
    EXPECT_NEAR(actual, expected, std::abs(expected) * rel) << "expected " << expected;
}

// Restores the global default converters, which other tests rely on.
struct RestoreOpenSourceDefault
{
    ~RestoreOpenSourceDefault() { setup_opensource(); }
};

DatasetSpec one_frame_spec()
{
    DatasetSpec spec;
    FrameSpec f;
    f.id = 1;
    f.scans = {{{10, 5}}, {}, {{20, 6}, {30, 7}}};
    spec.frames.push_back(f);
    return spec;
}

} // anonymous namespace

// --- OpenSourceTof2MzConverter -----------------------------------------------

TEST(OpenSourceTof2Mz, EndpointsMatchAcquisitionRange)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    const uint32_t tofs[] = {0, kTofMax};
    double mzs[2];
    conv.convert(1, mzs, tofs, 2);
    expect_rel_near(mzs[0], kMzMin);
    expect_rel_near(mzs[1], kMzMax);
}

TEST(OpenSourceTof2Mz, IsLinearInSqrt)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    EXPECT_DOUBLE_EQ(conv.intercept(), std::sqrt(kMzMin));
    EXPECT_DOUBLE_EQ(conv.slope(), (std::sqrt(kMzMax) - std::sqrt(kMzMin)) / kTofMax);
    std::vector<uint32_t> tofs;
    for(uint32_t t = 0; t <= kTofMax; t += 4321)
        tofs.push_back(t);
    std::vector<double> mzs(tofs.size());
    conv.convert(7, mzs.data(), tofs.data(), uint32_t(tofs.size()));
    for(size_t i = 0; i < tofs.size(); i++)
        expect_rel_near(mzs[i], expected_mz(tofs[i]));
    for(size_t i = 1; i < tofs.size(); i++)
        EXPECT_GT(mzs[i], mzs[i - 1]);
}

TEST(OpenSourceTof2Mz, DoubleAndIntegerOverloadsAgree)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    const uint32_t itofs[] = {0, 1, 2, 1000, 268877, kTofMax};
    const double dtofs[] = {0, 1, 2, 1000, 268877, kTofMax};
    double a[6], b[6];
    conv.convert(1, a, itofs, 6);
    conv.convert(1, b, dtofs, 6);
    for(int i = 0; i < 6; i++)
        EXPECT_EQ(a[i], b[i]);
}

TEST(OpenSourceTof2Mz, FractionalTofsInterpolate)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    const double tofs[] = {100.0, 100.5, 101.0};
    double mzs[3];
    conv.convert(1, mzs, tofs, 3);
    EXPECT_LT(mzs[0], mzs[1]);
    EXPECT_LT(mzs[1], mzs[2]);
}

TEST(OpenSourceTof2Mz, InverseRoundTrips)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    std::vector<uint32_t> tofs = {0, 1, 2, 3, 99, 36849, 268877, 399169, kTofMax - 1, kTofMax};
    std::vector<double> mzs(tofs.size());
    std::vector<uint32_t> back(tofs.size(), 12345);
    conv.convert(1, mzs.data(), tofs.data(), uint32_t(tofs.size()));
    conv.inverse_convert(1, back.data(), mzs.data(), uint32_t(tofs.size()));
    EXPECT_EQ(back, tofs);
}

TEST(OpenSourceTof2Mz, InverseClampsBelowRangeToZero)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    const double mzs[] = {0.0, 1.0, kMzMin * 0.5};
    uint32_t tofs[] = {7, 7, 7};
    conv.inverse_convert(1, tofs, mzs, 3);
    EXPECT_EQ(tofs[0], 0u);
    EXPECT_EQ(tofs[1], 0u);
    EXPECT_EQ(tofs[2], 0u);
}

TEST(OpenSourceTof2Mz, OtofControlWidensRange)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax, true);
    const uint32_t tofs[] = {0, kTofMax};
    double mzs[2];
    conv.convert(1, mzs, tofs, 2);
    expect_rel_near(mzs[0], kMzMin - 5.0);
    expect_rel_near(mzs[1], kMzMax + 5.0);
}

TEST(OpenSourceTof2Mz, ZeroSizeIsANoOp)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    conv.convert(1, static_cast<double*>(nullptr), static_cast<const uint32_t*>(nullptr), 0);
    conv.convert(1, static_cast<double*>(nullptr), static_cast<const double*>(nullptr), 0);
    conv.inverse_convert(1, nullptr, nullptr, 0);
}

TEST(OpenSourceTof2Mz, UpdateCalibration)
{
    OpenSourceTof2MzConverter conv(kMzMin, kMzMax, kTofMax);
    conv.updateCalibration(2.0, 0.5);
    EXPECT_EQ(conv.intercept(), 2.0);
    EXPECT_EQ(conv.slope(), 0.5);
    const uint32_t tof = 4;
    double mz;
    conv.convert(1, &mz, &tof, 1);
    EXPECT_EQ(mz, 16.0); // (2 + 0.5*4)^2
    EXPECT_FALSE(conv.description().empty());
}

// --- OpenSourceScan2ImConverter ----------------------------------------------

TEST(OpenSourceScan2Im, EndpointsAndLinearity)
{
    const uint32_t scan_max = 918;
    OpenSourceScan2ImConverter conv(kImMin, kImMax, scan_max);
    std::vector<uint32_t> scans;
    for(uint32_t s = 0; s <= scan_max; s++)
        scans.push_back(s);
    std::vector<double> ims(scans.size());
    conv.convert(1, ims.data(), scans.data(), uint32_t(scans.size()));
    expect_rel_near(ims.front(), kImMax);
    expect_rel_near(ims.back(), kImMin);
    for(size_t i = 0; i < scans.size(); i++)
        expect_rel_near(ims[i], expected_inv_ion_mobility(scans[i], scan_max));
    for(size_t i = 1; i < scans.size(); i++)
        EXPECT_LT(ims[i], ims[i - 1]) << "1/K0 falls with the scan number";
}

TEST(OpenSourceScan2Im, DoubleAndIntegerOverloadsAgree)
{
    OpenSourceScan2ImConverter conv(kImMin, kImMax, 918);
    const uint32_t iscans[] = {0, 1, 33, 917};
    const double dscans[] = {0, 1, 33, 917};
    double a[4], b[4];
    conv.convert(1, a, iscans, 4);
    conv.convert(1, b, dscans, 4);
    for(int i = 0; i < 4; i++)
        EXPECT_EQ(a[i], b[i]);
}

TEST(OpenSourceScan2Im, InverseRoundTrips)
{
    OpenSourceScan2ImConverter conv(kImMin, kImMax, 918);
    std::vector<uint32_t> scans = {0, 1, 2, 33, 49, 500, 917, 918};
    std::vector<double> ims(scans.size());
    std::vector<uint32_t> back(scans.size(), 777);
    conv.convert(1, ims.data(), scans.data(), uint32_t(scans.size()));
    conv.inverse_convert(1, back.data(), ims.data(), uint32_t(scans.size()));
    EXPECT_EQ(back, scans);
}

TEST(OpenSourceScan2Im, InverseClampsAboveRangeToZero)
{
    OpenSourceScan2ImConverter conv(kImMin, kImMax, 918);
    const double ims[] = {kImMax, kImMax + 0.5, 10.0};
    uint32_t scans[] = {7, 7, 7};
    conv.inverse_convert(1, scans, ims, 3);
    EXPECT_EQ(scans[0], 0u);
    EXPECT_EQ(scans[1], 0u);
    EXPECT_EQ(scans[2], 0u);
    EXPECT_FALSE(conv.description().empty());
}

// --- Error converters ---------------------------------------------------------

TEST(ErrorConverters, EveryConversionThrowsLogicError)
{
    TempDir tmp;
    write_dataset(one_frame_spec(), tmp.path());
    TimsDataHandle h(tmp.path().string(), NoPressureCompensation,
                     &ErrorTof2MzConverterFactory::instance(),
                     &ErrorScan2InvIonMobilityConverterFactory::instance());
    double d = 1.0;
    uint32_t u = 1;
    EXPECT_THROW(h.tof2mz_converter->convert(1, &d, &d, 1), std::logic_error);
    EXPECT_THROW(h.tof2mz_converter->convert(1, &d, &u, 1), std::logic_error);
    EXPECT_THROW(h.tof2mz_converter->inverse_convert(1, &u, &d, 1), std::logic_error);
    EXPECT_THROW(h.scan2inv_ion_mobility_converter->convert(1, &d, &d, 1), std::logic_error);
    EXPECT_THROW(h.scan2inv_ion_mobility_converter->convert(1, &d, &u, 1), std::logic_error);
    EXPECT_THROW(h.scan2inv_ion_mobility_converter->inverse_convert(1, &u, &d, 1), std::logic_error);
    EXPECT_FALSE(h.tof2mz_converter->description().empty());
    EXPECT_FALSE(h.scan2inv_ion_mobility_converter->description().empty());
}

TEST(ErrorConverters, RawColumnsStillExtract)
{
    TempDir tmp;
    const DatasetSpec spec = one_frame_spec();
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string(), NoPressureCompensation,
                     &ErrorTof2MzConverterFactory::instance(),
                     &ErrorScan2InvIonMobilityConverterFactory::instance());
    const Columns exp = expected_columns(spec, {1});
    Columns got;
    got.resize(exp.size());
    h.extract_frames(std::vector<uint32_t>{1}, got.frame.data(), got.scan.data(), got.tof.data(),
                     got.intensity.data(), nullptr, nullptr, got.retention_time.data());
    EXPECT_EQ(got.frame, exp.frame);
    EXPECT_EQ(got.scan, exp.scan);
    EXPECT_EQ(got.tof, exp.tof);
    EXPECT_EQ(got.intensity, exp.intensity);
    EXPECT_EQ(got.retention_time, exp.retention_time);

    EXPECT_THROW(h.extract_frames(std::vector<uint32_t>{1}, nullptr, nullptr, nullptr, nullptr,
                                  got.mz.data(), nullptr, nullptr), std::logic_error);
    EXPECT_THROW(h.extract_frames(std::vector<uint32_t>{1}, nullptr, nullptr, nullptr, nullptr,
                                  nullptr, got.inv_ion_mobility.data(), nullptr), std::logic_error);
}

// --- Default factories ---------------------------------------------------------

TEST(DefaultFactories, HandleUsesCurrentDefault)
{
    RestoreOpenSourceDefault restore;
    TempDir tmp;
    write_dataset(one_frame_spec(), tmp.path());

    DefaultTof2MzConverterFactory::setAsDefault<ErrorTof2MzConverterFactory>();
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<ErrorScan2InvIonMobilityConverterFactory>();
    {
        TimsDataHandle h(tmp.path().string());
        double d;
        uint32_t u = 0;
        EXPECT_THROW(h.tof2mz_converter->convert(1, &d, &u, 1), std::logic_error);
        EXPECT_THROW(h.scan2inv_ion_mobility_converter->convert(1, &d, &u, 1), std::logic_error);
    }

    setup_opensource();
    {
        TimsDataHandle h(tmp.path().string());
        double mz, im;
        const uint32_t tof = 268877, scan = 0;
        h.tof2mz_converter->convert(1, &mz, &tof, 1);
        h.scan2inv_ion_mobility_converter->convert(1, &im, &scan, 1);
        expect_rel_near(mz, expected_mz(tof));
        expect_rel_near(im, kImMax);
    }
}

TEST(DefaultFactories, ExplicitFactoryOverridesDefault)
{
    TempDir tmp;
    write_dataset(one_frame_spec(), tmp.path());
    TimsDataHandle h(tmp.path().string(), NoPressureCompensation,
                     &OpenSourceTof2MzConverterFactory::instance(), nullptr);
    double mz;
    const uint32_t tof = 1000;
    h.tof2mz_converter->convert(1, &mz, &tof, 1);
    expect_rel_near(mz, expected_mz(tof));
}

TEST(DefaultFactories, HandleKeepsItsConvertersAfterDefaultChanges)
{
    RestoreOpenSourceDefault restore;
    TempDir tmp;
    write_dataset(one_frame_spec(), tmp.path());
    TimsDataHandle h(tmp.path().string());
    DefaultTof2MzConverterFactory::setAsDefault<ErrorTof2MzConverterFactory>();
    double mz;
    const uint32_t tof = 1000;
    EXPECT_NO_THROW(h.tof2mz_converter->convert(1, &mz, &tof, 1));
}

// --- Open-source factories reading metadata ----------------------------------

namespace {

void open_with_opensource(const fs::path& dir, pressure_compensation_strategy pcs = NoPressureCompensation)
{
    TimsDataHandle h(dir.string(), pcs, &OpenSourceTof2MzConverterFactory::instance(),
                     &OpenSourceScan2ImConverterFactory::instance());
}

} // anonymous namespace

TEST(OpenSourceFactories, RejectPressureCompensation)
{
    TempDir tmp;
    write_dataset(one_frame_spec(), tmp.path());
    for(auto pcs : {AnalyisGlobalPressureCompensation, PerFramePressureCompensation,
                    PerFramePressureCompensationWithMissingReference})
        EXPECT_THROW(open_with_opensource(tmp.path(), pcs), std::runtime_error) << pcs;
    EXPECT_NO_THROW(open_with_opensource(tmp.path(), NoPressureCompensation));
}

class OpenSourceFactoryMetadata : public ::testing::TestWithParam<std::pair<std::string, std::string>> {};

TEST_P(OpenSourceFactoryMetadata, InvalidCalibrationIsRejected)
{
    const auto& [key, value] = GetParam();
    DatasetSpec spec = one_frame_spec();
    if(value == "<absent>")
        spec.metadata.erase(key);
    else
        spec.metadata[key] = value;
    TempDir tmp;
    write_dataset(spec, tmp.path());
    try
    {
        open_with_opensource(tmp.path());
        FAIL() << "expected an exception";
    }
    catch(const std::runtime_error& e)
    {
        EXPECT_NE(std::string(e.what()).find("invalid calibration metadata"), std::string::npos) << e.what();
    }
}

INSTANTIATE_TEST_SUITE_P(
    Keys, OpenSourceFactoryMetadata,
    ::testing::Values(
        std::make_pair(std::string("MzAcqRangeLower"), std::string("<absent>")),
        std::make_pair(std::string("MzAcqRangeUpper"), std::string("<absent>")),
        std::make_pair(std::string("DigitizerNumSamples"), std::string("<absent>")),
        std::make_pair(std::string("OneOverK0AcqRangeLower"), std::string("<absent>")),
        std::make_pair(std::string("OneOverK0AcqRangeUpper"), std::string("<absent>")),
        std::make_pair(std::string("MzAcqRangeLower"), std::string("0")),
        std::make_pair(std::string("MzAcqRangeLower"), std::string("-5")),
        std::make_pair(std::string("MzAcqRangeLower"), std::string("garbage")),
        std::make_pair(std::string("MzAcqRangeUpper"), std::string("50.0")),  // == lower
        std::make_pair(std::string("MzAcqRangeUpper"), std::string("10.0")),  // < lower
        std::make_pair(std::string("DigitizerNumSamples"), std::string("0")),
        std::make_pair(std::string("OneOverK0AcqRangeLower"), std::string("0")),
        std::make_pair(std::string("OneOverK0AcqRangeUpper"), std::string("0.6")),
        std::make_pair(std::string("OneOverK0AcqRangeUpper"), std::string("0.1"))),
    [](const auto& info) {
        std::string name = info.param.first + "_" + info.param.second;
        for(char& c : name)
            if(!std::isalnum(static_cast<unsigned char>(c)))
                c = '_';
        return name + "_" + std::to_string(info.index);
    });

TEST(OpenSourceFactories, NoFramesMeansNoScanCalibration)
{
    DatasetSpec spec;
    TempDir tmp;
    write_dataset(spec, tmp.path());
    EXPECT_THROW(open_with_opensource(tmp.path()), std::runtime_error);
}

TEST(OpenSourceFactories, OtofControlIsDetected)
{
    DatasetSpec spec = one_frame_spec();
    spec.metadata["AcquisitionSoftware"] = "Bruker otofControl";
    TempDir tmp;
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string(), NoPressureCompensation, &OpenSourceTof2MzConverterFactory::instance(), nullptr);
    double mz;
    const uint32_t tof = 0;
    h.tof2mz_converter->convert(1, &mz, &tof, 1);
    expect_rel_near(mz, kMzMin - 5.0);
}

TEST(OpenSourceFactories, ScanCalibrationUsesMaxNumScansOverFrames)
{
    DatasetSpec spec;
    FrameSpec a, b;
    a.id = 1;
    a.scans.resize(10);
    b.id = 2;
    b.scans.resize(40);
    spec.frames = {a, b};
    TempDir tmp;
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string(), NoPressureCompensation, nullptr, &OpenSourceScan2ImConverterFactory::instance());
    double im;
    const uint32_t scan = 40;
    h.scan2inv_ion_mobility_converter->convert(1, &im, &scan, 1);
    expect_rel_near(im, kImMin);
}

TEST(OpenSourceFactories, MetadataParsingIgnoresLocale)
{
    TempDir tmp;
    write_dataset(one_frame_spec(), tmp.path());
    ottest::LocaleGuard guard;
    if(!guard.set_comma_decimal_locale())
        GTEST_SKIP() << "no comma-decimal locale installed";
    TimsDataHandle h(tmp.path().string(), NoPressureCompensation, &OpenSourceTof2MzConverterFactory::instance(),
                     &OpenSourceScan2ImConverterFactory::instance());
    double mz, im;
    const uint32_t tof = kTofMax, scan = 0;
    h.tof2mz_converter->convert(1, &mz, &tof, 1);
    h.scan2inv_ion_mobility_converter->convert(1, &im, &scan, 1);
    expect_rel_near(mz, kMzMax);
    expect_rel_near(im, kImMax);
}

// --- Bruker factories without the proprietary library ---------------------------

TEST(BrukerFactories, MissingLibraryThrowsRuntimeError)
{
    EXPECT_THROW(BrukerTof2MzConverterFactory("/nonexistent/libtimsdata.so"), std::runtime_error);
    EXPECT_THROW(BrukerScan2InvIonMobilityConverterFactory("/nonexistent/libtimsdata.so"), std::runtime_error);
}

TEST(BrukerFactories, SetupBrukerWithMissingLibraryThrows)
{
    RestoreOpenSourceDefault restore;
    EXPECT_THROW(setup_bruker("/nonexistent/libtimsdata.so"), std::runtime_error);
}

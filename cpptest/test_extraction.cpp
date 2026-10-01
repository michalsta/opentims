/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// Peak extraction through every public entry point, against known contents.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "opentims.h"
#include "thread_mgr.h"
#include "dataset_builder.h"

using namespace ottest;

namespace {

void expect_columns_eq(const Columns& got, const Columns& exp)
{
    EXPECT_EQ(got.frame, exp.frame);
    EXPECT_EQ(got.scan, exp.scan);
    EXPECT_EQ(got.tof, exp.tof);
    EXPECT_EQ(got.intensity, exp.intensity);
    EXPECT_EQ(got.retention_time, exp.retention_time);
    ASSERT_EQ(got.mz.size(), exp.mz.size());
    for(size_t i = 0; i < exp.size(); i++)
    {
        EXPECT_NEAR(got.mz[i], exp.mz[i], 1e-12 * exp.mz[i]) << "peak " << i;
        EXPECT_NEAR(got.inv_ion_mobility[i], exp.inv_ion_mobility[i], 1e-12) << "peak " << i;
    }
}

Columns extract_all(TimsDataHandle& h, const std::vector<uint32_t>& ids)
{
    Columns c;
    c.resize(h.no_peaks_in_frames(ids));
    h.extract_frames(ids, c.frame.data(), c.scan.data(), c.tof.data(), c.intensity.data(),
                     c.mz.data(), c.inv_ion_mobility.data(), c.retention_time.data());
    return c;
}

std::vector<uint32_t> all_ids(const DatasetSpec& spec)
{
    std::vector<uint32_t> ids;
    for(const FrameSpec& f : spec.frames)
        ids.push_back(f.id);
    return ids;
}

// Restores the default thread count when a test changes it.
struct ThreadCountGuard
{
    ~ThreadCountGuard() { ThreadingManager::get_instance().set_num_threads(0); }
};

} // anonymous namespace

// --- pytest/test.d, real zstd-compressed frames ---------------------------------

namespace {
struct GoldenPeak { uint32_t frame, scan, tof, intensity; double mz, im, rt; };
// The same values pytest/test_opensource.py pins.
const GoldenPeak kGolden[] = {
    {1, 33, 268877, 10, 796.9985194813775, 1.5640522875816993, 0.644238},
    {1, 49, 36849, 10, 99.42108214671732, 1.5466230936819172, 0.644238},
    {1, 53, 356994, 87, 1236.633340924859, 1.5422657952069718, 0.644238},
    {1, 59, 366925, 10, 1292.2118408784838, 1.5357298474945535, 0.644238},
    {1, 62, 103614, 80, 231.8093673885497, 1.5324618736383444, 0.644238},
    {2, 54, 399169, 10, 1481.0866010361667, 1.5411764705882354, 0.751175},
    {2, 56, 205746, 51, 541.1613947631356, 1.5389978213507627, 0.751175},
    {2, 73, 205657, 10, 540.8355715906617, 1.520479302832244, 0.751175},
    {2, 80, 66533, 10, 151.46433815861303, 1.5128540305010894, 0.751175},
    {2, 83, 315625, 10, 1018.2569352019063, 1.5095860566448802, 0.751175},
};

void expect_golden(const Columns& c, size_t first = 0)
{
    for(size_t i = 0; i < c.size(); i++)
    {
        const GoldenPeak& g = kGolden[first + i];
        EXPECT_EQ(c.frame[i], g.frame) << i;
        EXPECT_EQ(c.scan[i], g.scan) << i;
        EXPECT_EQ(c.tof[i], g.tof) << i;
        EXPECT_EQ(c.intensity[i], g.intensity) << i;
        EXPECT_NEAR(c.mz[i], g.mz, 1e-9) << i;
        EXPECT_NEAR(c.inv_ion_mobility[i], g.im, 1e-12) << i;
        EXPECT_NEAR(c.retention_time[i], g.rt, 1e-12) << i;
    }
}
} // anonymous namespace

TEST(TestD, ExtractFramesMatchesGolden)
{
    TimsDataHandle h(test_d().string());
    const Columns c = extract_all(h, {1, 2});
    ASSERT_EQ(c.size(), 10u);
    expect_golden(c);
}

TEST(TestD, SliceMatchesGolden)
{
    TimsDataHandle h(test_d().string());
    Columns c;
    c.resize(h.no_peaks_in_slice(1, 3, 1));
    ASSERT_EQ(c.size(), 10u);
    h.extract_frames_slice(1, 3, 1, c.frame.data(), c.scan.data(), c.tof.data(), c.intensity.data(),
                           c.mz.data(), c.inv_ion_mobility.data(), c.retention_time.data());
    expect_golden(c);
}

TEST(TestD, SecondFrameAlone)
{
    TimsDataHandle h(test_d().string());
    const Columns c = extract_all(h, {2});
    ASSERT_EQ(c.size(), 5u);
    expect_golden(c, 5);
}

TEST(TestD, SliceStepDoesNotWrap)
{
    TimsDataHandle h(test_d().string());
    const uint32_t step = (std::numeric_limits<uint32_t>::max)();
    const Columns expected = extract_all(h, {1});
    ASSERT_EQ(h.no_peaks_in_slice(1, 3, step), expected.size());
    Columns actual;
    actual.resize(expected.size());
    h.extract_frames_slice(1, 3, step, actual.frame.data(), actual.scan.data(), actual.tof.data(),
                          actual.intensity.data(), actual.mz.data(), actual.inv_ion_mobility.data(),
                          actual.retention_time.data());
    expect_columns_eq(actual, expected);

    std::vector<uint32_t> matrix(4 * expected.size()), expected_matrix(matrix.size());
    h.get_frame(1).save_to_matrix_buffer(expected_matrix.data());
    h.extract_frames_slice(1, 3, step, matrix.data());
    EXPECT_EQ(matrix, expected_matrix);
}

TEST(TestD, PerFrameTIC)
{
    TimsDataHandle h(test_d().string());
    uint32_t tic[2] = {99, 99};
    h.per_frame_TIC(tic);
    EXPECT_EQ(tic[0], 10u + 10 + 87 + 10 + 80);
    EXPECT_EQ(tic[1], 10u + 51 + 10 + 10 + 10);
}

TEST(TestD, MatrixBufferLayout)
{
    TimsDataHandle h(test_d().string());
    std::vector<uint32_t> buf(4 * 5);
    h.get_frame(2).save_to_matrix_buffer(buf.data());
    for(size_t i = 0; i < 5; i++)
    {
        EXPECT_EQ(buf[i], 2u);
        EXPECT_EQ(buf[5 + i], kGolden[5 + i].scan);
        EXPECT_EQ(buf[10 + i], kGolden[5 + i].tof);
        EXPECT_EQ(buf[15 + i], kGolden[5 + i].intensity);
    }
}

// --- Synthetic datasets ---------------------------------------------------------

class Synthetic : public ::testing::Test
{
 protected:
    TempDir tmp;
    DatasetSpec spec;
    std::unique_ptr<TimsDataHandle> h;

    void open(DatasetSpec s)
    {
        spec = std::move(s);
        write_dataset(spec, tmp.path());
        h = std::make_unique<TimsDataHandle>(tmp.path().string());
    }
    void TearDown() override { h.reset(); }
};

TEST_F(Synthetic, RandomDatasetRoundTrips)
{
    open(random_dataset(1, 40, 60, 12));
    const auto ids = all_ids(spec);
    expect_columns_eq(extract_all(*h, ids), expected_columns(spec, ids));
}

TEST_F(Synthetic, ArbitraryOrderAndRepeats)
{
    open(random_dataset(2, 10, 20, 8, 1, 1, 0));
    const std::vector<uint32_t> ids = {7, 1, 7, 10, 3, 3, 3};
    expect_columns_eq(extract_all(*h, ids), expected_columns(spec, ids));
}

TEST_F(Synthetic, EmptySelection)
{
    open(random_dataset(3, 4, 5, 5));
    const Columns c = extract_all(*h, {});
    EXPECT_EQ(c.size(), 0u);
}

TEST_F(Synthetic, SliceWithSteps)
{
    open(random_dataset(4, 30, 15, 6));
    for(uint32_t step : {1u, 2u, 3u, 7u, 29u, 30u, 100u})
        for(uint32_t start : {1u, 2u, 5u})
            for(uint32_t end : {start, start + 1, 17u, 31u})
            {
                std::vector<uint32_t> ids;
                for(uint32_t id = start; id < end; id += step)
                    ids.push_back(id);
                Columns c;
                c.resize(h->no_peaks_in_slice(start, end, step));
                h->extract_frames_slice(start, end, step, c.frame.data(), c.scan.data(), c.tof.data(),
                                        c.intensity.data(), c.mz.data(), c.inv_ion_mobility.data(),
                                        c.retention_time.data());
                expect_columns_eq(c, expected_columns(spec, ids));
            }
    uint32_t dummy;
    EXPECT_THROW(h->extract_frames_slice(1, 5, 0, &dummy, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
                 std::runtime_error);
}

TEST_F(Synthetic, MissingIdsThrowOutOfRange)
{
    open(random_dataset(5, 5, 5, 5, 10, 2));
    std::vector<uint32_t> buf(1000);
    EXPECT_THROW(h->extract_frames(std::vector<uint32_t>{11}, buf.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
                 std::out_of_range);
    EXPECT_THROW(h->extract_frames_slice(10, 13, 1, buf.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
                 std::out_of_range);
    EXPECT_THROW(h->get_frame(0), std::out_of_range);
}

// Every subset of output columns, the rest nullptr.
TEST_F(Synthetic, AnySubsetOfColumns)
{
    open(random_dataset(6, 6, 25, 9, 1, 1, 3));
    const auto ids = all_ids(spec);
    const Columns exp = expected_columns(spec, ids);
    const size_t n = exp.size();
    ASSERT_GT(n, 0u);
    for(unsigned mask = 0; mask < (1u << 7); mask++)
    {
        SCOPED_TRACE(mask);
        Columns c;
        c.resize(n);
        auto pick = [mask](unsigned bit, auto* p) { return (mask >> bit) & 1 ? p : nullptr; };
        h->extract_frames(ids, pick(0, c.frame.data()), pick(1, c.scan.data()), pick(2, c.tof.data()),
                          pick(3, c.intensity.data()), pick(4, c.mz.data()), pick(5, c.inv_ion_mobility.data()),
                          pick(6, c.retention_time.data()));
        if(mask & 1) { EXPECT_EQ(c.frame, exp.frame); }
        if(mask & 2) { EXPECT_EQ(c.scan, exp.scan); }
        if(mask & 4) { EXPECT_EQ(c.tof, exp.tof); }
        if(mask & 8) { EXPECT_EQ(c.intensity, exp.intensity); }
        if(mask & 16)
        {
            for(size_t i = 0; i < n; i++)
                ASSERT_NEAR(c.mz[i], exp.mz[i], 1e-12 * exp.mz[i]);
        }
        if(mask & 32)
        {
            for(size_t i = 0; i < n; i++)
                ASSERT_NEAR(c.inv_ion_mobility[i], exp.inv_ion_mobility[i], 1e-12);
        }
        if(mask & 64) { EXPECT_EQ(c.retention_time, exp.retention_time); }
    }
}

TEST_F(Synthetic, FrameSaveToBuffsBothOverloads)
{
    open(random_dataset(7, 8, 30, 10));
    for(const FrameSpec& fs : spec.frames)
    {
        TimsFrame& f = h->get_frame(fs.id);
        const Columns exp = expected_columns(spec, {fs.id});
        Columns a, b, c;
        a.resize(exp.size());
        b.resize(exp.size());
        c.resize(exp.size());
        f.save_to_buffs(a.frame.data(), a.scan.data(), a.tof.data(), a.intensity.data(), a.mz.data(),
                        a.inv_ion_mobility.data(), a.retention_time.data());
        const TimsFrame& cf = f;
        cf.save_to_buffs(b.frame.data(), b.scan.data(), b.tof.data(), b.intensity.data(), b.mz.data(),
                         b.inv_ion_mobility.data(), b.retention_time.data(), nullptr, nullptr);
        std::vector<char> buf(f.data_size_bytes());
        std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> ctx(ZSTD_createDCtx(), &ZSTD_freeDCtx);
        cf.save_to_buffs(c.frame.data(), c.scan.data(), c.tof.data(), c.intensity.data(), c.mz.data(),
                         c.inv_ion_mobility.data(), c.retention_time.data(), buf.data(), ctx.get());
        expect_columns_eq(a, exp);
        expect_columns_eq(b, exp);
        expect_columns_eq(c, exp);
    }
}

TEST_F(Synthetic, DeprecatedMatrixExtraction)
{
    open(random_dataset(8, 12, 20, 7));
    const std::vector<uint32_t> ids = {2, 5, 9, 12};
    const Columns exp = expected_columns(spec, ids);
    const size_t n = exp.size();

    std::vector<uint32_t> m(4 * n);
    h->extract_frames(ids, m.data());
    EXPECT_EQ(std::vector<uint32_t>(m.begin(), m.begin() + n), exp.frame);
    EXPECT_EQ(std::vector<uint32_t>(m.begin() + n, m.begin() + 2 * n), exp.scan);
    EXPECT_EQ(std::vector<uint32_t>(m.begin() + 2 * n, m.begin() + 3 * n), exp.tof);
    EXPECT_EQ(std::vector<uint32_t>(m.begin() + 3 * n, m.end()), exp.intensity);

    const Columns sexp = expected_columns(spec, {3, 6, 9, 12});
    std::vector<uint32_t> s(4 * sexp.size());
    h->extract_frames_slice(3, 13, 3, s.data());
    EXPECT_EQ(std::vector<uint32_t>(s.begin(), s.begin() + sexp.size()), sexp.frame);
    EXPECT_EQ(std::vector<uint32_t>(s.begin() + 3 * sexp.size(), s.end()), sexp.intensity);
    EXPECT_THROW(h->extract_frames_slice(3, 13, 0, s.data()), std::runtime_error);
}

TEST_F(Synthetic, ExposeFrame)
{
    open(random_dataset(9, 10, 20, 10));
    EXPECT_EQ(h->scan_ids_buffer(), nullptr);
    for(const FrameSpec& fs : spec.frames)
    {
        const Columns exp = expected_columns(spec, {fs.id});
        ASSERT_EQ(h->expose_frame(fs.id), exp.size());
        for(size_t i = 0; i < exp.size(); i++)
        {
            ASSERT_EQ(h->scan_ids_buffer()[i], exp.scan[i]);
            ASSERT_EQ(h->tofs_buffer()[i], exp.tof[i]);
            ASSERT_EQ(h->intensities_buffer()[i], exp.intensity[i]);
        }
    }
    h->free_buffers();
    EXPECT_EQ(h->tofs_buffer(), nullptr);
    EXPECT_EQ(h->expose_frame(spec.frames[0].id), spec.frames[0].num_peaks()) << "re-allocates on demand";
}

TEST_F(Synthetic, PerFrameTICWithGapsAndEmptyFrames)
{
    open(random_dataset(10, 9, 10, 6, 100, 3, 4));
    const uint32_t span = h->max_frame_id() - h->min_frame_id() + 1;
    ASSERT_EQ(span, 25u);
    std::vector<uint32_t> tic(span, 0xDEADBEEF);
    h->per_frame_TIC(tic.data());
    std::vector<uint32_t> exp(span, 0);
    for(const FrameSpec& f : spec.frames)
        for(const auto& scan : f.scans)
            for(const Peak& p : scan)
                exp[f.id - 100] += corrected_intensity(p.intensity, f.accumulation_time);
    EXPECT_EQ(tic, exp);
}

TEST_F(Synthetic, PerFrameTICAllFramesEmpty)
{
    DatasetSpec s;
    for(uint32_t id = 1; id <= 3; id++)
    {
        FrameSpec f;
        f.id = id;
        f.scans.resize(4);
        s.frames.push_back(f);
    }
    open(s);
    std::vector<uint32_t> tic(3, 0xDEADBEEF);
    h->per_frame_TIC(tic.data());
    EXPECT_EQ(tic, std::vector<uint32_t>(3, 0));
}

TEST_F(Synthetic, ThreadedPointerArrayExtraction)
{
    ThreadCountGuard guard;
    open(random_dataset(12, 50, 40, 10));
    std::vector<uint32_t> ids = all_ids(spec);
    std::reverse(ids.begin(), ids.end());
    ids.push_back(ids.front()); // a repeat

    for(size_t threads : {1u, 2u, 3u, 8u, 64u})
    {
        SCOPED_TRACE(threads);
        ThreadingManager::get_instance().set_num_threads(threads);
        std::vector<Columns> per(ids.size());
        std::vector<uint32_t*> fr(ids.size()), sc(ids.size()), tf(ids.size()), in(ids.size());
        std::vector<double*> mz(ids.size()), im(ids.size()), rt(ids.size());
        for(size_t i = 0; i < ids.size(); i++)
        {
            per[i].resize(h->get_frame(ids[i]).num_peaks);
            fr[i] = per[i].frame.data();
            sc[i] = per[i].scan.data();
            tf[i] = per[i].tof.data();
            in[i] = per[i].intensity.data();
            mz[i] = per[i].mz.data();
            im[i] = per[i].inv_ion_mobility.data();
            rt[i] = per[i].retention_time.data();
        }
        h->extract_frames(ids, fr.data(), sc.data(), tf.data(), in.data(), mz.data(), im.data(), rt.data());
        for(size_t i = 0; i < ids.size(); i++)
            expect_columns_eq(per[i], expected_columns(spec, {ids[i]}));
    }
}

TEST_F(Synthetic, ThreadedPointerArrayWithNullColumns)
{
    open(random_dataset(13, 10, 10, 10));
    const std::vector<uint32_t> ids = all_ids(spec);
    std::vector<std::vector<uint32_t>> tofs(ids.size());
    std::vector<uint32_t*> tf(ids.size());
    for(size_t i = 0; i < ids.size(); i++)
    {
        tofs[i].resize(h->get_frame(ids[i]).num_peaks);
        tf[i] = tofs[i].data();
    }
    std::vector<uint32_t*> nul32(ids.size(), nullptr);
    std::vector<double*> nuld(ids.size(), nullptr);
    h->extract_frames(ids, nul32.data(), nul32.data(), tf.data(), nul32.data(), nuld.data(), nuld.data(), nuld.data());
    for(size_t i = 0; i < ids.size(); i++)
        EXPECT_EQ(tofs[i], expected_columns(spec, {ids[i]}).tof);
}

// --- Frame contents at the edges of the format ------------------------------------

TEST_F(Synthetic, EdgeShapes)
{
    DatasetSpec s;
    auto add = [&s](std::vector<std::vector<Peak>> scans) {
        FrameSpec f;
        f.id = uint32_t(s.frames.size() + 1);
        f.time = f.id * 0.25;
        f.scans = std::move(scans);
        s.frames.push_back(f);
    };
    add({{{0, 1}}});                                         // one scan, tof 0
    add({{{5, 1}}, {}, {}, {}});                             // only the first scan has peaks
    add({{}, {}, {}, {{5, 1}, {6, 2}}});                     // only the last scan has peaks
    add({{}, {}, {}});                                       // scans, no peaks
    add({{{7, 1}, {7, 2}, {7, 3}}});                         // repeated tof: zero deltas
    add({{{0xFFFFFFFFu, 4}}, {{0, 5}, {0xFFFFFFFEu, 6}}});   // the full uint32 tof range
    add({{{1, 0}, {2, 0xFFFFFFFFu}}});                       // extreme intensities
    std::vector<std::vector<Peak>> many(1000);
    for(uint32_t i = 0; i < many.size(); i += 7)
        many[i].push_back({i * 13, i + 1});
    add(many);                                               // many scans, sparse peaks
    open(s);
    const auto ids = all_ids(spec);
    const Columns got = extract_all(*h, ids);
    const Columns exp = expected_columns(spec, ids);
    EXPECT_EQ(got.frame, exp.frame);
    EXPECT_EQ(got.scan, exp.scan);
    EXPECT_EQ(got.tof, exp.tof);
    EXPECT_EQ(got.intensity, exp.intensity);
    EXPECT_EQ(got.retention_time, exp.retention_time);
}

TEST_F(Synthetic, FrameLargerThanOneZstdBlock)
{
    DatasetSpec s;
    FrameSpec f;
    f.id = 1;
    f.scans.resize(500);
    uint32_t tof = 0;
    for(uint32_t i = 0; i < 60000; i++)
    {
        tof += 1 + (i % 5);
        f.scans[i % 500].push_back({tof, 1 + i % 1000});
    }
    for(auto& scan : f.scans)
        std::sort(scan.begin(), scan.end(), [](const Peak& a, const Peak& b) { return a.tof < b.tof; });
    s.frames = {f};
    open(s);
    ASSERT_GT(h->get_frame(1).data_size_bytes(), 3u * 128 * 1024);
    expect_columns_eq(extract_all(*h, {1}), expected_columns(spec, {1}));
}

TEST_F(Synthetic, IntensityCorrection)
{
    DatasetSpec s;
    FrameSpec f;
    f.id = 1;
    f.accumulation_time = 40.0; // x2.5
    f.scans = {{{1, 0}, {2, 1}, {3, 2}, {4, 3}, {5, 1000}}};
    s.frames = {f};
    open(s);
    const Columns c = extract_all(*h, {1});
    // 0 -> 0.5, 2.5 -> 3, 5 -> 5.5, 7.5 -> 8, 2500 -> 2500.5; truncated
    EXPECT_EQ(c.intensity, (std::vector<uint32_t>{0, 3, 5, 8, 2500}));
}

TEST_F(Synthetic, IntensityCorrectionSaturates)
{
    DatasetSpec s;
    FrameSpec f;
    f.id = 1;
    f.accumulation_time = 50.0; // x2
    f.scans = {{{1, 0x7FFFFFFFu}, {2, 0x80000000u}, {3, 0xFFFFFFFFu}}};
    FrameSpec g;
    g.id = 2;
    g.sql_accumulation_time = "0"; // correction 100/0 = inf
    g.scans = {{{1, 0}, {2, 1}}};
    FrameSpec k;
    k.id = 3;
    k.sql_accumulation_time = "-100"; // correction -1
    k.scans = {{{1, 0}, {2, 5}}};
    s.frames = {f, g, k};
    open(s);
    EXPECT_EQ(extract_all(*h, {1}).intensity, (std::vector<uint32_t>{0xFFFFFFFEu, 0xFFFFFFFFu, 0xFFFFFFFFu}));
    EXPECT_EQ(extract_all(*h, {2}).intensity, (std::vector<uint32_t>{0, 0xFFFFFFFFu})) << "0*inf is NaN";
    EXPECT_EQ(extract_all(*h, {3}).intensity, (std::vector<uint32_t>{0, 0}));
}

// --- decompress() caching ---------------------------------------------------------

TEST_F(Synthetic, DecompressCachesAndCloseReleases)
{
    open(random_dataset(14, 4, 20, 10, 1, 1, 0));
    TimsFrame& f = h->get_frame(2);
    const Columns exp = expected_columns(spec, {2});
    ASSERT_GT(exp.size(), 0u);

    std::vector<char> buf(f.data_size_bytes());
    f.decompress(buf.data());

    Columns c;
    c.resize(exp.size());
    f.save_to_buffs(nullptr, c.scan.data(), c.tof.data(), c.intensity.data(), nullptr, nullptr, nullptr);
    EXPECT_EQ(c.tof, exp.tof);

    // Zeroing the caller's buffer shows the non-const overload reads the cache...
    std::fill(buf.begin(), buf.end(), 0);
    f.save_to_buffs(nullptr, c.scan.data(), c.tof.data(), c.intensity.data(), nullptr, nullptr, nullptr);
    EXPECT_NE(c.tof, exp.tof);
    EXPECT_EQ(c.scan, std::vector<uint32_t>(exp.size(), f.num_scans - 1)) << "all-zero headers put every peak in the last scan";

    // ...while the const overload never does.
    const TimsFrame& cf = f;
    cf.save_to_buffs(nullptr, c.scan.data(), c.tof.data(), c.intensity.data(), nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(c.tof, exp.tof);

    f.close();
    f.save_to_buffs(nullptr, c.scan.data(), c.tof.data(), c.intensity.data(), nullptr, nullptr, nullptr);
    EXPECT_EQ(c.tof, exp.tof);
    EXPECT_EQ(c.scan, exp.scan);
}

TEST_F(Synthetic, DecompressedFramesDoNotAlias)
{
    open(random_dataset(15, 6, 20, 10, 1, 1, 0));
    for(const FrameSpec& fs : spec.frames)
        h->get_frame(fs.id).decompress(); // own buffers
    // Extract in an order unlike the decompression order.
    std::vector<uint32_t> ids = all_ids(spec);
    std::reverse(ids.begin(), ids.end());
    expect_columns_eq(extract_all(*h, ids), expected_columns(spec, ids));
    for(const FrameSpec& fs : spec.frames)
        h->get_frame(fs.id).close();
    expect_columns_eq(extract_all(*h, ids), expected_columns(spec, ids));
}

TEST_F(Synthetic, DecompressIsRepeatableAndCloseIsIdempotent)
{
    open(random_dataset(16, 2, 10, 10, 1, 1, 0));
    TimsFrame& f = h->get_frame(1);
    f.close(); // never decompressed
    for(int i = 0; i < 3; i++)
        f.decompress();
    std::vector<char> buf(f.data_size_bytes());
    f.decompress(buf.data()); // replaces an owned buffer with a borrowed one
    f.close();
    f.close();
    expect_columns_eq(extract_all(*h, {1}), expected_columns(spec, {1}));
}

TEST_F(Synthetic, DecompressWithOwnContext)
{
    open(random_dataset(17, 2, 10, 10, 1, 1, 0));
    std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> ctx(ZSTD_createDCtx(), &ZSTD_freeDCtx);
    TimsFrame& f = h->get_frame(1);
    f.decompress(nullptr, ctx.get());
    const Columns exp = expected_columns(spec, {1});
    Columns c;
    c.resize(exp.size());
    f.save_to_buffs(c.frame.data(), c.scan.data(), c.tof.data(), c.intensity.data(), c.mz.data(),
                    c.inv_ion_mobility.data(), c.retention_time.data(), ctx.get());
    expect_columns_eq(c, exp);
    f.close();
}

TEST_F(Synthetic, HandleSurvivesTheFilesBeingDeleted)
{
#if defined(OPENTIMS_WINDOWS)
    GTEST_SKIP() << "Windows cannot delete files that are open";
#else
    open(random_dataset(18, 3, 10, 10));
    fs::remove(tmp / "analysis.tdf_bin");
    fs::remove(tmp / "analysis.tdf");
    const auto ids = all_ids(spec);
    expect_columns_eq(extract_all(*h, ids), expected_columns(spec, ids));
#endif
}

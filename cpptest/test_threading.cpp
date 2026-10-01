/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// Concurrency. These pass or fail on their own, but are mostly here for
// ThreadSanitizer, which turns a silent data race into a failure.

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "opentims.h"
#include "thread_mgr.h"
#include "dataset_builder.h"

using namespace ottest;

namespace {

struct ThreadCountGuard
{
    ~ThreadCountGuard() { ThreadingManager::get_instance().set_num_threads(0); }
};

std::vector<uint32_t> all_ids(const DatasetSpec& spec)
{
    std::vector<uint32_t> ids;
    for(const FrameSpec& f : spec.frames)
        ids.push_back(f.id);
    return ids;
}

// Threaded pointer-array extraction of `ids`, checked against `spec`.
bool threaded_extract_matches(TimsDataHandle& h, const DatasetSpec& spec, const std::vector<uint32_t>& ids)
{
    std::vector<Columns> per(ids.size());
    std::vector<uint32_t*> fr(ids.size()), sc(ids.size()), tf(ids.size()), in(ids.size());
    std::vector<double*> mz(ids.size()), im(ids.size()), rt(ids.size());
    for(size_t i = 0; i < ids.size(); i++)
    {
        per[i].resize(h.get_frame(ids[i]).num_peaks);
        fr[i] = per[i].frame.data();
        sc[i] = per[i].scan.data();
        tf[i] = per[i].tof.data();
        in[i] = per[i].intensity.data();
        mz[i] = per[i].mz.data();
        im[i] = per[i].inv_ion_mobility.data();
        rt[i] = per[i].retention_time.data();
    }
    h.extract_frames(ids, fr.data(), sc.data(), tf.data(), in.data(), mz.data(), im.data(), rt.data());
    for(size_t i = 0; i < ids.size(); i++)
    {
        const Columns exp = expected_columns(spec, {ids[i]});
        if(per[i].frame != exp.frame || per[i].scan != exp.scan || per[i].tof != exp.tof ||
           per[i].intensity != exp.intensity || per[i].retention_time != exp.retention_time)
            return false;
    }
    return true;
}

} // anonymous namespace

TEST(Threading, ConstSaveToBuffsOnOneFrameFromManyThreads)
{
    TempDir tmp;
    const DatasetSpec spec = random_dataset(21, 3, 40, 15, 1, 1, 0);
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string());
    const TimsFrame& f = h.get_frame(2);
    const Columns exp = expected_columns(spec, {2});

    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for(int t = 0; t < 8; t++)
        threads.emplace_back([&, t]() {
            std::vector<char> buf(f.data_size_bytes());
            std::unique_ptr<ZSTD_DCtx, decltype(&ZSTD_freeDCtx)> ctx(ZSTD_createDCtx(), &ZSTD_freeDCtx);
            for(int i = 0; i < 50; i++)
            {
                Columns c;
                c.resize(exp.size());
                // Alternate between caller-provided and per-call state.
                const bool own = (i + t) % 2 == 0;
                f.save_to_buffs(c.frame.data(), c.scan.data(), c.tof.data(), c.intensity.data(), c.mz.data(),
                                c.inv_ion_mobility.data(), c.retention_time.data(),
                                own ? buf.data() : nullptr, own ? ctx.get() : nullptr);
                if(c.tof != exp.tof || c.scan != exp.scan || c.intensity != exp.intensity)
                    mismatches++;
            }
        });
    for(auto& th : threads)
        th.join();
    EXPECT_EQ(mismatches.load(), 0);
}

TEST(Threading, ConstSaveToBuffsOnDifferentFrames)
{
    TempDir tmp;
    const DatasetSpec spec = random_dataset(22, 16, 30, 10);
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string());

    std::atomic<int> mismatches{0};
    std::vector<std::thread> threads;
    for(const FrameSpec& fs : spec.frames)
        threads.emplace_back([&, id = fs.id]() {
            const TimsFrame& f = h.get_frame(id);
            const Columns exp = expected_columns(spec, {id});
            for(int i = 0; i < 20; i++)
            {
                Columns c;
                c.resize(exp.size());
                f.save_to_buffs(nullptr, c.scan.data(), c.tof.data(), c.intensity.data(), nullptr, nullptr,
                                nullptr, nullptr, nullptr);
                if(c.tof != exp.tof || c.scan != exp.scan || c.intensity != exp.intensity)
                    mismatches++;
            }
        });
    for(auto& th : threads)
        th.join();
    EXPECT_EQ(mismatches.load(), 0);
}

TEST(Threading, ThreadedExtractionWithManyWorkers)
{
    ThreadCountGuard guard;
    TempDir tmp;
    const DatasetSpec spec = random_dataset(23, 120, 50, 10);
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string());
    for(size_t n : {1u, 4u, 16u, 100u})
    {
        ThreadingManager::get_instance().set_num_threads(n);
        EXPECT_TRUE(threaded_extract_matches(h, spec, all_ids(spec))) << n << " threads";
    }
}

TEST(Threading, HandlesOpenedAndUsedConcurrently)
{
    TempDir tmp;
    const DatasetSpec spec = random_dataset(24, 20, 30, 10);
    write_dataset(spec, tmp.path());
    const auto ids = all_ids(spec);
    const Columns exp = expected_columns(spec, ids);

    std::atomic<int> failures{0};
    std::mutex errors_mutex;
    std::vector<std::string> errors;
    std::vector<std::thread> threads;
    for(int t = 0; t < 8; t++)
        threads.emplace_back([&]() {
            try
            {
                for(int round = 0; round < 3; round++)
                {
                    TimsDataHandle h(tmp.path().string());
                    Columns c;
                    c.resize(h.no_peaks_in_frames(ids));
                    h.extract_frames(ids, c.frame.data(), c.scan.data(), c.tof.data(), c.intensity.data(),
                                     c.mz.data(), c.inv_ion_mobility.data(), c.retention_time.data());
                    if(c.tof != exp.tof || c.frame != exp.frame || c.intensity != exp.intensity)
                        failures++;
                }
            }
            catch(const std::exception& e)
            {
                failures++;
                std::lock_guard<std::mutex> lock(errors_mutex);
                errors.push_back(e.what());
            }
            catch(...)
            {
                failures++;
            }
        });
    for(auto& th : threads)
        th.join();
    EXPECT_EQ(failures.load(), 0);
    for(const auto& e : errors)
        ADD_FAILURE() << e;
}

TEST(Threading, ThreadedExtractionOnSeparateHandlesConcurrently)
{
    // The threaded extract_frames() flips the global threading mode while it
    // runs; two of them at once, on different handles, must not race on it.
    ThreadCountGuard guard;
    TempDir tmp;
    const DatasetSpec spec = random_dataset(25, 30, 30, 10);
    write_dataset(spec, tmp.path());
    ThreadingManager::get_instance().set_num_threads(3);

    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for(int t = 0; t < 4; t++)
        threads.emplace_back([&]() {
            try
            {
                TimsDataHandle h(tmp.path().string());
                for(int round = 0; round < 5; round++)
                    if(!threaded_extract_matches(h, spec, all_ids(spec)))
                        failures++;
            }
            catch(...)
            {
                failures++;
            }
        });
    for(auto& th : threads)
        th.join();
    EXPECT_EQ(failures.load(), 0);
}

TEST(Threading, ThreadedExtractionWhileThreadCountChanges)
{
    ThreadCountGuard guard;
    TempDir tmp;
    const DatasetSpec spec = random_dataset(26, 30, 30, 10);
    write_dataset(spec, tmp.path());
    TimsDataHandle h(tmp.path().string());

    std::atomic<bool> stop{false};
    std::thread changer([&]() {
        size_t n = 1;
        while(!stop.load())
        {
            ThreadingManager::get_instance().set_num_threads(n);
            n = n % 7 + 1;
            std::this_thread::yield();
        }
    });
    bool ok = true;
    for(int round = 0; round < 10; round++)
        ok = ok && threaded_extract_matches(h, spec, all_ids(spec));
    stop = true;
    changer.join();
    EXPECT_TRUE(ok);
}

TEST(ThreadingManager, ThreadCounts)
{
    ThreadCountGuard guard;
    ThreadingManager& tm = ThreadingManager::get_instance();
    EXPECT_EQ(&tm, &ThreadingManager::get_instance());
    tm.set_num_threads(1);
    EXPECT_EQ(tm.get_no_opentims_threads(), 1u);
    tm.set_num_threads(5);
    EXPECT_EQ(tm.get_no_opentims_threads(), 6u); // 20% over the requested count, for I/O
    tm.set_num_threads(10);
    EXPECT_EQ(tm.get_no_opentims_threads(), 12u);
    tm.set_num_threads(0);
    EXPECT_GE(tm.get_no_opentims_threads(), 1u) << "0 means the hardware default, never zero threads";
    tm.set_opentims_threading();
    tm.set_shared_threading();
    tm.set_converter_threading();
    EXPECT_GE(tm.get_no_opentims_threads(), 1u);
}

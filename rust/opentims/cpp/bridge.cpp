// Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
// SPDX-License-Identifier: MIT
#include "opentims_all.cpp"
#include <cstdio>
// Exceptions never cross the C ABI. Errors belong to the calling thread.
static thread_local char last_error[2048];
extern "C" {
const char* ot_error() noexcept { return last_error; }
void* ot_open(const char* path) noexcept {
    try {
        OpenSourceTof2MzConverterFactory mz;
        OpenSourceScan2ImConverterFactory im;
        return new TimsDataHandle(path, NoPressureCompensation, &mz, &im);
    } catch(const std::exception& e) { std::snprintf(last_error,sizeof(last_error),"%s",e.what()); }
      catch(...) { std::snprintf(last_error,sizeof(last_error),"unknown C++ exception"); }
    return nullptr;
}
void ot_close(void* handle) noexcept { delete static_cast<TimsDataHandle*>(handle); }
int ot_read(void* handle, uint32_t id, size_t capacity, uint32_t* scans, uint32_t* tofs, uint32_t* intensities) noexcept {
    try {
        auto& frame = static_cast<TimsDataHandle*>(handle)->get_frame(id);
        if(capacity != frame.num_peaks) throw std::runtime_error("frame capacity mismatch");
        frame.save_to_buffs(nullptr,scans,tofs,intensities,nullptr,nullptr,nullptr);
        return 0;
    } catch(const std::exception& e) { std::snprintf(last_error,sizeof(last_error),"%s",e.what()); }
      catch(...) { std::snprintf(last_error,sizeof(last_error),"unknown C++ exception"); }
    return -1;
}
}

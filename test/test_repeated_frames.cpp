#include <opentims++/converters.h>
#include <opentims++/opentims.h>
#include <opentims++/thread_mgr.h>

#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Columns
{
    std::array<std::vector<uint32_t>, 4> raw;
    std::array<std::vector<double>, 3> converted;

    Columns(size_t size, unsigned mask = 127)
    {
        for(size_t col = 0; col < raw.size(); ++col)
            if(mask & (1u << col))
                raw[col].assign(size, 0xdeadbeef);
        for(size_t col = 0; col < converted.size(); ++col)
            if(mask & (1u << (col + raw.size())))
                converted[col].assign(size, -1.0);
    }

    void check(const Columns& expected) const
    {
        for(size_t col = 0; col < raw.size(); ++col)
            if(!raw[col].empty() && raw[col] != expected.raw[col])
                throw std::runtime_error("raw column " + std::to_string(col) + " differs");
        for(size_t col = 0; col < converted.size(); ++col)
            if(!converted[col].empty() && converted[col] != expected.converted[col])
                throw std::runtime_error("converted column " + std::to_string(col) + " differs");
    }
};

template<typename T> T* data_or_null(std::vector<T>& column)
{
    return column.empty() ? nullptr : column.data();
}

void check_requests(TimsDataHandle& data, const std::array<Columns, 2>& expected, unsigned first_mask)
{
    // Interleave two frames to exercise both task ordering and parallel decoding.
    // Later requests supply the columns omitted by the first requests, followed
    // by full requests that must receive copies from both sets of buffers.
    const std::vector<uint32_t> ids{2, 1, 2, 1, 2, 1};
    std::vector<Columns> outputs;
    for(size_t ii = 0; ii < ids.size(); ++ii)
        outputs.emplace_back(data.get_frame(ids[ii]).num_peaks,
                             ii < 2 ? first_mask : ii < 4 ? (127 ^ first_mask) : 127);

    std::array<std::vector<uint32_t*>, 4> raw;
    std::array<std::vector<double*>, 3> converted;
    for(auto& output : outputs)
    {
        for(size_t col = 0; col < raw.size(); ++col)
            raw[col].push_back(data_or_null(output.raw[col]));
        for(size_t col = 0; col < converted.size(); ++col)
            converted[col].push_back(data_or_null(output.converted[col]));
    }

    data.extract_frames(ids, raw[0].data(), raw[1].data(), raw[2].data(), raw[3].data(),
                        converted[0].data(), converted[1].data(), converted[2].data());
    for(size_t ii = 0; ii < ids.size(); ++ii)
        outputs[ii].check(expected[ids[ii] - 1]);
}

} // namespace

int main(int argc, char** argv)
{
    if(argc != 2)
        return 2;
    try
    {
        setup_opensource();
        TimsDataHandle data(argv[1]);
        std::array<Columns, 2> expected{Columns(data.get_frame(1).num_peaks),
                                        Columns(data.get_frame(2).num_peaks)};
        for(size_t ii = 0; ii < expected.size(); ++ii)
        {
            auto& columns = expected[ii];
            data.get_frame(ii + 1).save_to_buffs(
                columns.raw[0].data(), columns.raw[1].data(), columns.raw[2].data(),
                columns.raw[3].data(), columns.converted[0].data(),
                columns.converted[1].data(), columns.converted[2].data());
        }
        for(size_t threads : {1, 4})
        {
            ThreadingManager::get_instance().set_num_threads(threads);
            for(unsigned mask = 0; mask < 128; ++mask)
                check_requests(data, expected, mask);
        }
        std::cout << "All 128 column splits passed with one and four threads.\n";
    }
    catch(const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

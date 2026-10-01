/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

// Opens a dataset through the installed library and checks its peak count:
//   opentims_consumer <dataset.d> <expected number of peaks>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <opentims++/opentims.h>
#include <opentims++/converters.h>

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cerr << "usage: " << argv[0] << " <dataset.d> <expected peaks>\n";
        return 2;
    }
    setup_opensource();
    TimsDataHandle h(argv[1]);
    const size_t n = h.no_peaks_total();
    std::vector<uint32_t> frames(n), tofs(n);
    std::vector<double> mzs(n);
    h.extract_frames_slice(h.min_frame_id(), h.max_frame_id() + 1, 1,
                           frames.data(), nullptr, tofs.data(), nullptr, mzs.data(), nullptr, nullptr);
    std::cout << n << " peaks in frames " << h.min_frame_id() << ".." << h.max_frame_id()
              << ", first m/z " << (n > 0 ? mzs[0] : 0.0) << "\n";
    return n == std::strtoull(argv[2], nullptr, 10) ? 0 : 1;
}

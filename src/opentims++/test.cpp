/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2021 Michał Startek and Mateusz Łącki
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License, version 3 only,
 *   as published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <cassert>
#include "opentims.cpp"
#include "tof2mz_converter.cpp"
#include "scan2inv_ion_mobility_converter.cpp"

int main(int argc, char** argv)
{
    assert(argc == 2);
    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>("/home/mist/svn/git/timsdata_scratchpad/tims2hdf5/libtimsdata.so");
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>("/home/mist/svn/git/timsdata_scratchpad/tims2hdf5/libtimsdata.so");
    TimsDataHandle TDH(argv[1]);
    for(size_t ii = TDH.min_frame_id(); ii <= TDH.max_frame_id(); ii++)
    {
        TimsFrame& frame = TDH.get_frame(ii);

        //frame.decompress();

    //    TDH.frame_desc(1).print();

        size_t s = frame.num_peaks;

        std::unique_ptr<uint32_t[]> frames = std::make_unique<uint32_t[]>(s);
        std::unique_ptr<uint32_t[]> scans  = std::make_unique<uint32_t[]>(s);
        std::unique_ptr<uint32_t[]> tofs   = std::make_unique<uint32_t[]>(s);
        std::unique_ptr<uint32_t[]> intensities  = std::make_unique<uint32_t[]>(s);
        std::unique_ptr<double[]> masses  = std::make_unique<double[]>(s);
        std::unique_ptr<double[]> inv_ion_mobilities  = std::make_unique<double[]>(s);

        frame.save_to_buffs(frames.get(), scans.get(), tofs.get(), intensities.get(), masses.get(), inv_ion_mobilities.get(), nullptr);

        for(size_t ii = 0; ii < s; ii++)
            std::cout << frames[ii] << "\t" << scans[ii] << "\t" << intensities[ii] << "\t" << masses[ii] << "\t" << inv_ion_mobilities[ii] << std::endl;
//            std::cout << frames[ii] << "\t" << scans[ii] << "\t" << tofs[ii] << "\t" << intensities[ii] << std::endl;
    }
}

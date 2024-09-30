// A small example demonstrating the basic usage of OpenTIMS in C++.
// Edit to fill in the path to your TIMS data directory (below),
// then compile by issuing the "make" command.

#include <string>
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include "../opentims++/opentims_all.cpp"


std::ofstream exc_open(const std::string& path, std::ios_base::openmode openmode = std::ios::binary | std::ios::out)
{
    std::ofstream ret;
    ret.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    ret.open(path, openmode);
    return ret;
}

int main(int argc, char** argv)
{
    if(argc != 4)
    {
        std::cerr << "Three arguments are needed: path to Bruker's libtimsdata.so, data path to extract, target dir" << std::endl;
        exit(1);
    }
    std::string bruker_binary_lib_path = argv[1];
    std::string data_path = argv[2];
    std::string out_path = argv[3];

    DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(bruker_binary_lib_path.c_str());
    DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(bruker_binary_lib_path.c_str());

    TimsDataHandle TDH(data_path);
    std::filesystem::create_directory(out_path);

    // Allocate buffers for data: instead of reallocating for every frame, we just allocate a buffer that will fit all
    // and reuse it.
    const size_t buffer_size_needed = TDH.max_peaks_in_frame(); 

    std::unique_ptr<uint32_t[]> frame_ids = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> scan_ids = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> tofs = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> intensities = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<double[]> mzs = std::make_unique<double[]>(buffer_size_needed);
    std::unique_ptr<double[]> inv_ion_mobilities = std::make_unique<double[]>(buffer_size_needed);
    std::unique_ptr<double[]> retention_times = std::make_unique<double[]>(buffer_size_needed);


    std::ofstream frame_ids_out = exc_open(out_path + "/0.bin");
    std::ofstream scan_ids_out = exc_open(out_path + "/1.bin");
    std::ofstream tofs_out = exc_open(out_path + "/2.bin");
    std::ofstream intensities_out = exc_open(out_path + "/3.bin");
    std::ofstream mzs_out = exc_open(out_path + "/4.bin");
    std::ofstream inv_ion_mobilities_out = exc_open(out_path + "/5.bin");
    std::ofstream retention_times_out = exc_open(out_path + "/6.bin");

    std::ofstream metadata = exc_open(out_path + "/schema.txt", std::ios::out);
    metadata << "uint32 frame\nuint32 scan\nuint32 tof\nuint32 intensity\nfloat64 mz\nfloat64 inv_ion_mobility\nfloat64 retention_time\n";
    metadata.close();

    


    // Obtain frames: as a map frame_id -> frame object
    std::unordered_map<uint32_t, TimsFrame>& frame_desc = TDH.get_frame_descs();

    // Iterate over frames (in order), and output a CSV-formatted data dump
    for(uint32_t idx = TDH.min_frame_id(); idx < TDH.max_frame_id(); idx++)
        // The file format in theory allows some frames to be missing - we have not seen any such files in the wild though
        if(TDH.has_frame(idx))
        {
            TimsFrame& frame = TDH.get_frame(idx);
            frame.save_to_buffs(frame_ids.get(), scan_ids.get(), tofs.get(), intensities.get(), mzs.get(), inv_ion_mobilities.get(), retention_times.get());
            for(size_t peak_id = 0; peak_id < frame.num_peaks; peak_id++)
            {
                std::cout << frame_ids[peak_id] << "\t" << scan_ids[peak_id] << "\t" << tofs[peak_id] << "\t" << intensities[peak_id] << "\t";
                std::cout << mzs[peak_id] << "\t" << inv_ion_mobilities[peak_id] << "\t";
                std::cout << retention_times[peak_id] << std::endl;
            }
        }

}

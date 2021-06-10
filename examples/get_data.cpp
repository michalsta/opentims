// A small example demonstrating the basic usage of OpenTIMS in C++.
// Edit to fill in the path to your TIMS data directory (below),
// then compile by issuing the "make" command.

#include <string>
#include <iostream>
#include <cstdlib>
#include "../opentims++/opentims_all.cpp"


int main(int argc, char** argv)
{
    /* Setup: parse args, set up paths */
    std::string data_path;
    std::string bruker_binary_lib_path;
    bool use_bruker_code = false;

    if(argc==2)
    {
        use_bruker_code = false;
        data_path = argv[1];
    }
    else if(argc==3)
    {
        use_bruker_code = true;
        bruker_binary_lib_path = argv[1];
        data_path = argv[2];
    }
    else
    {
        std::cerr << "Usage:" << std::endl;
        std::cerr << std::endl;
        std::cerr << "\t" << argv[0] << " <path to your_data.d folder containing timsTOF dataset>" << std::endl;
        std::cerr << "This will NOT use Bruker's proprietary conversion functions" << std::endl;
        std::cerr << std::endl;
        std::cerr << "or:" << std::endl;
        std::cerr << std::endl;
        std::cerr << "\t" << argv[0] << " <path to Bruker's binary timsdata.so/dll> <path to your_data.d folder containing timsTOF dataset>" << std::endl;
        std::cerr << "to use Bruker's conversion functions" << std::endl;
        std::cerr << std::endl;
        return 1;
    }

    // If we're using Bruker's proprietary conversion functions, they must be set up before opening any TimsDataHandle(s) that will use them
    if(use_bruker_code)
    {
        DefaultTof2MzConverterFactory::setAsDefault<BrukerTof2MzConverterFactory, const char*>(bruker_binary_lib_path.c_str());
        DefaultScan2InvIonMobilityConverterFactory::setAsDefault<BrukerScan2InvIonMobilityConverterFactory, const char*>(bruker_binary_lib_path.c_str());
    }

    // Open the dataset
    TimsDataHandle TDH(data_path);

    // Allocate buffers for data: instead of reallocating for every frame, we just allocate a buffer that will fit all
    // and reuse it.
    const size_t buffer_size_needed = TDH.max_peaks_in_frame(); 

    std::unique_ptr<uint32_t[]> frame_ids = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> scan_ids = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> tofs = std::make_unique<uint32_t[]>(buffer_size_needed);
    std::unique_ptr<uint32_t[]> intensities = std::make_unique<uint32_t[]>(buffer_size_needed);

    std::unique_ptr<double[]> mzs = use_bruker_code ? std::make_unique<double[]>(buffer_size_needed) : nullptr;
    std::unique_ptr<double[]> inv_ion_mobilities = use_bruker_code ? std::make_unique<double[]>(buffer_size_needed) : nullptr;

    std::unique_ptr<double[]> retention_times = std::make_unique<double[]>(buffer_size_needed);

    // Print table header
    std::cout << "\"Frame ID\"" << "\t" << "\"Scan ID\"" << "\t" << "\"Time-of-flight\"" << "\t" << "\"Intensity\"" << "\t";
    if(use_bruker_code)
        std::cout << "\"MZ\"" << "\t" << "\"Inverse of ion mobility\"" << "\t";
    std::cout << "\"Retention time\"" << std::endl;


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
                if(use_bruker_code)
                    std::cout << mzs[peak_id] << "\t" << inv_ion_mobilities[peak_id] << "\t";
                std::cout << retention_times[peak_id] << std::endl;
            }
        }

}

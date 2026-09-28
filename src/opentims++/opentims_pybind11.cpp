/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2026 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <cstdint>
#include "platform.h"
#include "opentims_all.h"

namespace py = pybind11;
using namespace pybind11::literals;

enum class ConversionMethod {
    Default,
    Bruker,
    OpenSource,
    NoConversion
};

static std::string bruker_so_path;
static bool bruker_so_initialized = false;


// Arrays passed in from Python for input: converted (copied if needed) to a
// contiguous array of the type C++ reads, so strides and dtypes cannot be misread.
template<typename T> using input_array = py::array_t<T, py::array::c_style | py::array::forcecast>;

static bool is_contiguous(const py::buffer_info& info, bool fortran_order)
{
    if(info.size == 0)
        return true; // strides of empty arrays are arbitrary
    py::ssize_t expected_stride = info.itemsize;
    for(py::ssize_t ii = 0; ii < info.ndim; ii++)
    {
        const py::ssize_t dim = fortran_order ? ii : info.ndim - 1 - ii;
        if(info.shape[dim] != 1 && info.strides[dim] != expected_stride)
            return false;
        expected_stride *= info.shape[dim];
    }
    return true;
}

// Validate a caller-provided output buffer, which C++ fills through a raw pointer:
// it must have the right dtype, be writable, contiguous (column-major for the 2D
// matrix outputs) and hold at least min_size elements. If optional, an empty
// buffer means "do not compute this column" and yields nullptr.
template<typename T> T* get_ptr(py::buffer& buf, const char* name, size_t min_size, bool optional = true, bool fortran_order = false)
{
    py::buffer_info info = buf.request();
    if(optional && info.size == 0)
        return nullptr;
    const std::string prefix = std::string("Output array '") + name + "' ";
    if(!info.item_type_is_equivalent_to<T>())
        throw py::type_error(prefix + "must have dtype " + py::str(py::dtype::of<T>()).cast<std::string>());
    if(info.readonly)
        throw py::value_error(prefix + "is read-only");
    if(!is_contiguous(info, fortran_order))
        throw py::value_error(prefix + (fortran_order ? "must be Fortran-contiguous (column-major)" : "must be contiguous") + "; pass a copy, e.g. np.ascontiguousarray()");
    if(static_cast<size_t>(info.size) < min_size)
        throw py::value_error(prefix + "is too small: holds " + std::to_string(info.size) + " elements, needs " + std::to_string(min_size));
    return static_cast<T*>(info.ptr);
}

template<typename T>
std::unique_ptr<T*[]> extract_ptrs(std::vector<py::array_t<T> >& V, size_t size)
{
    std::unique_ptr<T*[]> A = std::make_unique<T*[]>(size);
    if(V.size() == size)
        for(size_t ii = 0; ii < size; ii++)
            A[ii] = V[ii].size() == 0 ? nullptr : V[ii].mutable_data();
    return A;
}

std::tuple<
    std::vector<py::array_t<uint32_t> >,
    std::vector<py::array_t<uint32_t> >,
    std::vector<py::array_t<uint32_t> >,
    std::vector<py::array_t<uint32_t> >,
    std::vector<py::array_t<double> >,
    std::vector<py::array_t<double> >,
    std::vector<py::array_t<double> >
>
extract_separate_frames(
    TimsDataHandle& dh,
    std::vector<uint32_t> frames_to_get,
    bool get_frame_ids,
    bool get_scan_ids,
    bool get_tofs,
    bool get_intensities,
    bool get_mzs,
    bool get_inv_ion_mobilities,
    bool get_retention_times)
{
    std::vector<py::array_t<uint32_t> > frame_ids;
    std::vector<py::array_t<uint32_t> > scan_ids;
    std::vector<py::array_t<uint32_t> > tofs;
    std::vector<py::array_t<uint32_t> > intensities;
    std::vector<py::array_t<double> > mzs;
    std::vector<py::array_t<double> > inv_ion_mobilities;
    std::vector<py::array_t<double> > retention_times;

    const size_t no_frames = frames_to_get.size();

    if(get_frame_ids) frame_ids.reserve(no_frames);
    if(get_scan_ids) scan_ids.reserve(no_frames);
    if(get_tofs) tofs.reserve(no_frames);
    if(get_intensities) intensities.reserve(no_frames);
    if(get_mzs) mzs.reserve(no_frames);
    if(get_inv_ion_mobilities) inv_ion_mobilities.reserve(no_frames);
    if(get_retention_times) retention_times.reserve(no_frames);

    for(uint32_t frame_id_to_get : frames_to_get)
    {
        TimsFrame& frame = dh.get_frame(frame_id_to_get);
        const uint32_t size = frame.num_peaks;

        if(get_frame_ids) frame_ids.push_back(py::array_t<uint32_t, py::array::c_style>(size));
        if(get_scan_ids) scan_ids.push_back(py::array_t<uint32_t, py::array::c_style>(size));
        if(get_tofs) tofs.push_back(py::array_t<uint32_t, py::array::c_style>(size));
        if(get_intensities) intensities.push_back(py::array_t<uint32_t, py::array::c_style>(size));
        if(get_mzs) mzs.push_back(py::array_t<double, py::array::c_style>(size));
        if(get_inv_ion_mobilities) inv_ion_mobilities.push_back(py::array_t<double, py::array::c_style>(size));
        if(get_retention_times) retention_times.push_back(py::array_t<double, py::array::c_style>(size));
    }

    dh.extract_frames(frames_to_get,
                      extract_ptrs<uint32_t>(frame_ids, no_frames).get(),
                      extract_ptrs<uint32_t>(scan_ids, no_frames).get(),
                      extract_ptrs<uint32_t>(tofs, no_frames).get(),
                      extract_ptrs<uint32_t>(intensities, no_frames).get(),
                      extract_ptrs<double>(mzs, no_frames).get(),
                      extract_ptrs<double>(inv_ion_mobilities, no_frames).get(),
                      extract_ptrs<double>(retention_times, no_frames).get()
                      );

    return {
        frame_ids,
        scan_ids,
        tofs,
        intensities,
        mzs,
        inv_ion_mobilities,
        retention_times
    };
}

PYBIND11_MODULE(opentimspy_cpp, m) {
    py::enum_<ConversionMethod>(m, "conversion_method")
        .value("Default", ConversionMethod::Default)
        .value("Bruker", ConversionMethod::Bruker)
        .value("OpenSource", ConversionMethod::OpenSource)
        .value("NoConversion", ConversionMethod::NoConversion);

    py::enum_<pressure_compensation_strategy>(m, "pressure_compensation_strategy")
        .value("NoPressureCompensation", pressure_compensation_strategy::NoPressureCompensation)
        .value("AnalyisGlobalPressureCompensation", pressure_compensation_strategy::AnalyisGlobalPressureCompensation)
        .value("PerFramePressureCompensation", pressure_compensation_strategy::PerFramePressureCompensation)
        .value("PerFramePressureCompensationWithMissingReference", pressure_compensation_strategy::PerFramePressureCompensationWithMissingReference);

    py::class_<TimsFrame>(m, "TimsFrame")
        .def_readonly("id", &TimsFrame::id)
        .def_readonly("num_scans", &TimsFrame::num_scans)
        .def_readonly("num_peaks", &TimsFrame::num_peaks)
        .def_readonly("msms_type", &TimsFrame::msms_type)
        .def_readonly("intensity_correction", &TimsFrame::intensity_correction)
        .def_readonly("time", &TimsFrame::time)

        .def("save_to_pybuffer",
            [](TimsFrame &m, py::buffer& b)
            {
                m.save_to_matrix_buffer(get_ptr<uint32_t>(b, "result", 4 * size_t(m.num_peaks), false, true));
            }
        );

    py::class_<TimsDataHandle>(m, "TimsDataHandle")
        .def(py::init<const std::string &, pressure_compensation_strategy>())
        .def(py::init([](const std::string& path, pressure_compensation_strategy pcs, ConversionMethod cm) {
            Tof2MzConverterFactory* tof_fac = nullptr;
            Scan2InvIonMobilityConverterFactory* im_fac = nullptr;
            switch(cm) {
                case ConversionMethod::Default:
                    break; // nullptr = use global default
                case ConversionMethod::Bruker:
                    if (!bruker_so_initialized)
                        throw std::runtime_error("conversion_method.Bruker requested but Bruker bridge has not been initialized (call setup_bruker_so first)");
                    tof_fac = &BrukerTof2MzConverterFactory::instance(bruker_so_path);
                    im_fac = &BrukerScan2InvIonMobilityConverterFactory::instance(bruker_so_path);
                    break;
                case ConversionMethod::OpenSource:
                    tof_fac = &OpenSourceTof2MzConverterFactory::instance();
                    im_fac = &OpenSourceScan2ImConverterFactory::instance();
                    break;
                case ConversionMethod::NoConversion:
                    tof_fac = &ErrorTof2MzConverterFactory::instance();
                    im_fac = &ErrorScan2InvIonMobilityConverterFactory::instance();
                    break;
            }
            return new TimsDataHandle(path, pcs, tof_fac, im_fac);
        }), py::arg("path"), py::arg("pcs") = pressure_compensation_strategy::NoPressureCompensation, py::arg("conversion_method") = ConversionMethod::Default)
        .def("no_peaks_total", &TimsDataHandle::no_peaks_total)
        .def("min_frame_id", &TimsDataHandle::min_frame_id)
        .def("max_frame_id", &TimsDataHandle::max_frame_id)
        .def("get_frame", &TimsDataHandle::get_frame, py::return_value_policy::reference)
        .def("no_peaks_in_frames",
            [](TimsDataHandle& dh, const input_array<uint32_t>& frames)
            {
                return dh.no_peaks_in_frames(frames.data(), frames.size());
            })
        .def("no_peaks_in_slice", &TimsDataHandle::no_peaks_in_slice)
        .def("extract_frames",
            [](TimsDataHandle& dh, const input_array<uint32_t>& frames, py::buffer& result_b)
            {
                const size_t n = dh.no_peaks_in_frames(frames.data(), frames.size());
                dh.extract_frames(frames.data(),
                                  frames.size(),
                                  get_ptr<uint32_t>(result_b, "result", 4 * n, false, true));
            })
        .def("extract_frames",
            [](
                TimsDataHandle& dh,
                const input_array<uint32_t>& frames,
                py::buffer& frame_ids,
                py::buffer& scan_ids,
                py::buffer& tofs,
                py::buffer& intensities,
                py::buffer& mzs,
                py::buffer& inv_ion_mobilities,
                py::buffer& retention_times)
                {
                    const size_t n = dh.no_peaks_in_frames(frames.data(), frames.size());
                    dh.extract_frames(
                        frames.data(),
                        frames.size(),
                        get_ptr<uint32_t>(frame_ids, "frame", n),
                        get_ptr<uint32_t>(scan_ids, "scan", n),
                        get_ptr<uint32_t>(tofs, "tof", n),
                        get_ptr<uint32_t>(intensities, "intensity", n),
                        get_ptr<double>(mzs, "mz", n),
                        get_ptr<double>(inv_ion_mobilities, "inv_ion_mobility", n),
                        get_ptr<double>(retention_times, "retention_time", n)
                    );
                },
            py::arg("frames"),
            py::arg("frame"),
            py::arg("scan"),
            py::arg("tof"),
            py::arg("intensity"),
            py::arg("mz"),
            py::arg("inv_ion_mobility"),
            py::arg("retention_time")
        )
        .def("extract_frames_slice",
            [](TimsDataHandle& dh, size_t start, size_t end, size_t step, py::buffer& result_b)
            {
                const size_t n = dh.no_peaks_in_slice(start, end, step);
                dh.extract_frames_slice(start, end, step, get_ptr<uint32_t>(result_b, "result", 4 * n, false, true));
            })
        .def("extract_frames_slice",
            [](
            TimsDataHandle& dh,
            size_t start,
            size_t end,
            size_t step,
            py::buffer& frame_ids,
            py::buffer& scan_ids,
            py::buffer& tofs,
            py::buffer& intensities,
            py::buffer& mzs,
            py::buffer& inv_ion_mobilities,
            py::buffer& retention_times)
            {
            const size_t n = dh.no_peaks_in_slice(start, end, step);
            dh.extract_frames_slice(
                start,
                end,
                step,
                get_ptr<uint32_t>(frame_ids, "frame", n),
                get_ptr<uint32_t>(scan_ids, "scan", n),
                get_ptr<uint32_t>(tofs, "tof", n),
                get_ptr<uint32_t>(intensities, "intensity", n),
                get_ptr<double>(mzs, "mz", n),
                get_ptr<double>(inv_ion_mobilities, "inv_ion_mobility", n),
                get_ptr<double>(retention_times, "retention_time", n)
            );
        },
            py::arg("start"),
            py::arg("end"),
            py::arg("step"),
            py::arg("frame"),
            py::arg("scan"),
            py::arg("tof"),
            py::arg("intensity"),
            py::arg("mz"),
            py::arg("inv_ion_mobility"),
            py::arg("retention_time")
        )
        .def("extract_separate_frames", &extract_separate_frames)
        .def("per_frame_TIC",
            [](
                TimsDataHandle& dh,
                py::buffer& tics)
            {
                const size_t n = dh.get_frame_descs().empty() ? 0 : size_t(dh.max_frame_id()) - dh.min_frame_id() + 1;
                dh.per_frame_TIC(get_ptr<uint32_t>(tics, "tics", n, n == 0));
            }
        )
        .def("tof_to_mz",
                [](
                    TimsDataHandle& dh,
                    uint32_t frame_id,
                    const input_array<uint32_t>& arg
                )
                {
                    const size_t n = arg.size();
                    py::array_t<double> ret(n);
                    dh.tof2mz_converter->convert(frame_id, ret.mutable_data(), arg.data(), n);
                    return ret;
                }
        )
        .def("mz_to_tof",
                [](
                    TimsDataHandle& dh,
                    uint32_t frame_id,
                    const input_array<double>& arg
                )
                {
                    const size_t n = arg.size();
                    py::array_t<uint32_t> ret(n);
                    dh.tof2mz_converter->inverse_convert(frame_id, ret.mutable_data(), arg.data(), n);
                    return ret;
                }
        )
        .def("scan_to_inv_mobility",
                [](
                    TimsDataHandle& dh,
                    uint32_t frame_id,
                    const input_array<uint32_t>& arg
                )
                {
                    const size_t n = arg.size();
                    py::array_t<double> ret(n);
                    dh.scan2inv_ion_mobility_converter->convert(frame_id, ret.mutable_data(), arg.data(), n);
                    return ret;
                }
        )
        .def("inv_mobility_to_scan",
                [](
                    TimsDataHandle& dh,
                    uint32_t frame_id,
                    const input_array<double>& arg
                )
                {
                    const size_t n = arg.size();
                    py::array_t<uint32_t> ret(n);
                    dh.scan2inv_ion_mobility_converter->inverse_convert(frame_id, ret.mutable_data(), arg.data(), n);
                    return ret;
                }
        )
        ;

    m.def("setup_bruker_so", [](const std::string& path)
                                {
                                    setup_bruker(path);
                                    bruker_so_path = path;
                                    bruker_so_initialized = true;
                                });
    m.def("setup_opensource", []()
                                {
                                    setup_opensource();
                                });
    m.def("set_num_threads", [](size_t n)
                                {
                                    ThreadingManager::get_instance().set_num_threads(n);
                                });
    m.def("setup_sqlite_so", []([[maybe_unused]] const std::string& path)
                                {
#ifndef OPENTIMS_LINK_SQLITE_STATICALLY
                                    ot_sqlite::sqlite_so_handle.emplace(path);
#endif
                                });
}

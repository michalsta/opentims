/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020 Michał Startek and Mateusz Łącki
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
#include <pybind11/pybind11.h>
#include <cstdint>
#include "opentims.cpp"

namespace py = pybind11;

using namespace pybind11::literals;

PYBIND11_MODULE(opentims_cpp, m) {
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
                py::buffer_info info = b.request();
                m.save_to_matrix_buffer(static_cast<uint32_t*>(info.ptr));
            }
        );

    py::class_<TimsDataHandle>(m, "TimsDataHandle")
        .def(py::init<const std::string &>())
        .def(py::init<const std::string &, const std::string&, const std::string&>())
        .def("no_peaks_total", &TimsDataHandle::no_peaks_total)
        .def("min_frame_id", &TimsDataHandle::min_frame_id)
        .def("max_frame_id", &TimsDataHandle::max_frame_id)
        .def("get_frame", &TimsDataHandle::get_frame, py::return_value_policy::reference)
        .def("no_peaks_in_frames",
            [](TimsDataHandle& dh, py::buffer& b)
            {
                py::buffer_info info = b.request();
                return dh.no_peaks_in_frames(static_cast<uint32_t*>(info.ptr), info.size);
            })
        .def("no_peaks_in_slice", &TimsDataHandle::no_peaks_in_slice)
        .def("extract_frames",
            [](TimsDataHandle& dh, py::buffer& indexes_b, py::buffer& result_b)
            {
                py::buffer_info indexes_info = indexes_b.request();
                py::buffer_info result_info  = result_b.request();
                dh.extract_frames(static_cast<uint32_t*>(indexes_info.ptr), indexes_info.size, static_cast<uint32_t*>(result_info.ptr));
            })
        .def("extract_frames_slice",
            [](TimsDataHandle& dh, size_t start, size_t end, size_t step, py::buffer& result_b)
            {
                py::buffer_info result_info  = result_b.request();
                dh.extract_frames_slice(start, end, step, static_cast<uint32_t*>(result_info.ptr));
            });
}

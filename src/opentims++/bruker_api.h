/*
 *   OpenTIMS: a fully open-source library for opening Bruker's TimsTOF data files.
 *   Copyright (C) 2020-2024 Michał Startek and Mateusz Łącki
 *
 *   Licensed under the MIT License. See LICENCE file in the project root for details.
 */

#pragma once

typedef uint64_t tims_open_fun_t(const char *path, uint32_t recalibration);

enum pressure_compensation_strategy {
    NoPressureCompensation = 0,
    AnalyisGlobalPressureCompensation = 1,
    PerFramePressureCompensation = 2,
    PerFramePressureCompensationWithMissingReference = 3
};
typedef uint64_t tims_open_v2_fun_t(const char *path, uint32_t recalibration,
                                    pressure_compensation_strategy pres_comp_strat);

typedef uint32_t tims_convert_fun_t(uint64_t file_hndl, int64_t frame_id, const double *tofs, double *mzs, uint32_t arr_size);

typedef uint32_t tims_scan2inv_ion_mobility_fun_t(uint64_t file_hndl, int64_t frame_id, const double *tofs, double *mzs, uint32_t arr_size);

typedef uint32_t tims_get_last_error_string_fun_t(char *target, uint32_t str_length);

typedef void tims_close_fun_t(uint64_t file_hndl);

typedef void tims_set_num_threads_t(uint32_t n);

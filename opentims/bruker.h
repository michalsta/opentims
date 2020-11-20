#pragma once

typedef uint64_t tims_open_fun_t(const char *path, uint32_t recalibration);

typedef uint32_t tims_convert_fun_t(uint64_t file_hndl, int64_t frame_id, const double *tofs, double *mzs, uint32_t arr_size);

typedef uint32_t tims_scan2drift_fun_t(uint64_t file_hndl, int64_t frame_id, const double *tofs, double *mzs, uint32_t arr_size);

typedef uint32_t tims_get_last_error_string_fun_t(char *target, uint32_t str_length);

typedef void tims_close_fun_t (uint64_t file_hndl);


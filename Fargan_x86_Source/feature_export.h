#ifndef FEATURE_EXPORT_H
#define FEATURE_EXPORT_H

#include <stddef.h>

int feature_export_write_f32(const char *path, const float *features, size_t frame_count);
int feature_export_write_text(const char *path, const float *features, size_t frame_count);
int feature_export_write_c_array(const char *path, const char *array_name,
                                 const float *features, size_t frame_count);

#endif /* FEATURE_EXPORT_H */

#ifndef FRAME_EXPORT_H
#define FRAME_EXPORT_H

#include <stddef.h>
#include <stdint.h>

#include "tts_frontend.h"

int frame_export_write_text(const char *path, const TtsFrontendResult *result);
int frame_export_write_binary(const char *path, const TtsFrontendResult *result);
int frame_export_write_c_array(const char *path, const char *array_name, const TtsFrontendResult *result);

#endif /* FRAME_EXPORT_H */

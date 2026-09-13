#include <stdio.h>
#include <stdint.h>

#include "fargan_wrapper.h"
#define FARGAN_FEATURE_DIM TTS_FARGAN_FEATURE_DIM
#include "feature_export.h"

/* Raw .f32: frame_count * 20 float32 values, no header. */
int feature_export_write_f32(const char *path, const float *features, size_t frame_count)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return -1;
    }

    size_t count = frame_count * FARGAN_FEATURE_DIM;
    if (count > 0) {
        if (fwrite(features, sizeof(float), count, fp) != count) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

int feature_export_write_text(const char *path, const float *features, size_t frame_count)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    fprintf(fp, "# frame_index f0 f1 f2 ... f19\n");
    for (size_t frame = 0; frame < frame_count; frame++) {
        const float *f = features + frame * FARGAN_FEATURE_DIM;
        fprintf(fp, "%u", (unsigned)frame);
        for (size_t k = 0; k < FARGAN_FEATURE_DIM; k++) {
            fprintf(fp, " %.6f", f[k]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);
    return 0;
}

int feature_export_write_c_array(const char *path, const char *array_name,
                                 const float *features, size_t frame_count)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    size_t count = frame_count * FARGAN_FEATURE_DIM;

    fprintf(fp, "#include <stdint.h>\n\n");
    fprintf(fp, "const uint32_t %s_frame_count = %u;\n", array_name, (unsigned)frame_count);
    fprintf(fp, "const uint32_t %s_feature_dim = %u;\n", array_name, (unsigned)FARGAN_FEATURE_DIM);
    fprintf(fp, "const float %s[%u] = {\n    ", array_name, (unsigned)count);

    for (size_t i = 0; i < count; i++) {
        fprintf(fp, "%.8ff", features[i]);
        if (i + 1 < count) {
            fprintf(fp, ", ");
        }
        if ((i + 1) % 5 == 0 && i + 1 < count) {
            fprintf(fp, "\n    ");
        }
    }

    fprintf(fp, "\n};\n");
    fclose(fp);
    return 0;
}

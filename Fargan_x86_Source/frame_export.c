#include <stdio.h>
#include <stdint.h>

#include "phonemes.h"
#include "tts_frontend.h"
#include "frame_export.h"

int frame_export_write_text(const char *path, const TtsFrontendResult *result)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    fprintf(fp, "# frame_index phone_id phone_name\n");
    for (size_t i = 0; i < result->frame_count; i++) {
        uint8_t ph = result->frame_phones[i];
        fprintf(fp, "%u %u %s\n", (unsigned)i, (unsigned)ph, phoneme_name(ph));
    }

    fclose(fp);
    return 0;
}

/*
 * Little endian: magic 'TTSF', uint16 version, uint16 reserved,
 * uint32 frame_count, then frame_count phone bytes.
 */
int frame_export_write_binary(const char *path, const TtsFrontendResult *result)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        return -1;
    }

    const uint32_t magic = 0x46535454u; /* TTSF */
    const uint16_t version = 1u;
    const uint16_t reserved = 0u;
    const uint32_t frame_count = (uint32_t)result->frame_count;

    if (fwrite(&magic, sizeof(magic), 1, fp) != 1) goto fail;
    if (fwrite(&version, sizeof(version), 1, fp) != 1) goto fail;
    if (fwrite(&reserved, sizeof(reserved), 1, fp) != 1) goto fail;
    if (fwrite(&frame_count, sizeof(frame_count), 1, fp) != 1) goto fail;

    if (result->frame_count > 0) {
        if (fwrite(result->frame_phones, sizeof(uint8_t), result->frame_count, fp) != result->frame_count) {
            goto fail;
        }
    }

    fclose(fp);
    return 0;

fail:
    fclose(fp);
    return -1;
}

int frame_export_write_c_array(const char *path, const char *array_name, const TtsFrontendResult *result)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    fprintf(fp, "#include <stdint.h>\n\n");
    fprintf(fp, "const uint32_t %s_count = %u;\n", array_name, (unsigned)result->frame_count);
    fprintf(fp, "const uint8_t %s[] = {\n    ", array_name);

    for (size_t i = 0; i < result->frame_count; i++) {
        fprintf(fp, "%u", (unsigned)result->frame_phones[i]);
        if (i + 1 < result->frame_count) {
            fprintf(fp, ", ");
        }
        if ((i + 1) % 16 == 0 && i + 1 < result->frame_count) {
            fprintf(fp, "\n    ");
        }
    }

    fprintf(fp, "\n};\n");
    fclose(fp);
    return 0;
}

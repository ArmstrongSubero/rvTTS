#include <ctype.h>
#include <string.h>
#include "textnorm.h"

static void add_word(WordList *out, const char *buf, size_t len)
{
    if (out->count >= MAX_WORDS || len == 0) {
        return;
    }

    if (len >= MAX_WORD_LEN) {
        len = MAX_WORD_LEN - 1;
    }

    memcpy(out->words[out->count], buf, len);
    out->words[out->count][len] = '\0';
    out->count++;
}

static void add_literal(WordList *out, const char *s)
{
    add_word(out, s, strlen(s));
}

static void add_number_words(WordList *out, unsigned value)
{
    static const char *small[] = {
        "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "TEN"
    };

    if (value <= 10) {
        add_word(out, small[value], strlen(small[value]));
        return;
    }

    /* Anything over ten is spelled digit by digit. */
    char tmp[16];
    int len = 0;
    while (value > 0 && len < (int)sizeof(tmp)) {
        tmp[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (int i = len - 1; i >= 0; i--) {
        unsigned d = (unsigned)(tmp[i] - '0');
        add_word(out, small[d], strlen(small[d]));
    }
}

static void add_unknown_apostrophe_word(WordList *out, const char *buf, size_t len)
{
    char clean[MAX_WORD_LEN];
    size_t n = 0;

    for (size_t i = 0; i < len && n < MAX_WORD_LEN - 1; i++) {
        if (buf[i] != '\'') {
            clean[n++] = buf[i];
        }
    }

    add_word(out, clean, n);
}

static int ends_with(const char *s, const char *suffix)
{
    size_t ns = strlen(s);
    size_t nf = strlen(suffix);
    return ns >= nf && strcmp(s + ns - nf, suffix) == 0;
}

static void add_contraction_or_word(WordList *out, const char *buf, size_t len)
{
    char tmp[MAX_WORD_LEN];

    if (len == 0) return;
    if (len >= MAX_WORD_LEN) len = MAX_WORD_LEN - 1;

    memcpy(tmp, buf, len);
    tmp[len] = '\0';

    /* Contractions, both plain ASCII and the Windows smart apostrophe. */
    if (strcmp(tmp, "CAN'T") == 0) { add_literal(out, "CAN"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "WON'T") == 0) { add_literal(out, "WILL"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "DON'T") == 0) { add_literal(out, "DO"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "DOESN'T") == 0) { add_literal(out, "DOES"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "DIDN'T") == 0) { add_literal(out, "DID"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "ISN'T") == 0) { add_literal(out, "IS"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "AREN'T") == 0) { add_literal(out, "ARE"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "WASN'T") == 0) { add_literal(out, "WAS"); add_literal(out, "NOT"); return; }
    if (strcmp(tmp, "WEREN'T") == 0) { add_literal(out, "WERE"); add_literal(out, "NOT"); return; }

    if (strcmp(tmp, "I'M") == 0) { add_literal(out, "I"); add_literal(out, "AM"); return; }
    if (strcmp(tmp, "I'VE") == 0) { add_literal(out, "I"); add_literal(out, "HAVE"); return; }
    if (strcmp(tmp, "I'D") == 0) { add_literal(out, "I"); add_literal(out, "WOULD"); return; }
    if (strcmp(tmp, "I'LL") == 0) { add_literal(out, "I"); add_literal(out, "WILL"); return; }

    if (ends_with(tmp, "'LL")) {
        tmp[strlen(tmp) - 3] = '\0';
        add_literal(out, tmp);
        add_literal(out, "WILL");
        return;
    }

    if (ends_with(tmp, "'RE")) {
        tmp[strlen(tmp) - 3] = '\0';
        add_literal(out, tmp);
        add_literal(out, "ARE");
        return;
    }

    if (ends_with(tmp, "'VE")) {
        tmp[strlen(tmp) - 3] = '\0';
        add_literal(out, tmp);
        add_literal(out, "HAVE");
        return;
    }

    if (ends_with(tmp, "'D")) {
        tmp[strlen(tmp) - 2] = '\0';
        add_literal(out, tmp);
        add_literal(out, "WOULD");
        return;
    }

    if (ends_with(tmp, "'S")) {
        tmp[strlen(tmp) - 2] = '\0';
        add_literal(out, tmp);
        add_literal(out, "IS");
        return;
    }

    add_unknown_apostrophe_word(out, tmp, strlen(tmp));
}

void normalize_and_tokenize(const char *text, WordList *out)
{
    char buf[MAX_WORD_LEN];
    size_t n = 0;
    unsigned num = 0;
    int in_number = 0;

    out->count = 0;

    for (size_t i = 0;; i++) {
        unsigned char ch = (unsigned char)text[i];
        int apostrophe = (ch == 39 || ch == 0x92);
        int utf8_semicolon_pause = 0;
        int utf8_skip_char = 0;

        if (ch == 0xE2 && text[i + 1] && text[i + 2]) {
            unsigned char c1 = (unsigned char)text[i + 1];
            unsigned char c2 = (unsigned char)text[i + 2];

            if (c1 == 0x80 && (c2 == 0x98 || c2 == 0x99)) {
                /* UTF-8 curly apostrophe or single quote. */
                apostrophe = 1;
                i += 2;
            } else if (c1 == 0x80 && (c2 == 0x93 || c2 == 0x94 || c2 == 0xA6)) {
                /* Long dashes and the ellipsis end a phrase. */
                utf8_semicolon_pause = 1;
                i += 2;
            } else {
                /* Ignore the rest, curly double quotes and so on. */
                utf8_skip_char = 1;
                i += 2;
            }
        }
        if (utf8_semicolon_pause) {
            if (n > 0) {
                add_contraction_or_word(out, buf, n);
                n = 0;
            }
            if (in_number) {
                add_number_words(out, num);
                num = 0;
                in_number = 0;
            }
            add_literal(out, "<SEMI>");
        } else if (utf8_skip_char) {
            /* Decorative punctuation should not force a word break. */
        } else if (isalpha(ch) || apostrophe) {
            if (in_number) {
                add_number_words(out, num);
                num = 0;
                in_number = 0;
            }
            if (n < MAX_WORD_LEN - 1) {
                buf[n++] = apostrophe ? '\'' : (char)toupper(ch);
            }
        } else if (isdigit(ch)) {
            if (n > 0) {
                add_contraction_or_word(out, buf, n);
                n = 0;
            }
            in_number = 1;
            num = num * 10u + (unsigned)(ch - '0');
        } else {
            if (n > 0) {
                add_contraction_or_word(out, buf, n);
                n = 0;
            }
            if (in_number) {
                add_number_words(out, num);
                num = 0;
                in_number = 0;
            }

            if (ch == ',') {
                add_literal(out, "<COMMA>");
            } else if (ch == ';' || ch == ':') {
                add_literal(out, "<SEMI>");
            } else if (ch == '.' || ch == '!' || ch == '?') {
                add_literal(out, "<SENT>");
            } else if (ch == '\n' || ch == '\r') {
                add_literal(out, "<PARA>");
            }

            if (ch == '\0') {
                break;
            }
        }
    }
}

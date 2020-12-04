#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#include "s3m.h"

#define MAX_SAMPLE_SIZE 64000

#define MS_TO_OFF(ms_) ((((ms_)[2] << 8) | (ms_)[1]) * 16)

static char *strlcpy(char *dest, const char *src, size_t size) {
    strncpy(dest, src, size - 1);
    dest[size - 1] = 0;
    return dest;
}

static s3m_vinstrument_t *create_vinstr(uint8_t *u8, s3m_instrument_t *on_disk) {
    char title[S3M_TITLE_LENGTH + 1] = {0};
    memcpy(title, on_disk->title, S3M_TITLE_LENGTH);

    uint16_t sample_size = on_disk->length;
    if (sample_size > MAX_SAMPLE_SIZE) {
        fprintf(stderr, "Warning: sample %s size %hu exceeds limit of %hu.\n",
            title, sample_size, (uint16_t) MAX_SAMPLE_SIZE
        );
        sample_size = MAX_SAMPLE_SIZE;
    }

    s3m_vinstrument_t *vinstr = malloc(sizeof(s3m_vinstrument_t) + sample_size * sizeof(float));
    assert(vinstr);

    vinstr->on_disk = on_disk;
    vinstr->sample_length = sample_size;

    if (!(on_disk->flags & 4)) {
        uint8_t *rawSample = (uint8_t *) (u8 + MS_TO_OFF(on_disk->memseg));
        for (uint16_t i = 0; i < sample_size; ++i) {
            vinstr->sample[i] = (rawSample[i] / 128.f) - 1;
            assert(vinstr->sample[i] <= 1);
            assert(vinstr->sample[i] >= -1);
        }
    } else {
        uint16_t *rawSample = (uint16_t *) (u8 + MS_TO_OFF(on_disk->memseg));
        for (uint16_t i = 0; i < sample_size; ++i) {
            vinstr->sample[i] = (rawSample[i] / 65536.f) - 0.5;
            assert(vinstr->sample[i] <= 1);
            assert(vinstr->sample[i] >= -1);
        }
    }

    memcpy(vinstr->title, title, S3M_TITLE_LENGTH + 1);

    return vinstr;
}

static s3m_cell_t *read_pattern(s3m_t *s3m, uint8_t *u8) {
    s3m_cell_t *cells = calloc(S3M_NUM_ROWS_PER_PATTERN * S3M_NUM_CHANNELS, sizeof(s3m_cell_t));
    assert(cells);

    uint16_t length = *((uint16_t *) u8);

    u8 += 2;

    uint8_t previous_note[S3M_NUM_CHANNELS] = {0};
    uint8_t previous_instrument[S3M_NUM_CHANNELS] = {0};
    uint8_t previous_volume[S3M_NUM_CHANNELS] = {0};

    for (int row = 0; row < S3M_NUM_ROWS_PER_PATTERN && u8 < u8 + length;) {
        s3m_cell_t cell = {0};
        cell.raw = *u8;
        ++u8;

        if (!cell.raw) {
            ++row;
            continue;
        }

        uint8_t channel = cell.raw & (S3M_NUM_CHANNELS - 1);

        if (cell.raw & 32) {
            cell.note = *u8;
            if (cell.note == 255) cell.note = 0;
            if (cell.note) {
                previous_note[channel] = cell.note;
            }

            cell.instrument = u8[1];
            if (cell.instrument) {
                previous_instrument[channel] = cell.instrument;
                previous_volume[channel] = 0;
            }

            u8 += 2;
        }

        if ((cell.raw ^ 64) && ((cell.raw ^ 32) || !cell.note)) {
            cell.note = previous_note[channel];
        }

        if ((cell.raw ^ 64) && ((cell.raw ^ 32) || !cell.instrument)) {
            cell.instrument = previous_instrument[channel];
        }

        if (cell.raw & 64) {
            cell.volume = *u8; 
            if (cell.volume > 64) cell.volume = 0;

            if (cell.volume) {
                previous_volume[channel] = cell.volume;
            }

            ++u8;
        } else {
            cell.volume = previous_volume[channel];
        }

        if (!cell.volume && cell.instrument) {
            cell.volume = s3m->instruments[cell.instrument - 1]->on_disk->volume;
        }

        if (cell.raw & 128) {
            cell.effect = u8[0];
            cell.effect_info = u8[1];

            u8 += 2;
        }

        *s3m_get_cell(cells, channel, row) = cell;
    }

    return cells;
}

s3m_error_t s3m_open(void *buf, s3m_t *s3m) {
    s3m_assert_static_invariants();

    assert(buf);
    assert(s3m);

    s3m->hdr = buf;

    if (s3m->hdr->magic1 != S3M_HEADER_MAGIC_1
            || memcmp(s3m->hdr->magic2, S3M_HEADER_MAGIC_2, sizeof(S3M_HEADER_MAGIC_2) - 1)) {
        return S3M_E_BAD_HEADER_MAGIC;
    }

    s3m->tempo = s3m->hdr->initial_tempo;
    s3m->speed = s3m->hdr->initial_speed;

    uint8_t *u8 = buf;
    uint16_t *u16 = buf;

    s3m->orders = u8 + sizeof(s3m_header_t);

    s3m->instruments = malloc(s3m->hdr->num_instruments * sizeof(s3m_vinstrument_t *));
    assert(s3m->instruments);

    for (uint16_t i = 0; i < s3m->hdr->num_instruments; ++i) {
        uint16_t pp = u16[S3M_INPP_OFFSET(s3m) / 2 + i] * 16;

        s3m_instrument_t *on_disk = (s3m_instrument_t *) (u8 + pp);
        s3m->instruments[i] = create_vinstr(u8, on_disk);
    }

    s3m->patterns = malloc(s3m->hdr->num_patterns * sizeof(s3m_cell_t *));
    assert(s3m->patterns);

    for (uint16_t i = 0; i < s3m->hdr->num_patterns; ++i) {
        uint16_t pp = u16[S3M_PAPP_OFFSET(s3m) / 2 + i] * 16;

        if (!pp) {
            s3m->patterns[i] = NULL;
            continue;
        }

        s3m_cell_t *cells = read_pattern(s3m, u8 + pp);
        s3m->patterns[i] = cells;
    }

    return S3M_OK;
}

static const char *note_names[] = {
    "C-",
    "C#",
    "D-",
    "D#",
    "E-",
    "F-",
    "F#",
    "G-",
    "G#",
    "A-",
    "A#",
    "B-",
    "C-",
    "C#",
    "D-",
    "D#",
    "E-",
    "F-",
    "F#",
    "G-",
    "G#",
    "A-",
    "A#",
    "B-"
};

static int note_periods[] = {
    1712,
    1616,
    1524,
    1440,
    1356,
    1280,
    1208,
    1140,
    1076,
    1016,
     960,
     907
};

void s3m_note_to_text(uint8_t note, char *buf, size_t len) {
    if ((note >> 4) == 0xF) {
        strlcpy(buf, "^^ ", len);
    } else {
        snprintf(buf, len, "%-2s%d", note_names[note & 0xF], (note >> 4) + 1);
    }
}

void s3m_effect_to_text(uint8_t effect, uint8_t data, char *buf, size_t len) {
    if (!effect) {
        strlcpy(buf, "---", len);
        return;
    }

    snprintf(buf, len, "%c%02hhX", 'A' + effect - 1, data);
}

void s3m_cell_to_text(s3m_cell_t *cell, char *buf, size_t len) {
    assert(cell);

    size_t note_buf_len = 5;
    char note_buf[note_buf_len];
    s3m_note_to_text(cell->note, note_buf, note_buf_len);

    size_t effect_buf_len = 4;
    char effect_buf[effect_buf_len];
    s3m_effect_to_text(cell->effect, cell->effect_info, effect_buf, effect_buf_len);

    if (cell->raw) {
        snprintf(buf, len, 
            "\033[34;1m%s \033[36;1m%02hhu\033[32;1mv%02hhu \033[33m%s\033[0m",
            note_buf, cell->instrument, cell->volume, effect_buf
        );
    } else {
        strlcpy(buf, "\033[34;1m--- \033[36;1m--\033[32;1m -- \033[33m---\033[0m", len);
    }
}

double s3m_get_note_freq(s3m_vinstrument_t *vinstr, uint8_t note) {
    double period = 8368.0 * 16 * (note_periods[note & 0xF] >> (note >> 4)) 
                  / vinstr->on_disk->c5_freq;

    return 14317056.0 / period;
}

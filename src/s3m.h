#pragma once

#include <stdint.h>
#include <assert.h>

#define S3M_TITLE_LENGTH 28
#define S3M_FILENAME_LENGTH 12

#define S3M_HEADER_MAGIC_1 0x1A
#define S3M_HEADER_MAGIC_2 "SCRM"

#define S3M_HEADER_TYPE 16

#define S3M_INSTRUMENT_MAGIC "SCRS"

#define S3M_NUM_CHANNELS 32
#define S3M_NUM_ROWS_PER_PATTERN 64

#define S3M_SEG_TO_OFF(seg_) ((seg_) * 16)
#define S3M_INPP_OFFSET(s3m_) (sizeof(s3m_header_t) + (s3m_)->hdr->num_orders)
#define S3M_PAPP_OFFSET(s3m_) (S3M_INPP_OFFSET(s3m_) + (s3m_)->hdr->num_instruments * 2)

#define S3M_IS_EFFECT(val_, eff_) ((val_) == (eff_) - 'A' + 1)

typedef struct s3m_header {
    char title[S3M_TITLE_LENGTH];

    uint8_t magic1;

    uint8_t type;

    uint8_t unused1[2];

    uint16_t num_orders;
    uint16_t num_instruments;
    uint16_t num_patterns;

    uint16_t old_flags;
    uint16_t tracker_version;
    uint16_t format_version;

    char magic2[4];

    uint8_t global_volume;
    uint8_t initial_speed;
    uint8_t initial_tempo;
    uint8_t master_volume;

    uint8_t unused2[10];

    uint16_t special;

    uint8_t channel_settings[32];
} __attribute__((packed)) s3m_header_t;

typedef struct s3m_instrument {
    uint8_t type;

    char name[S3M_FILENAME_LENGTH];

    uint8_t memseg[3];

    uint32_t length;
    uint32_t loop_begin;
    uint32_t loop_end;

    uint8_t volume;

    uint8_t unused1;

    uint8_t pack;
    uint8_t flags;

    uint32_t c5_freq;

    uint8_t unused2[4];
    uint8_t internal_unused[8];

    char title[S3M_TITLE_LENGTH];

    char magic[4];
} __attribute__((packed)) s3m_instrument_t;

typedef struct s3m_cell {
    uint8_t raw;
    uint8_t instrument;
    uint8_t note;
    uint8_t volume;
    uint8_t effect;
    uint8_t effect_info;
} s3m_cell_t;

typedef struct s3m_vinstrument {
    s3m_instrument_t *on_disk;

    char title[S3M_TITLE_LENGTH + 1];

    size_t sample_length;
    float sample[];
} s3m_vinstrument_t;

typedef struct s3m {
    s3m_header_t *hdr;

    s3m_vinstrument_t **instruments;
    s3m_cell_t **patterns;
    uint8_t *orders;

    double tempo;
    double speed;
} s3m_t;

typedef enum s3m_error {
    S3M_OK,

    S3M_E_BAD_HEADER_MAGIC
} s3m_error_t;

typedef uint16_t s3m_parapointer_t;
typedef uint8_t s3m_order_t;

static inline void s3m_assert_static_invariants(void) {
    assert(sizeof(s3m_header_t) == 96);
    assert(sizeof(s3m_instrument_t) == 80);
}

int s3m_init_audio(void);
void s3m_play_sample(int channel, s3m_t *s3m, uint8_t instr, uint8_t note, uint8_t volume);

s3m_error_t s3m_open(void *buf, s3m_t *s3m);

void s3m_cell_to_text(s3m_cell_t *cell, char *buf, size_t len);

static inline s3m_cell_t *s3m_get_cell(s3m_cell_t *pattern, int channel, int row) {
    return pattern + row * S3M_NUM_CHANNELS + channel;
}

double s3m_get_note_freq(s3m_vinstrument_t *vinstr, uint8_t note);

static inline long s3m_tempo_to_ns(s3m_t *s3m) {
    return (long) (1e9 / (4.0 * s3m->tempo * (6.0 / s3m->speed) / 60.0));
}

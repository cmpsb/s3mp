#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <SDL_mixer.h>

#include "slopt/opt.h"

#include "s3m.h"

#define FLAG_SILENT (1 << 0)

static int BAR_COLORS[] = {
    196, 202, 208, 214, 220, 226, 190, 154, 118, 82, 46
};

static slopt_Option options[] = {
    {'w', "--wrap", SLOPT_DISALLOW_ARGUMENT},
    {0, NULL, 0}
};

static const char *path = NULL;
static int wrap = 0;

static void usage(const char *pname) {
    printf("Usage: %s FILE\n", pname);
}

static void on_option(int sw, char sname, const char *lname, const char *value, void *pl) {
    switch (sw) {
        case SLOPT_OK1:
        case SLOPT_OK2:
            switch (sname) {
                case 'w':
                    wrap = 1;
                    break;
            }
            break;

        case SLOPT_MISSING_ARGUMENT:
            fprintf(stderr, "The -%c option requires an argument.\n", sname);
            usage(pl);
            exit(1);

        case SLOPT_UNEXPECTED_ARGUMENT:
            fprintf(stderr, "The -%c does not accept an argument.\n", sname);
            usage(pl);
            exit(2);

        case SLOPT_UNKNOWN_SHORT_OPTION:
            fprintf(stderr, "Unknown switch -%c.\n", sname);
            usage(pl);
            exit(3);

        case SLOPT_UNKNOWN_LONG_OPTION:
            fprintf(stderr, "Unknown option --%s.\n", lname);
            usage(pl);
            exit(4);

        case SLOPT_DIRECT:
            if (path) {
                fprintf(stderr, "Only one file can be opened per instance.\n");
                exit(5);
            }

            path = value;
            break;
    }
}

static void play_pattern(s3m_t *s3m, uint16_t i, struct timespec *tv, int flags) {
    if (!s3m->patterns[i]) return;

    for (int r = 0; r < S3M_NUM_ROWS_PER_PATTERN; ++r) {
        if (flags ^ FLAG_SILENT) {
            printf("\n\033[3%c;1m%2d.%2d\033[0m", '1' + (i % 6), i, r);
        }

        for (int c = 0; c < 32; ++c) {
            s3m_cell_t *cell = s3m_get_cell(s3m->patterns[i], c, r);
            assert(cell);

            size_t cell_text_size = 128;
            char cell_text[cell_text_size];
            s3m_cell_to_text(cell, cell_text, cell_text_size);

            if (cell->raw && cell->instrument) {
                s3m_vinstrument_t *vinstr = s3m->instruments[cell->instrument - 1];

                uint8_t volume = cell->volume;
                if (!cell->volume) {
                    volume = vinstr->on_disk->volume;
                }

                s3m_play_sample(c, s3m, cell->instrument - 1, cell->note, volume);
            }

            if (tv->tv_nsec && S3M_IS_EFFECT(cell->effect, 'T')) {
                s3m->tempo = cell->effect_info;
                tv->tv_nsec = s3m_tempo_to_ns(s3m);
            }

            if (flags ^ FLAG_SILENT) printf(" | %s", cell_text);
        }
        if (flags ^ FLAG_SILENT) fflush(stdout);

        struct timespec ltv = *tv;
        nanosleep(&ltv, NULL);
    }
}

int main(int argc, char **argv) {
    s3m_init_audio();

    slopt_parse(argc - 1, argv + 1, options, on_option, argv[0]);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Unable to open %s. %s.\n", path, strerror(errno));
        exit(6);
    }

    struct stat file_info;
    int status = fstat(fd, &file_info);
    if (status == -1) {
        fprintf(stderr, "Unable to stat %s. %s.\n", path, strerror(errno));
        exit(7);
    }

    void *file = mmap(NULL, file_info.st_size, PROT_READ, MAP_SHARED | MAP_POPULATE, fd, 0);
    if (file == MAP_FAILED) {
        fprintf(stderr, "Unable to map %s. %s.", path, strerror(errno));
        exit(8);
    }

    s3m_t s3m;
    status = s3m_open(file, &s3m);
    assert(status == 0);

    // Disable line wrapping
    if (!wrap) {
        printf("\033[?7l");
    }

    printf("Filling note cache...\n");
    Mix_Volume(-1, 128);
    char bar[41];
    bar[40] = 0;
    for (uint16_t i = 0; i < s3m.hdr->num_patterns; ++i) {
        double progress = ((double) i) / s3m.hdr->num_patterns;
        int iprogress = (int) ceil(progress * 40);

        memset(bar, '#', iprogress);
        memset(bar + iprogress, ' ', 40 - iprogress);

        printf("\033[1K\033[1G%3d%% [\033[38;5;%dm%s\033[0m]",
            (int) ceil(progress * 100), BAR_COLORS[(int) ceil(progress * 10)], bar
        );
        fflush(stdout);

        struct timespec tv = {0};
        play_pattern(&s3m, i, &tv, FLAG_SILENT);
    }

    memset(bar, '#', 40);
    printf("\033[1K\033[1G100%% [\033[38;5;46m%s\033[0m]", bar);

    struct timespec tv = {
        .tv_sec = 0,
        .tv_nsec = s3m_tempo_to_ns(&s3m)
    };

    Mix_HaltChannel(-1);
    Mix_Volume(-1, 128);
    for (;;) {
        for (uint16_t i = 0; i < s3m.hdr->num_orders && s3m.orders[i] != 255; ++i) {
            play_pattern(&s3m, s3m.orders[i], &tv, 0);
        }
    }
}

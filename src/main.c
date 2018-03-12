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

static slopt_Option options[] = {
    {0, NULL, 0}
};

static const char *path = NULL;

static void usage(const char *pname) {
    printf("Usage: %s FILE\n");
}

static void on_option(int sw, char sname, const char *lname, const char *value, void *pl) {
    switch (sw) {
        case SLOPT_OK1:
        case SLOPT_OK2:
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

static void play_pattern(s3m_t *s3m, uint16_t i, struct timespec *tv) {
    printf("\n--------------------------------------------------\n");
    printf("PATTERN %hu\n", i);
    printf("--------------------------------------------------");
    for (int r = 0; r < S3M_NUM_ROWS_PER_PATTERN; ++r) {
        printf("\n%2d", r);
        for (int c = 0; c < 4; ++c) {
            s3m_cell_t *cell = s3m_get_cell(s3m->patterns[i], c, r);

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

            printf(" | %s", cell_text);
        }
        fflush(stdout);

        nanosleep(tv, NULL);
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
    assert(s3m_open(file, file_info.st_size, &s3m) == 0);

    printf("Filling note cache...");
    Mix_Volume(-1, 0);
    for (uint16_t i = 0; i < s3m.hdr->num_patterns; ++i) {
        struct timespec tv = {0};
        play_pattern(&s3m, i, &tv);
    }

    Mix_Volume(-1, 128);
    for (;;) {
        for (uint16_t i = 0; i < s3m.hdr->num_patterns; ++i) {
            struct timespec tv = {
                .tv_sec = 0,
                .tv_nsec = 104167000L
            };

            play_pattern(&s3m, i, &tv);
        }
    }
}

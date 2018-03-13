#include <stdint.h>
#include <math.h>
#include <errno.h>
#include <limits.h>

#include <SDL.h>
#include <SDL_mixer.h>
#include <samplerate.h>

#include "s3m.h"

#define BASE_SAMPLE_RATE 48000
#define FBASE_SAMPLE_RATE ((float) BASE_SAMPLE_RATE)

#define CHUNK_SIZE 1024

#define NUM_CHANNELS 32

static Mix_Chunk channels[NUM_CHANNELS];

static int16_t *resample_cache[100][256];

int s3m_init_audio(void) {
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        fprintf(stderr, "Unable to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    if (Mix_OpenAudio(BASE_SAMPLE_RATE, AUDIO_S16SYS, 1, CHUNK_SIZE)) {
        fprintf(stderr, "Unable to initialize the mixer: %s\n", Mix_GetError());
        return 2;
    }

    Mix_AllocateChannels(NUM_CHANNELS);

    for (int i = 0; i < NUM_CHANNELS; ++i) {
        channels[i].allocated = 0;
        channels[i].volume = 128;
    }

    for (int i = 0; i < 100; ++i) {
        for (int k = 0; k < 256; ++k) {
            resample_cache[i][k] = NULL;
        }
    }

    return 0;
}

void s3m_play_sample(int channel, s3m_t *s3m, uint8_t instr, uint8_t bnote, uint8_t volume) {
    if ((bnote >> 4) == 0xF) {
        Mix_HaltChannel(channel);
        return;
    }

    s3m_vinstrument_t *vinstr = s3m->instruments[instr];

    double note = s3m_get_note_freq(vinstr, bnote);
    double ratio = FBASE_SAMPLE_RATE / note;
    unsigned output_length = (unsigned) ceil(vinstr->sample_length * ratio);
    unsigned consv_output_length = (unsigned) floor(vinstr->sample_length * ratio);

    if (!resample_cache[instr][bnote]) {
        float *data_out = malloc(output_length * sizeof(float));

        SRC_DATA data = {
            .data_in = vinstr->sample,
            .input_frames = vinstr->sample_length,
            .data_out = data_out,
            .output_frames = output_length,
            .src_ratio = ratio
        };

        int status = src_simple(&data, SRC_SINC_BEST_QUALITY, 1);
        if (status) {
            fprintf(stderr, "Unable to convert sample %s: %s. Ratio: %g\n", vinstr->title, 
                    src_strerror(status), ratio
            );
            return;
        }

        for (unsigned i = 0; i < output_length; ++i) {
            data_out[i] /= 2;
        }

        int16_t *pcm = calloc(output_length, sizeof(int16_t));
        src_float_to_short_array(data_out, pcm, consv_output_length);

        resample_cache[instr][bnote] = pcm;

        free(data_out);
    }

    channels[channel].abuf = (uint8_t *) resample_cache[instr][bnote];
    channels[channel].alen = consv_output_length * sizeof(int16_t);
    channels[channel].volume = (uint8_t) volume * 1.5;
    Mix_PlayChannel(channel, channels + channel, 0);
}

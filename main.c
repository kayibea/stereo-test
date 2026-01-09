#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
  CHANNELS = 2,
  SAMPLE_RATE = 48000,
  AMPLITUDE = 30000,
  DURATION_SEC = 1,
  NUM_BUFFERS = 3  // left, right, dual
};

static volatile int running = 1;
static const double FREQUENCY_HZ = 440.0;
typedef enum { CHANNEL_LEFT = 0, CHANNEL_RIGHT = 1, CHANNEL_DUAL = 2 } channel_t;

typedef struct {
  int16_t *data[NUM_BUFFERS];
  int frames;
} audio_buffers_t;

static void handle_signal(int sig, siginfo_t *info, void *ctx) {
  (void)ctx;
  (void)info;
  running = 0;
  printf("\nCaught signal %d\n", sig);
}

static inline int16_t sine_sample(int index) {
  return (int16_t)(sin(2.0 * M_PI * FREQUENCY_HZ * index / SAMPLE_RATE) * AMPLITUDE);
}

static void generate_buffers(audio_buffers_t *b) {
  int frames = b->frames;

  int16_t *left = b->data[CHANNEL_LEFT];
  int16_t *right = b->data[CHANNEL_RIGHT];
  int16_t *dual = b->data[CHANNEL_DUAL];

  for (int i = 0; i < frames; i++) {
    int16_t s = sine_sample(i);
    int idx = i * CHANNELS;

    // left
    left[idx + 0] = s;
    left[idx + 1] = 0;

    // right
    right[idx + 0] = 0;
    right[idx + 1] = s;

    // dual
    dual[idx + 0] = s;
    dual[idx + 1] = s;
  }
}

static int play_buffer(snd_pcm_t *pcm, int16_t *buf, int frames) {
  int frames_written = 0;
  while (frames_written < frames) {
    ssize_t ret = snd_pcm_writei(pcm, buf + frames_written * CHANNELS, frames - frames_written);
    if (ret < 0) {
      if (ret == -EPIPE) {  // underrun
        snd_pcm_prepare(pcm);
      } else if (ret == -ESTRPIPE) {  // suspend
        while ((ret = snd_pcm_resume(pcm)) == -EAGAIN) sleep(1);
        if (ret < 0) snd_pcm_prepare(pcm);
      } else {
        fprintf(stderr, "ALSA write error: %s\n", snd_strerror(ret));
        return -1;
      }
    } else {
      frames_written += ret;
    }
  }
  return 0;
}

static int init_pcm(snd_pcm_t **pcm) {
  snd_pcm_hw_params_t *params;

  if (snd_pcm_open(pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    fprintf(stderr, "Failed to open PCM device\n");
    return -1;
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(*pcm, params);
  snd_pcm_hw_params_set_access(*pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(*pcm, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_rate(*pcm, params, SAMPLE_RATE, 0);
  snd_pcm_hw_params_set_channels(*pcm, params, CHANNELS);

  if (snd_pcm_hw_params(*pcm, params) < 0) {
    fprintf(stderr, "Failed to set PCM parameters\n");
    snd_pcm_close(*pcm);
    return -1;
  }

  return 0;
}

int main(void) {
  struct sigaction sa = {0};
  sa.sa_sigaction = handle_signal;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  snd_pcm_t *pcm;
  if (init_pcm(&pcm) < 0) return 1;

  int frames = SAMPLE_RATE * DURATION_SEC;
  audio_buffers_t buffers = {0};
  buffers.frames = frames;

  for (int i = 0; i < NUM_BUFFERS; i++) {
    buffers.data[i] = malloc(frames * CHANNELS * sizeof(int16_t));
    if (!buffers.data[i]) {
      fprintf(stderr, "Failed to allocate buffer %d: %s\n", i, strerror(errno));
      for (int j = 0; j < i; j++) free(buffers.data[j]);
      snd_pcm_close(pcm);
      return 1;
    }
  }

  generate_buffers(&buffers);

  printf("pid: %d\n(q) to quit\n(l) for left\n(r) for right\n(d) for both\n", getpid());

  while (running) {
    printf("> ");
    fflush(stdout);

    int c = getchar();
    if (c == EOF) break;
    if (isspace(c)) continue;

    c = tolower(c);

    switch (c) {
      case 'q':
        running = 0;
        break;
      case 'l':
        printf("Playing left channel...\n");
        play_buffer(pcm, buffers.data[CHANNEL_LEFT], frames);
        break;
      case 'r':
        printf("Playing right channel...\n");
        play_buffer(pcm, buffers.data[CHANNEL_RIGHT], frames);
        break;
      case 'd':
        printf("Playing both channels...\n");
        play_buffer(pcm, buffers.data[CHANNEL_DUAL], frames);
        break;
      default:
        printf("Invalid command: '%c'\n", c);
    }

    int junk;
    while ((junk = getchar()) != '\n' && junk != EOF);
  }

  for (int i = 0; i < NUM_BUFFERS; i++) free(buffers.data[i]);
  snd_pcm_close(pcm);
  return 0;
}

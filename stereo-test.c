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

#define CHANNELS 2
#define FREQ 440.0
#define DURATION_SEC 1
#define SAMPLE_RATE 48000

static volatile int running = 1;

static void handle_signal(int sig, siginfo_t *info, void *ctx) {
  (void)ctx;
  (void)info;
  running = 0;
  printf("\nCaught signal %d\n", sig);
}

static void generate_buffer(int16_t *buf, int frames, int channel) {
  for (int i = 0; i < frames; i++) {
    int16_t sample =
        (int16_t)(sin(2.0 * M_PI * FREQ * i / SAMPLE_RATE) * 30000);

    switch (channel) {
    case 0:
      buf[i * CHANNELS] = sample;
      buf[i * CHANNELS + 1] = 0;
      break;
    case 1:
      buf[i * CHANNELS] = 0;
      buf[i * CHANNELS + 1] = sample;
      break;
    default:
      buf[i * CHANNELS] = sample;
      buf[i * CHANNELS + 1] = sample;
    }
  }
}

static void play_buffer(snd_pcm_t *pcm, int16_t *buf, int frames) {
  snd_pcm_prepare(pcm);
  int ret = snd_pcm_writei(pcm, buf, frames);
  if (ret < 0) {
    fprintf(stderr, "PCM write error: %s\n", snd_strerror(ret));
    snd_pcm_prepare(pcm);
  }
  snd_pcm_drain(pcm);
}

int main(void) {
  struct sigaction sa = {0};
  sa.sa_sigaction = handle_signal;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);

  snd_pcm_t *pcm;
  snd_pcm_hw_params_t *params;

  if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
    fprintf(stderr, "Failed to open PCM device\n");
    return 1;
  }

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(pcm, params);
  snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_rate(pcm, params, SAMPLE_RATE, 0);
  snd_pcm_hw_params_set_channels(pcm, params, CHANNELS);

  if (snd_pcm_hw_params(pcm, params) < 0) {
    fprintf(stderr, "Failed to set HW params\n");
    return 1;
  }

  int frames = SAMPLE_RATE * DURATION_SEC;
  int16_t *left = malloc(frames * CHANNELS * sizeof(int16_t));
  int16_t *right = malloc(frames * CHANNELS * sizeof(int16_t));
  int16_t *dual = malloc(frames * CHANNELS * sizeof(int16_t));

  if (!left || !right || !dual) {
    fprintf(stderr, "Buffer alloc failed\n");
    fprintf(stderr, "%s (%d)\n", strerror(errno), errno);
    snd_pcm_close(pcm);
    free(left);
    free(right);
    free(dual);
    return 1;
  }

  generate_buffer(dual, frames, 2);
  generate_buffer(left, frames, 0);
  generate_buffer(right, frames, 1);

  printf("pid: %d\n", getpid());
  printf("(q) to quit\n");
  printf("(l) for left channel\n");
  printf("(r) for right channel\n");
  printf("(d) for both channels\n");

  while (running) {
    printf("> ");
    fflush(stdout);

    int c = getchar();
    if (c == EOF)
      break;
    if (isspace(c))
      continue;

    c = tolower(c);

    switch (c) {
    case 'q':
      running = 0;
      break;
    case 'l':
      printf("Left\n");
      play_buffer(pcm, left, frames);
      break;
    case 'r':
      printf("Right\n");
      play_buffer(pcm, right, frames);
      break;
    case 'd':
      printf("Dual\n");
      play_buffer(pcm, dual, frames);
      break;
    default:
      printf("Invalid command: '%c'\n", c);
    }

    int junk;
    while ((junk = getchar()) != '\n' && junk != EOF)
      ;
  }

  snd_pcm_close(pcm);
  free(left);
  free(right);
  free(dual);
  return 0;
}

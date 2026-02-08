#define _GNU_SOURCE
#include <alsa/asoundlib.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 48000
#define AMP 30000
#define FREQ 440.0
#define DUR 1
#define CHANS 2

static volatile int running = 1;

void handle_signal(int sig) {
  (void)sig;
  running = 0;
}

snd_pcm_t *init_pcm() {
  snd_pcm_t *pcm;
  if (snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) return NULL;

  int err = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, CHANS,
                               RATE, 1, 1000000);
  if (err < 0) {
    snd_pcm_close(pcm);
    return NULL;
  }
  return pcm;
}

void play(snd_pcm_t *pcm, int16_t *data, int frames, char mode) {
  int off = 0;
  int16_t frame_chunk[512 * CHANS];

  while (running && off < frames) {
    int chunk_size = (frames - off > 512) ? 512 : (frames - off);

    for (int i = 0; i < chunk_size; i++) {
      int16_t s = data[off + i];
      frame_chunk[i * 2] = (mode == 'r') ? 0 : s;
      frame_chunk[i * 2 + 1] = (mode == 'l') ? 0 : s;
    }

    snd_pcm_sframes_t ret = snd_pcm_writei(pcm, frame_chunk, chunk_size);
    if (ret < 0) {
      if (snd_pcm_recover(pcm, ret, 1) < 0) break;
    } else {
      off += ret;
    }
  }

  snd_pcm_drain(pcm);
  snd_pcm_prepare(pcm);
}

int main() {
  signal(SIGINT, handle_signal);
  snd_pcm_t *pcm = init_pcm();
  if (!pcm) return 1;

  int frames = RATE * DUR;
  int16_t *samples = malloc(frames * sizeof(int16_t));

  for (int i = 0; i < frames; i++) {
    samples[i] = (int16_t)(sin(2.0 * M_PI * FREQ * i / RATE) * AMP);
  }

  printf("Commands: [l]eft, [r]ight, [d]ual, [q]uit\n");
  while (running) {
    printf("> ");
    fflush(stdout);
    int c = getchar();
    if (c == 'q' || c == EOF) break;
    if (c == '\n') continue;

    if (strchr("lrd", c)) play(pcm, samples, frames, c);

    while (getchar() != '\n');
  }

  free(samples);
  snd_pcm_close(pcm);
  return 0;
}

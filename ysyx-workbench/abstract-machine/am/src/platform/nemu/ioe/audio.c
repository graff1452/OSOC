#include <am.h>
#include <nemu.h>
#include <klib.h>

#define AUDIO_FREQ_ADDR      (AUDIO_ADDR + 0x00)
#define AUDIO_CHANNELS_ADDR  (AUDIO_ADDR + 0x04)
#define AUDIO_SAMPLES_ADDR   (AUDIO_ADDR + 0x08)
#define AUDIO_SBUF_SIZE_ADDR (AUDIO_ADDR + 0x0c)
#define AUDIO_INIT_ADDR      (AUDIO_ADDR + 0x10)
#define AUDIO_COUNT_ADDR     (AUDIO_ADDR + 0x14)

#define SBUF_ADDR ((uint8_t *)0xa1200000)

static uint32_t sbuf_size = 0;
static uint32_t wpos = 0; // our own write position; we're the only writer

void __am_audio_init() {
}

void __am_audio_config(AM_AUDIO_CONFIG_T *cfg) {
  cfg->present = true;
  cfg->bufsize = inl(AUDIO_SBUF_SIZE_ADDR);
}

void __am_audio_ctrl(AM_AUDIO_CTRL_T *ctrl) {
  outl(AUDIO_FREQ_ADDR, ctrl->freq);
  outl(AUDIO_CHANNELS_ADDR, ctrl->channels);
  outl(AUDIO_SAMPLES_ADDR, ctrl->samples);
  outl(AUDIO_INIT_ADDR, 1);
  sbuf_size = inl(AUDIO_SBUF_SIZE_ADDR);
  wpos = 0;
}

void __am_audio_status(AM_AUDIO_STATUS_T *stat) {
  stat->count = inl(AUDIO_COUNT_ADDR);
}

void __am_audio_play(AM_AUDIO_PLAY_T *ctl) {
  uint8_t *src = (uint8_t *)ctl->buf.start;
  uint32_t len = ctl->buf.end - ctl->buf.start;
  uint32_t written = 0;
  while (written < len) {
    uint32_t free_space = sbuf_size - inl(AUDIO_COUNT_ADDR);
    if (free_space == 0) continue; // buffer full; wait for playback to drain it
    uint32_t remain = len - written;
    uint32_t chunk = (remain < free_space) ? remain : free_space;
    uint32_t until_wrap = sbuf_size - wpos;
    if (chunk > until_wrap) chunk = until_wrap; // never let one memcpy cross the wrap point
    memcpy(SBUF_ADDR + wpos, src + written, chunk);
    wpos = (wpos + chunk) % sbuf_size;
    written += chunk;
  }
}
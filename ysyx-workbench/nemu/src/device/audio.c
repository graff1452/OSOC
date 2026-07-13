/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/
#include <common.h>
#include <device/map.h>
#include <SDL2/SDL.h>

enum {
  reg_freq,
  reg_channels,
  reg_samples,
  reg_sbuf_size,
  reg_init,
  reg_count,
  nr_reg
};

static uint8_t *sbuf = NULL;
static uint32_t *audio_base = NULL;

static uint32_t sbuf_tail = 0;  // read position, managed by the playback callback
static uint32_t sbuf_count = 0; // bytes currently queued, unconsumed

static void audio_callback(void *userdata, uint8_t *stream, int len) {
  int i;
  for (i = 0; i < len && sbuf_count > 0; i++) {
    stream[i] = sbuf[sbuf_tail];
    sbuf_tail = (sbuf_tail + 1) % CONFIG_SB_SIZE;
    sbuf_count--;
  }
  for (; i < len; i++) {
    stream[i] = 0; // pad with silence once real data runs out
  }
}

static void audio_io_handler(uint32_t offset, int len, bool is_write) {
  switch (offset) {
    case reg_sbuf_size * 4:
      if (!is_write) audio_base[reg_sbuf_size] = CONFIG_SB_SIZE;
      break;
    case reg_init * 4:
      if (is_write) {
        SDL_AudioSpec s = {};
        s.format = AUDIO_S16SYS;
        s.userdata = NULL;
        s.freq = audio_base[reg_freq];
        s.channels = audio_base[reg_channels];
        s.samples = audio_base[reg_samples];
        s.callback = audio_callback;
        SDL_InitSubSystem(SDL_INIT_AUDIO);
        SDL_OpenAudio(&s, NULL);
        SDL_PauseAudio(0);
        sbuf_tail = 0;
        sbuf_count = 0;
      }
      break;
    case reg_count * 4:
      if (!is_write) audio_base[reg_count] = sbuf_count;
      break;
    default: break;
  }
}

// fires on every write into the stream buffer itself (memcpy from AM
// compiles to a series of store instructions, each one triggering this)
static void sbuf_io_handler(uint32_t offset, int len, bool is_write) {
  if (is_write) {
    sbuf_count += len;
    if (sbuf_count > CONFIG_SB_SIZE) sbuf_count = CONFIG_SB_SIZE; // defensive clamp
  }
}

void init_audio() {
  uint32_t space_size = sizeof(uint32_t) * nr_reg;
  audio_base = (uint32_t *)new_space(space_size);
#ifdef CONFIG_HAS_PORT_IO
  add_pio_map ("audio", CONFIG_AUDIO_CTL_PORT, audio_base, space_size, audio_io_handler);
#else
  add_mmio_map("audio", CONFIG_AUDIO_CTL_MMIO, audio_base, space_size, audio_io_handler);
#endif
  sbuf = (uint8_t *)new_space(CONFIG_SB_SIZE);
  add_mmio_map("audio-sbuf", CONFIG_SB_ADDR, sbuf, CONFIG_SB_SIZE, sbuf_io_handler);
}
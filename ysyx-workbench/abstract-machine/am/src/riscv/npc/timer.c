#include <am.h>
#include <npc.h>

void __am_timer_init() {
}

void __am_timer_uptime(AM_TIMER_UPTIME_T *uptime) {
  uint32_t lo = inl(RTC_ADDR);
  uint32_t hi = inl(RTC_ADDR + 4);
  uint64_t cycles = ((uint64_t)hi << 32) | lo;
  // Step 9: RTC_ADDR/RTC_ADDR+4 now answer from a real hardware mtime
  // counter (npc/.../vsrc/clint.v) instead of DPI-C's get_elapsed_us() --
  // mtime counts CYCLES, not real time, so it needs converting. Using
  // this design's own measured synthesis frequency from step 8
  // (154.661 MHz => ~154.661 cycles/us, here rounded to 155) as the
  // conversion factor, per the handout's own suggested approach: "if this
  // coefficient equals the simulation rate, we can calculate real-time
  // progression based on the progression of mtime."
  //
  // Honest caveat: this is an ESTIMATE tied to step 8's synthesis result,
  // not a guarantee of matching real wall-clock time -- Verilator
  // simulation speed and the synthesized chip's real speed are two
  // completely different things, and step 8's own frequency number
  // carries its own caveat (dominated by a flip-flop-memory synthesis
  // artifact, not real SRAM timing). Integer division, not floating
  // point, since FP support isn't guaranteed in this freestanding
  // environment.
  uptime->us = cycles / 155;
}

void __am_timer_rtc(AM_TIMER_RTC_T *rtc) {
  rtc->second = 0;
  rtc->minute = 0;
  rtc->hour   = 0;
  rtc->day    = 0;
  rtc->month  = 0;
  rtc->year   = 1900;
}

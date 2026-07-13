#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

int vsprintf(char *out, const char *fmt, va_list ap) {
  char *start = out;
  for (const char *p = fmt; *p != '\0'; p++) {
    if (*p != '%') {
      *out++ = *p;
      continue;
    }
    p++; // skip past '%', now look at the format character
    if (*p == 's') {
      char *s = va_arg(ap, char *);
      while (*s != '\0') {
        *out++ = *s++;
      }
    } else if (*p == 'd') {
      int val = va_arg(ap, int);
      char buf[12]; // enough digits for a 32-bit int plus a sign
      int i = 0;
      int neg = (val < 0);
      unsigned int uval = neg ? (unsigned int)(-(val + 1)) + 1 : (unsigned int)val;
      // (the -(val+1)+1 trick avoids overflow if val is INT_MIN, whose
      // magnitude doesn't fit in a positive int)
      if (uval == 0) {
        buf[i++] = '0';
      } else {
        while (uval != 0) {
          buf[i++] = '0' + (uval % 10);
          uval /= 10;
        }
      }
      if (neg) buf[i++] = '-';
      while (i > 0) {
        *out++ = buf[--i]; // digits were built in reverse; write them back out reversed
      }
    } else if (*p == '%') {
      *out++ = '%';
    }
  }
  *out = '\0';
  return out - start;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int printf(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(buf, fmt, ap);
  va_end(ap);
  for (int i = 0; i < ret; i++) {
    putch(buf[i]);
  }
  return ret;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  char buf[256];
  int ret = vsprintf(buf, fmt, ap);
  size_t i;
  for (i = 0; i + 1 < n && i < (size_t)ret; i++) {
    out[i] = buf[i];
  }
  if (n > 0) out[i] = '\0';
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsnprintf(out, n, fmt, ap);
  va_end(ap);
  return ret;
}

#endif
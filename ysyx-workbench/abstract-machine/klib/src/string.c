#include <klib.h>
#include <klib-macros.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  size_t len = 0;
  for (; *s != '\0'; s++) {
    len++;
  }
  return len;
}

char *strcpy(char *dst, const char *src) {
  char *ret = dst;
  while ((*dst++ = *src++) != '\0')
    ;
  return ret;
}

char *strncpy(char *dst, const char *src, size_t n) {
  char *ret = dst;
  size_t i;
  for (i = 0; i < n && src[i] != '\0'; i++) {
    dst[i] = src[i];
  }
  for (; i < n; i++) {
    dst[i] = '\0';
  }
  return ret;
}

char *strcat(char *dst, const char *src) {
  char *ret = dst;
  while (*dst != '\0')
    dst++;
  while ((*dst++ = *src++) != '\0')
    ;
  return ret;
}

int strcmp(const char *s1, const char *s2) {
  while (*s1 != '\0' && *s2 != '\0') {
    if (*s1 != *s2) {
      return (unsigned char)*s1 - (unsigned char)*s2;
    }
    s1++;
    s2++;
  }
  return (unsigned char)*s1 - (unsigned char)*s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) {
    if (s1[i] != s2[i]) {
      return (unsigned char)s1[i] - (unsigned char)s2[i];
    }
    if (s1[i] == '\0') {
      return 0;
    }
  }
  return 0;
}

void *memset(void *s, int c, size_t n) {
  char *p = (char *)s;
  for (size_t i = 0; i < n; i++) {
    p[i] = (char)c;
  }
  return s;
}

void *memmove(void *dst, const void *src, size_t n) {
  char *d = (char *)dst;
  const char *s = (const char *)src;
  if (d < s) {
    for (size_t i = 0; i < n; i++) {
      d[i] = s[i];
    }
  } else if (d > s) {
    for (size_t i = n; i > 0; i--) {
      d[i - 1] = s[i - 1];
    }
  }
  return dst;
}

void *memcpy(void *out, const void *in, size_t n) {
  char *d = (char *)out;
  const char *s = (const char *)in;
  for (size_t i = 0; i < n; i++) {
    d[i] = s[i];
  }
  return out;
}

int memcmp(const void *s1, const void *s2, size_t n) {
  const char *p1 = (const char *)s1;
  const char *p2 = (const char *)s2;
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] - p2[i];
    }
  }
  return 0;
}

#endif

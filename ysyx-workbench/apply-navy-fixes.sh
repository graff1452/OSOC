#!/bin/bash
# Patches navy-apps' vendored (gitignored) libc and pal/repo after they've
# been freshly cloned by the build system, since git never tracks their
# contents. Run this once after the first `make ISA=riscv32 -C apps/pal`
# (or any build that triggers the initial clones) on a new machine.
set -e

NAVY="$(dirname "$0")/navy-apps"

LIBC_MK="$NAVY/libs/libc/Makefile"
if [ ! -f "$LIBC_MK" ]; then
  echo "ERROR: $LIBC_MK doesn't exist yet — build something in navy-apps first" \
       "(e.g. 'make ISA=riscv32 -C tests/dummy') to trigger the libc clone."
  exit 1
fi
if grep -q "_COMPILING_NEWLIB" "$LIBC_MK"; then
  echo "libc Makefile already patched, skipping."
else
  sed -i \
    -e 's|^CFLAGS = -DNO_FLOATING_POINT -DHAVE_INITFINI_ARRAY$|CFLAGS = -DNO_FLOATING_POINT -DHAVE_INITFINI_ARRAY -D_COMPILING_NEWLIB|' \
    -e 's|^SRCS = \$(shell find src/ -name "\*.c" -o -name "\*.S" -o -name "\*.cpp")$|SRCS = $(filter-out %/posix_spawn.c %/fstat64r.c %/lseek64r.c %/open64r.c %/stat64r.c %/wcstold.c %/strtold.c %/getpass.c, $(shell find src/ -name "*.c" -o -name "*.S" -o -name "*.cpp"))|' \
    "$LIBC_MK"
  echo "Patched $LIBC_MK"
fi

PALCFG="$NAVY/apps/pal/repo/src/global/palcfg.c"
if [ ! -f "$PALCFG" ]; then
  echo "ERROR: $PALCFG doesn't exist yet — run 'make ISA=riscv32 -C apps/pal'" \
       "first to trigger the pal-navy clone."
  exit 1
fi
if grep -q 'strdup(gConfig.pszGamePath ? gConfig.pszGamePath : PAL_PREFIX)' "$PALCFG"; then
  echo "palcfg.c already patched, skipping."
else
  sed -i \
    's|if (!gConfig.pszShaderPath) gConfig.pszShaderPath = strdup(gConfig.pszGamePath);|if (!gConfig.pszShaderPath) gConfig.pszShaderPath = strdup(gConfig.pszGamePath ? gConfig.pszGamePath : PAL_PREFIX);|' \
    "$PALCFG"
  echo "Patched $PALCFG"
fi

echo "Done — rebuild pal to pick up both fixes:"
echo "  cd $NAVY && make ISA=riscv32 -C apps/pal"
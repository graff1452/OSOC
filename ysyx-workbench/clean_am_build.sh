#!/bin/bash
# Clears AM-side build caches. Use when a build looks wrong in a way that
# doesn't make sense from the source alone -- e.g. a compile error partway
# through leaves a stale/corrupted .o file that a later `make` silently
# trusts as "already built" (this has happened for real, more than once,
# in this project's own history).
#
# Usage:
#   bash clean_am_build.sh                          # just am/build + klib/build
#   bash clean_am_build.sh <program-dir>             # also that program's build/
#
# Example:
#   bash clean_am_build.sh ~/Desktop/OSOC/ysyx-workbench/fceux-am
set -e
AM_HOME="${AM_HOME:-$HOME/Desktop/OSOC/ysyx-workbench/abstract-machine}"

echo "Removing $AM_HOME/am/build ..."
rm -rf "$AM_HOME/am/build"
echo "Removing $AM_HOME/klib/build ..."
rm -rf "$AM_HOME/klib/build"

if [ -n "$1" ]; then
  echo "Removing $1/build ..."
  rm -rf "$1/build"
fi

echo "Done. Next make invocation will rebuild everything from scratch."

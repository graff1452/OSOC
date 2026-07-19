#!/bin/bash
# Rebuilds the npc/riscv32e Vminirv simulator.
#
# Run this any time main.cpp or any .v file in this directory changes --
# editing the source alone does nothing until this actually runs; the
# simulator you run against is whatever was last built.
#
# Usage:
#   bash rebuild_sim.sh          # normal incremental rebuild
#   bash rebuild_sim.sh --clean  # wipe build/ first (use if a previous
#                                # build attempt errored out partway
#                                # through, or things look inexplicably wrong)
set -e
cd "$(dirname "$0")"   # always run from this script's own directory,
                       # regardless of where it's invoked from

if [ "$1" == "--clean" ]; then
  echo "Removing previous build/ ..."
  rm -rf build
fi

verilator -cc --exe --build -Mdir build --top-module minirv --trace-fst \
  vsrc/templates.v vsrc/ifu.v vsrc/decoder.v vsrc/regfile.v vsrc/alu.v vsrc/lsu.v vsrc/minirv.v \
  csrc/main.cpp \
  -CFLAGS "$(sdl2-config --cflags)" -LDFLAGS "$(sdl2-config --libs) -lreadline -ldl"

echo ""
echo "Build OK: $(pwd)/build/Vminirv"
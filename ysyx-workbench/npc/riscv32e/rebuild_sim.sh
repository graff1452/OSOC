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
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"   # absolute path to this script's own
                                               # directory -- Verilator actually
                                               # compiles from inside build/, one
                                               # level deeper, so a bare relative
                                               # "../../" here would resolve wrong
cd "$SCRIPT_DIR"

if [ "$1" == "--clean" ]; then
  echo "Removing previous build/ ..."
  rm -rf build
fi

# itrace's disassembly needs capstone's real header, reused from NEMU's own
# already-built copy -- absolute path, see comment above for why
verilator -cc --exe --build -Mdir build --top-module minirv --trace-fst \
  vsrc/templates.v vsrc/ifu.v vsrc/decoder.v vsrc/regfile.v vsrc/alu.v vsrc/lsu.v vsrc/minirv.v \
  csrc/main.cpp \
  -CFLAGS "$(sdl2-config --cflags) -I$SCRIPT_DIR/../../nemu/tools/capstone/repo/include" \
  -LDFLAGS "$(sdl2-config --libs) -lreadline -ldl"

echo ""
echo "Build OK: $SCRIPT_DIR/build/Vminirv"
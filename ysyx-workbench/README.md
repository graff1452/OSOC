# ysyx-workbench

The official project scaffold for the ["一生一芯" (ysyx) — "One Student, One Chip"](https://ysyx.oscc.cc/)
course, originally from [OSCPU/ysyx-workbench](https://github.com/OSCPU/ysyx-workbench).
The course builds up a full chip-design toolchain incrementally: a bare-metal runtime
library, an instruction set simulator, and eventually a real RISC-V processor in RTL,
verified in simulation and (optionally) run on an FPGA.

This copy is my own personal working checkout, tracked as part of
[OSOC](https://github.com/graff1452/OSOC) — see that repo's top-level README for full
setup instructions across all my ysyx-related repos.

## What's actually in here

Most of this directory's *content* is fetched incrementally via `init.sh`, not
committed as static files — the framework is designed to grow as the course progresses.

```
ysyx-workbench/
├── Makefile              Sets student ID/name; contains a git-based work-tracking
│                          mechanism used by the course staff to verify independent
│                          progress (DO NOT modify the tracer machinery)
├── init.sh                Fetches each subproject on demand (see below)
├── abstract-machine/       A minimal hardware abstraction layer (see its own README)
└── npc/                    Where my own RTL processor gets built (see its own README)
```

## Fetching a subproject

```bash
bash init.sh <subproject-name>
```

Subprojects used so far in this repo: `abstract-machine`, `npc`, `nvboard`. Others
(`nemu`, `am-kernels`, `fceux-am`, `npc-chisel`, ...) are either not yet needed or are
tracked as separate repos of their own — see `OSOC/README.md`.

## Documentation

The authoritative, up-to-date course material lives at the
[official lecture notes](https://ysyx.oscc.cc/docs/). This README only covers what's
specific to *this* checkout.

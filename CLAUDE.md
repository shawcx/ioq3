# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

ioquake3 (ioq3): a maintained fork of the id Software Quake III Arena engine (C, CMake, SDL2). The project goal is a stable baseline that stays compatible with vanilla Q3 mods, protocol, and data. This repo (`shawcx/ioq3`) carries a few gameplay/QoL patches on top of upstream `ioquake/ioq3` (remote `upstream`).

## Build

There is no test suite or linter. CI (`.github/workflows/build.yml`) builds Linux, Windows, macOS, and Emscripten; "verify" means a clean build on the affected targets.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # or Debug
cmake --build build                                       # -> build/Release/ (or build/Debug/)
cmake --install build                                     # to CMAKE_INSTALL_PREFIX (replaces old `make copyfiles`)
emcmake cmake -S . -B build -DCMAKE_BUILD_TYPE=Release    # web build (see README.md)
```

- Linux needs `cmake` and `libsdl2-dev`. Other deps (zlib, jpeg, ogg/vorbis, opus, curl, openal-soft, SDL2) are vendored under `code/thirdparty/` and selected with `USE_INTERNAL_*` (logic in `cmake/libraries/`).
- Options are passed as `-D<NAME>=ON|OFF`; README.md lists them all. Common ones: `BUILD_CLIENT`, `BUILD_SERVER`, `BUILD_GAME_LIBRARIES`, `BUILD_GAME_QVMS`, `BUILD_RENDERER_OPENGL1/2`, `BUILD_STANDALONE`, `USE_RENDERER_DLOPEN`, `USE_HTTP`, `USE_VOIP`. For a fast engine-only build use `-DBUILD_GAME_QVMS=OFF`.
- Outputs in `build/<config>/`: `ioquake3` (client), `ioq3ded` (dedicated server), `renderer_opengl{1,2}.so`, and `baseq3/` + `missionpack/` holding native game libraries and `vm/*.qvm`.
- Running needs the original Q3 `pak0.pk3` in `baseq3/` (or `+set com_basegame <dir>` for standalone data).

## Running and debugging

From `build/<config>/`:

```sh
./ioquake3 +set developer 1 +devmap q3dm1   # client
./ioq3ded +map q3dm1                        # dedicated server
```

- `vm_game`, `vm_cgame`, `vm_ui` select how each game module loads: `0` = native library, `1` = interpreted QVM, `2` = JIT-compiled QVM (default, `qcommon/vm.c`).
- To debug mod code in gdb, use a Debug build and add `+set sv_pure 0 +set vm_game 0 +set vm_cgame 0 +set vm_ui 0`. Native modules only load when `sv_pure` is 0.

## Architecture

Engine and game sources live in `code/`; build definitions live in `cmake/` (`client.cmake`, `server.cmake`, `renderer_gl{1,2}.cmake`, `basegame.cmake`, `missionpack.cmake`, `shared_sources.cmake`). Source lists are explicit, so **when adding a `.c` file, add it to the matching list in `cmake/*.cmake`**. Game sources are listed once in `basegame.cmake`; `missionpack.cmake` reuses them and adds its own.

**Engine executables**
- `qcommon/` holds the shared core: cvars, commands, the filesystem (`files.c`, pk3/zip and `.pk3dir` support), net (`net_ip.c`, `net_chan.c`, `msg.c` with Huffman delta compression), collision model (`cm_*`), and the VM host (`vm.c` + JITs `vm_x86.c`, `vm_powerpc.c`, `vm_armv7l.c`, `vm_sparc.c`, interpreter `vm_interpreted.c`). `q_shared.[ch]` is compiled into the engine and into every game module; `PRODUCT_NAME`, `BASEGAME`, and related standalone defines live there.
- `client/` is the client (connection, demos, input, sound mixing `snd_*`, OpenAL, VoIP, HTTP downloads, AVI capture, `cl_cgame.c`/`cl_ui.c` syscall dispatch).
- `server/` is the server (`sv_game.c` handles game-module syscall dispatch, plus snapshots and client management). `botlib/` (AAS navigation, bot AI library) links into the server side.
- `sys/` is the platform entry point (`sys_main.c`) with `sys_unix.c`/`sys_win32.c`/`sys_osx.m` and console backends. `sdl/` holds SDL2 input, sound, and GL context. `null/` has stubs used by the dedicated server. `web/` holds the Emscripten shell.

**Renderers** are interchangeable behind `renderercommon/tr_public.h` (`refexport_t`/`refimport_t`). `renderergl1/` is the classic renderer. `renderergl2/` is the modern renderer (HDR, shadows, PBR-ish materials; see `docs/opengl2-readme.md`). Its GLSL in `renderergl2/glsl/*.glsl` is globbed and embedded at build time by `cmake/utils/stringify_shader.cmake`. With `USE_RENDERER_DLOPEN` the renderer is a separate library chosen by the `cl_renderer` cvar (default `opengl2`); otherwise one renderer is linked statically.

**Game modules (mod code)** are three modules that talk to the engine *only* through numbered syscalls:
- `game/` is the server-side game logic `qagame` (with bot AI `ai_*.c`).
- `cgame/` is client-side presentation and prediction.
- `q3_ui/` is the baseq3 menu UI. `ui/` is the Team Arena / missionpack script-driven UI.
- `bg_*.c` (in `game/`) holds player movement and shared item data, compiled into both qagame and cgame so prediction matches the server.
- The `missionpack` variant is the same source built with `MISSIONPACK` defined.
- `baseql` is the Quake Live parity mod (plan: `docs/quakelive-plan.md`). It has its own copy of the game sources in `code/baseql/{game,cgame,ui}`, always built with `MISSIONPACK`, via `cmake/baseql.cmake` when `-DBUILD_BASEQL=ON`. Make baseql changes there, never in the upstream `game/`, `cgame/` or `ui/` directories.

Each module is built two ways:
1. **Native shared library**: `*_syscalls.c` provides `dllEntry` + `syscall` function pointer.
2. **QVM bytecode**: compiled with the bundled LCC (`code/tools/lcc` -> `q3lcc`) and assembled by `code/tools/asm` (`q3asm`), wired up in `cmake/utils/qvm_tools.cmake`, with `*_syscalls.asm` providing the trap stubs. QVM code is ANSI C89 with no libc; `bg_lib.c` supplies the needed libc subset, and `Q3_VM` is defined.

**Syscall ABI rule:** syscall numbers are positional enums in `game/g_public.h`, `cgame/cg_public.h`, `ui/ui_public.h`, and must stay in sync with the negative `equ` numbers in the matching `*_syscalls.asm` and the engine-side dispatch switch (`sv_game.c`, `cl_cgame.c`, `cl_ui.c`). Never reorder or renumber existing entries, because that breaks every existing mod and QVM. Append new ones only. The same applies to `refexport_t`/`refimport_t`, network protocol (`com_protocol` / `com_legacyprotocol`), and `qfiles.h` file formats.

**Vendored third-party code** in `code/thirdparty/` should be updated wholesale (replace the version dir and bump its version in `misc/lib-versions.sh`, which `cmake/libraries/all.cmake` reads), not hand-edited.

## Conventions

- Engine code allocates with `Z_Malloc`/`Hunk_Alloc`, not `malloc`, and uses `Q_strncpyz`/`Com_sprintf` for strings.
- Keep vanilla Q3 behavior the default. New features go behind cvars (README.md documents the ioq3-added cvars and commands; update it when adding one).
- Game-module code must compile under both the native compiler and q3lcc, so no C99-only constructs, no system headers, and no 64-bit pointer assumptions (`intptr_t` for syscall args).
- Keep fork-specific changes small and isolated so merges from `upstream/main` stay easy. Upstream PRs: fill in the PR template, and don't put issue numbers in the PR title.

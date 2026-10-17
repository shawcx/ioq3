# Quake Live feature parity plan

Plan for bringing ioquake3 up to Quake Live (QL) feature parity as a separate
mod, `baseql`, without giving up this fork's vanilla-compatible baseline.

Written 2026-10-17 against `44378243` (in sync with `ioquake/ioq3` main, apart
from one README-only upstream commit).

## Starting point

What the tree already provides:

- **Team Arena game code** (`MISSIONPACK`, built by `cmake/missionpack.cmake`):
  Nailgun, Prox Launcher, Chaingun, runes (Scout/Guard/Doubler/Ammo Regen),
  Kamikaze, Invulnerability, 1-Flag CTF, Harvester, Overload.
- **Script-driven `.menu` HUD/UI** (`code/ui/ui_shared.c`, `code/cgame/cg_newdraw.c`).
  QL's HUD system grew out of this.
- **Engine features:** `pmove_fixed`/`pmove_msec`, `sv_fps`, HTTP downloads
  (`sv_dlURL`), Opus VoIP, `cl_mouseAccelStyle`, renderergl2 HDR and tonemapping.
- **This fork's own patches:** a low-ammo warning per weapon, demos that record
  and play back VoIP, better demo file names, `cg_gun*` without cheat protection.

Gaps: BSP v46 only (`code/qcommon/qfiles.h`), no round-based modes, no
match management, no widescreen-aware HUD.

## Ground rules

1. **Keep the QL game code in its own tree.** Copy `code/game`, `code/cgame`
   and `code/ui` into `code/baseql/{game,cgame,ui}`, and build them from a new
   `cmake/baseql.cmake` (modeled on `missionpack.cmake`, output dir `baseql/`).
   The build is off by default: `-DBUILD_BASEQL=ON`.
   - Upstream game files stay untouched, so upstream merges stay easy.
   - Cost: upstream game-code fixes have to be cherry-picked into the copy by hand.
2. **Engine changes only where unavoidable.** Add new syscalls and
   `refexport_t`/`refimport_t` entries at the end only, put each feature behind a
   cvar that defaults to vanilla behavior, and document new cvars in `README.md`.
3. **Game code stays C89 and q3lcc-compatible**: no libc, no system headers,
   `intptr_t` for syscall args. Every new `.c` file goes into the matching list
   in `cmake/*.cmake`.
4. **Clean-room.** QL is closed source. Reimplement from observed behavior and
   public data; don't port decompiled code.
5. **Get exact values from the free QL dedicated server**
   (`steamcmd +app_update 349090`). Its `.factories` files and cvar defaults
   give the real weapon, item and physics numbers.
6. **Reuse GPL-2 code where it fits:** Unlagged (lag compensation), OpenArena
   gamecode (Elimination/Domination, as a base for the round-based modes),
   Quake3e (engine fixes).
7. **Don't ship QL assets.** Use placeholder art (for example the HMG model) and
   optionally load QL pk3s for players who own QL.

## Phases

### 0. Foundation

- Scaffold `code/baseql/` and `cmake/baseql.cmake`; wire it into
  `CMakeLists.txt` behind `BUILD_BASEQL`. Build both native libraries and QVMs.
- Make every weapon, item and physics constant a cvar (`weapon_reload_*`,
  `g_damage_*`, `g_knockback_*`, `g_startingWeapons`, `g_startingHealth`,
  `g_startingArmor`). Send them to clients in a configstring so `bg_*` prediction
  matches the server.
- **Factories:** gametype presets made of a base gametype plus cvar overrides.
  Use Q3's own token format (COM_Parse-style braces), because the QVM can't use a
  JSON library. Add an offline script that converts QL `.factories` files.
- **Map pool:** a `mappool.txt` with `map|factory` entries drives map rotation.

### 1. Core gameplay

- **Physics** (`bg_pmove.c`), selectable per factory:
  - VQL: VQ3 plus ramp jumps and step jumps.
  - PQL: air control, CPM-style strafing, crouch slide, double jump.
  - Lock physics to a fixed 125 Hz step.
- **Weapons:** add the Heavy Machinegun; QL damage, reload and projectile-speed
  values; instant weapon switch; offhand grapple.
- **Items:** tiered armor (green/yellow/red) and the PQL armor-absorb rules;
  mega health respawn timer that starts on pickup; health and armor decay above
  100; QL powerup and item spawn timing.
- **Spawns:** QL-style spawn selection.
- **Lag compensation:** server-side hitscan rewind, ported Unlagged-style into
  the game module.

### 2. Game modes

Already present: FFA, Duel (Tournament), TDM, CTF, 1-Flag CTF, Harvester.

- First build a shared round framework: rounds, round timer, loadouts, no item
  pickups, round win conditions.
- Then, in order: Clan Arena → Freeze Tag → Attack & Defend → Domination →
  Red Rover / Infected → Race (checkpoint timer entities) → Instagib variants
  as factories.

### 3. Match management and server

- **Match flow:** ready-up warmup (`g_warmupReadyPercentage`), countdown,
  timeouts/pause, overtime and sudden death, mercy limit, forfeit.
- **Teams:** shuffle, lock, teamsize, duel spectator queue.
- **Voting:** expanded callvote (map plus factory, shuffle, teamsize, kick,
  timelimit), and a 3-map vote at the end of each match.
- **Admin:** `access.txt` with admin/mod/ban levels; referee commands (put,
  mute, allready, abort, lock).
- **Stats:** per-weapon accuracy, damage given and received, medals, end-of-match
  stats. Add a JSON stats/rcon feed like QL's ZeroMQ interface; this is engine
  side, behind a cvar.
- **Player ID:** QL used Steam IDs. Start with `cl_guid`; add key-based auth later.

### 4. Client and HUD

- **HUD:** widescreen-anchored `.menu` HUDs with new ownerdraws: weapon bar,
  team overlay with locations, round info, medals, item timers for spectators.
  Build on the fork's existing per-weapon low-ammo warning.
- **Visibility options:** forced enemy/team models with per-part bright colors,
  simple items, rail and LG beam styles, gun position, kick/bob/screen-damage
  toggles.
- **Feedback:** damage plums, hit beep pitched by damage, crosshair hit color,
  per-weapon crosshair, zoom sensitivity scaling, per-weapon config exec.
- **View:** Hor+ FOV for widescreen.
- **Spectator:** follow killer, freecam, player list.
- **Screens:** per-mode scoreboards, the `+acc` screen, end-of-match awards.
- **Chat:** chat beep, ignore list.

### 5. Engine and renderer

Each item is behind a cvar that defaults to vanilla behavior. These are also
candidates to send upstream; read upstream's AI/LLM contribution policy
(`8b495fdf`) first.

- **Mouse:** `m_cpi`-style sensitivity options.
- **Network defaults** for `baseql` servers: 40 Hz snapshots (`sv_fps 40`),
  higher rate limits.
- **Renderer:** picmip floor and map overbright controls, bloom, gamma and color
  correction as a post-process so it works windowed. Target renderergl1, which
  is closer to QL's look.
- **Demos:** demo browser, seek/fast-forward/rewind, freecam during playback.
  Builds on the fork's demo VoIP and demo-naming patches. Seeking needs keyframes
  in the engine.
- **Server browser:** filters for gametype, factory and tags; favorites.

### 6. Compatibility and bots (stretch)

- **QL maps:** accept QL's BSP version alongside 46. I believe QL uses IBSP v47
  with the same lump layout as v46; check that against a real QL map first.
  This is an additive `qfiles.h` change.
- **QL demos:** optional playback of protocols 73/90/91. Needs QL's entity and
  playerState layout.
- **Bots:** update botlib and the AI for the HMG, PQL movement and round-based
  modes.

## Out of scope

- Steam features: friends, achievements, Workshop, matchmaking.
- QL art, models and announcer voices (copyrighted).
- Online rankings. These could come later on top of the stats feed.

## Milestones

| # | Goal | Phases |
|---|------|--------|
| M1 | Duel, FFA, TDM and CTF play like QL | 0 + 1 |
| M2 | Competitive server features | 3 |
| M3 | Clan Arena and Freeze Tag | 2, first two modes |
| M4 | QL-style HUD and settings | 4 |
| M5 | Remaining modes, renderer and demos | 2 (rest) + 5 |
| M6 | Map/demo compatibility and bots | 6 |

## How to check each phase

There is no test suite, so every phase also has to build cleanly in CI on
Linux, Windows, macOS and Emscripten, with QVMs and native libraries both building.

- **Physics:** record strafe-jump speed curves and ramp/double-jump heights in
  real QL, then compare them numerically with `baseql`.
- **Weapons and items:** diff `baseql` cvar values against a cvar dump from the
  QL dedicated server.
- **Stability:** smoke-test a dedicated server with bots in each mode.
- **Vanilla baseline:** with `BUILD_BASEQL=OFF`, the `baseq3` and `missionpack`
  builds and their behavior must not change.

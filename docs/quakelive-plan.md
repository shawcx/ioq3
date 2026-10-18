# Quake Live feature parity plan

Plan for bringing ioquake3 up to Quake Live (QL) feature parity as a separate
mod, `baseql`, without giving up this fork's vanilla-compatible baseline.

Written 2026-10-17 against `44378243` (in sync with `ioquake/ioq3` main, apart
from one README-only upstream commit). Status, the near-term roadmap and how
the implemented parts work are in `docs/baseql-roadmap.md`.

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

Gaps: no round-based modes, no match management, no widescreen-aware HUD.

## Status

- [x] `baseql` builds as its own mod (`cmake/baseql.cmake`, `-DBUILD_BASEQL=ON`),
  native libraries and QVMs, with a CI workflow for Linux and Windows.
- [x] QL maps load (BSP v47; pulled forward from phase 6 because it enables testing).
- [x] Smoke test: `code/baseql/tools/smoketest.sh` runs `ioq3ded` with bots on a
  QL map, using only the free QL dedicated server data.
- [x] QL reference data collected (see ground rule 5).
- [x] Weapon tuning cvars, with vanilla defaults (`docs/baseql-cvars.md`): reload,
  switch times, damage, splash, projectile speed, knockback, starting weapons
  and ammo. Shared values reach clients in `CS_TUNING`.
- [x] Factories and the map pool (`docs/baseql-factories.md`), loading QL's own
  files unchanged. Phase 0 is done; health, armor and `pmove_*` physics cvars
  come with their mechanics in phase 1.
- [x] Phase 1 weapons: QL values, the shotgun pattern, nailgun cvars, and the
  Heavy Machinegun (placeholder art; bots can't use it yet).
- [x] Phase 1 items: respawn times, first powerup spawn delay, item class
  switches, starting health and armor, unlimited ammo, respawn delays.
- [x] Fixed upstream's QVM build not tracking headers (`ac188a4c`).
- [x] Phase 1 movement, the part with clear meaning: jump velocity, step
  height, acceleration, friction, swim and wade scales, auto-hop, no player clip.
- [x] Fixed the QVM interpreter rounding float-to-int casts on non-x86 hosts.
- [x] `make-ql-pak.py` builds Quake Live's HMG assets and maps into a pk3 from
  the player's own copy (`docs/baseql-ql-pak.md`).
- [x] Phase 4, first part: Quake Live's own HUD scripts run, from the player's
  Quake Live: widescreen anchoring, a font keyword, and the ownerdraws and
  flags its standard, minimal and competitive HUDs use for duels.
- [x] Quake Live's TrueType fonts in the HUD, through a vendored FreeType; the
  engine keys fonts by name and size and renders them at screen resolution.
- [ ] Next in phase 1: the rest of movement (PQL air control, step/ramp/chain
  jumps, crouch slide) and armor, once their rules can be checked against a
  source; lag compensation; spawn selection.

## Current focus: an initial playable duel

The first target is 1v1 duels between two human players on Quake Live's maps,
on a private server: no bots and no other gametypes. Done for that:

- Quake Live maps, HMG and Green Armor from the player's own Quake Live
  (`make-ql-pak.py`), with reproducible output for pure servers.
- The duel factory with Quake Live's duel values, Quake Live's duel map pool,
  and `duel.cfg` for the server.
- Duels move on to the next pool map after each match, as in Quake Live.
- Quake Live's HUDs (standard, minimal, competitive).
- `baseql.pk3` packaging, and a setup guide (`docs/baseql-duel.md`).
- Tested end to end: two clients on a pure server, warmup, match, time limit,
  and the next map from the pool.

Next for duels, by value: lag compensation (if you play over the internet),
ready-up and timeouts, forced enemy models and bright skins.

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
   (`steamcmd +login anonymous +app_update 349090`, build 1069 from June 2016,
   protocol 91). What it provides:
   - `scripts/factories.txt` in `baseq3/pak00.pk3`: all stock factories, as JSON.
   - `cvarlist` after `map campgrounds ffa`: 530 cvars, including every
     `weapon_reload_*`, `g_damage_*`, `g_splash*`, `g_knockback_*`,
     `g_velocity_*`, `g_startingAmmo_*` and `pmove_*` default.
   - `server_readme.txt`, `server.cfg`, `access.txt`, `mappool*.txt`.
   - 149 maps with AAS files, usable for testing (`pak00.pk3`).

   The server won't start without reaching Steam, even with `sv_serverType 0`.
   Run it with outbound network only, `sv_serverType 0` and `sv_master 0`, and
   no published ports. The x86 steamcmd doesn't run under Docker on Apple
   Silicon; use an x86_64 Linux host. Keep the extracted files out of the repo.
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
- **Factories:** gametype presets made of a base gametype plus cvar overrides,
  in QL's JSON format so QL's files load unchanged. `qcommon/json.h` can't build
  as a QVM (it needs `stdio.h`) and doesn't report errors, so baseql has its own
  small reader (`g_json.c`).
- **Map pool:** a `mappool.txt` with `map|factory` entries drives map rotation.
- Still to decide (phase 2): whether baseql's `g_gametype` numbers should match
  QL's (QL: 0 ffa, 1 duel, 2 race, 3 tdm, 4 ca, 5 ctf, ...), which external
  tools and server browsers read from serverinfo.

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
- QL maps already place the entities these need; baseql currently logs
  "doesn't have a spawn function" for `team_dom_point` (Domination),
  `race_point` (Race), `ammo_pack` (`g_ammoPack`) and `advertisement` (stub it).

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

- **QL maps:** done. QL maps are IBSP v47: the 17 lumps of v46 plus one
  trailing lump (multiples of 128 bytes, apparently advertisement data), checked
  against all 149 stock maps. The engine accepts v47 and ignores the extra lump.
  Remaining: QL's `scripts/arenas.txt` (13 KB) exceeds the game's 8 KB limit.
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
- **Client prediction:** run `ioq3ded` and the client on one machine, connected
  over UDP rather than loopback, with `sv_packetdelay 100` and `cl_packetdelay
  100`. Loopback ignores the packet delay, and with no latency the client replays
  almost no commands, so prediction bugs don't show. Compare predicted events
  (`cg_debugEvents 1`) against the server's rate, and watch `cg_showmiss 1`
  for movement errors. "changed predicted event" doesn't catch extra predicted
  events. Checked this way: a cgame that ignored `CS_TUNING` updates predicted
  14 shots where the server fired 8; the real cgame predicted 8.
- **Stability:** `code/baseql/tools/smoketest.sh <build/Release> <qlds dir>`
  starts `ioq3ded` on a QL map with three bots and checks that the QVM, AAS and
  bots load, someone scores a kill, and nothing crashes. Extend it per mode.
- **Vanilla baseline:** with `BUILD_BASEQL=OFF`, the `baseq3` and `missionpack`
  builds and their behavior must not change.

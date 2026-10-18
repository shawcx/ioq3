# baseql roadmap and design

Where baseql stands, what comes next, and how its parts work and why. The
long-range phase plan is `docs/quakelive-plan.md`. The user guides are
`docs/baseql-duel.md` (setup), `docs/baseql-cvars.md`,
`docs/baseql-factories.md` and `docs/baseql-ql-pak.md`.

Updated 2026-10-18 at `279ddbae`, branch `baseql` (not pushed).

## Goals

- **Now:** 1v1 duels between two human players on Quake Live's maps, on a
  private Linux server. No bots and no other gametypes; anything that doesn't
  serve that waits.
- **Later:** Quake Live feature parity as a separate mod, phase by phase
  (`docs/quakelive-plan.md`), without changing vanilla `baseq3`/`missionpack`.

Constraints that shape every change (the plan's ground rules, in short):

- Game code lives in `code/baseql/{game,cgame,ui}`; upstream's `game/`,
  `cgame/` and `ui/` stay untouched so upstream merges stay easy.
- Engine changes only where unavoidable. Syscalls, `refexport_t` and file
  formats are only ever appended to.
- Game code is C89 and builds with q3lcc: no libc, no system headers.
- Clean-room: values come from the free Quake Live dedicated server's data and
  observed behavior, never from decompiled code.
- No Quake Live assets in the repo. Each player builds `quakelive.pk3` from
  their own copy, and servers keep `sv_allowDownload 0` so it's never sent.

## Status

| Area | Done | Commit |
|---|---|---|
| Build | `baseql` as its own mod, native and QVM, CI on Linux and Windows | `e8e7a732` |
| Maps | Quake Live's BSP version 47 loads | `7668e120` |
| Testing | Smoke test with bots on a Quake Live map | `06f50024` |
| Tuning | Weapon cvars with Quake Live's names, sent to clients in `CS_TUNING` | `09659fa1` |
| Factories | Quake Live's JSON factories, `map <map> <factory>`, the map pool | `cb3b4fb8`, `86fe1bec` |
| Weapons | Quake Live's values, shotgun pattern, nailgun, Heavy Machinegun | `fe95b23e`, `bba81fad` |
| Items | Respawn times, powerup delay, item switches, starting health and armor | `17635945` |
| Movement | Jump velocity, step height, acceleration, friction, water, auto-hop | `9990b9cb` |
| Engine fixes | QVM header dependencies; interpreter float-to-int casts truncate | `ac188a4c`, `5b4594d7` |
| Content | `make-ql-pak.py`: maps, HMG, Green Armor, reproducible output | `0cba1f9a`, `0d8d0d25` |
| Duels | Duel factory, duel map pool, `duel.cfg`, next map after each duel, `baseql.pk3` | `5f0b4bbd` |
| HUD | Quake Live's standard, minimal and competitive HUDs | `126b4cb9` |
| Fonts | Quake Live's TrueType fonts, through a vendored FreeType | `50f03b54`, `7ae2a63e`, `17f24df5` |
| Lag compensation | Hitscan weapons rewind the other players to what the shooter saw | `5c19fc1b` |
| Match flow | Ready-up, timeouts, server pauses | `5c19fc1b` |
| Hit feedback | Damage plums, hit beeps pitched by damage | `5c19fc1b` |
| Forced models | Enemy and team models, Quake Live's bright skins with per-part colors | `279ddbae` |
| Map vote | End-of-match vote between three pool maps | `279ddbae` |
| Scoreboard | Quake 3's scoreboard where the HUD files have none; Quake Live shader keywords ioquake3 rejects are stripped | `279ddbae` |

Tested end to end on one machine: a pure server, two clients, warmup, a match
to the time limit, and the next pool map. The HUDs were checked with
screenshots at 1280×720; the fonts at 640×480, 1280×720 and 2560×1440, on both
renderers. Lag compensation, ready-up, timeouts, pauses, plums and hit beeps
were checked as described under Testing.

## Roadmap

### Now: get the new build to both players

The packages already handed out predate the HUD, its fonts, lag compensation,
ready-up and timeouts, hit feedback, forced models and the map vote.

1. Each player rebuilds `quakelive.pk3`; it now carries the HUD scripts,
   Quake Live's fonts, hit beeps and bright skins, and shaders without the
   keywords that made some surfaces invisible.
2. Rebuild the Linux binaries, now with `-DUSE_FREETYPE=ON`, and `baseql.pk3`
   (see Deployment), and copy the server's `baseql.pk3` to both players.
3. Restart the server.
4. Play a few duels and check the "Not verified yet" list below.

### Next for duels, in order

Each of these waits on something only playing or measuring can give:

1. **VQL step and chain jumps.** The QL server's defaults name them but not
   what they do: `pmove_StepJump 1` with `pmove_StepJumpVelocity 48`,
   `pmove_ChainJump 1` with `pmove_ChainJumpVelocity 110`, and
   `pmove_JumpTimeDeltaMin 100`, `pmove_JumpVelocityMax 700`,
   `pmove_JumpVelocityScaleAdd 0.4`, `pmove_JumpVelocityTimeThreshold 500`,
   `pmove_JumpVelocityTimeThresholdOffset 0.6`, `pmove_CrouchStepJump 1`,
   `pmove_AirSteps 1`, `pmove_AirStepFriction 0.03`. Ramp jumps are off in
   VQL (`pmove_RampJump 0`). Implementing them needs measurements from the
   Quake Live client (Windows), such as jump heights off a step and after
   landing at different intervals.
2. **Armor:** the Green Armor's amount isn't in the QL data (its bot data has
   only the respawn time). In the QL client, picking it up with no armor
   shows the amount; taking a hit of known damage shows the absorb rule.
3. **HUD polish:** play-testing will show what's missing; so far the
   scoreboard was (now Quake 3's). Quake Live's own scoreboard menus and its
   end-of-match vote menu are the next step, see Later.
4. **Lag compensation tuning,** once the friend's ping is known (see Lag
   compensation below): `status` on the server shows it.

### Later

The plan's phases, with what's left:

- **Phase 1, core gameplay:** PQL movement (air control, crouch slide, double
  jump), tiered armor, health and armor rules, spawn selection, offhand
  grapple.
- **Phase 2, game modes:** a round framework, then Clan Arena, Freeze Tag,
  Attack & Defend, Domination, Red Rover, Race. Quake Live's maps already have
  their entities (`team_dom_point`, `race_point`, `ammo_pack`).
- **Phase 3, match management:** voting, admin and referee commands, stats and
  a stats feed, overtime, mercy limit, forfeit. Ready-up and timeouts are done.
- **Phase 4, client and HUD:** the rest: Quake Live's scoreboard menus
  (`ingame_scoreboard_*.menu`, `end_scoreboard_*.menu`, which include its
  vote menu with the `CG_VOTE*` ownerdraws), weapon bar, team overlay, medals,
  visibility options, crosshair hit color, spectator tools, Hor+ FOV.
- **Phase 5, engine:** mouse options, renderer controls, demo seeking, server
  browser filters.
- **Phase 6, compatibility and bots:** Quake Live demos; bots that know the HMG
  and new movement.

## Design

### Source layout and build

- `cmake/baseql.cmake` (`-DBUILD_BASEQL=ON`) builds cgame, qagame and ui from
  `code/baseql`, always with `MISSIONPACK`, as native libraries and QVMs.
- `code/baseql/data/` holds data files: `scripts/factories.txt`,
  `mappool.txt`, `mappool_duel.txt`, `duel.cfg` and `ui/menudef.h`. The build
  copies them next to the modules and packs them with the QVMs into
  `baseql.pk3`. Pure servers need the pk3, since pure clients only load game
  code from pk3s.
- baseql runs on top of Team Arena (`fs_basegame missionpack`), whose menus
  and art it uses, so players need Team Arena's `pak0`–`pak3`.

Engine changes, each small and needed:

| Change | Files | Why |
|---|---|---|
| Accept BSP version 47 | `qcommon/qfiles.h`, `qcommon/cm_load.c`, both `tr_bsp.c` | Quake Live's maps are v46 plus one trailing lump, which is ignored |
| Truncate float-to-int casts in the QVM interpreter | `qcommon/vm_interpreted.c` | It rounded on non-x86 hosts, which broke `bg_lib`'s `%f` and could split server and client values |
| Pass `map`'s second argument as `g_factory` | `server/sv_ccmds.c` | `map <map> <factory>`, as in Quake Live |
| Track QVM header dependencies | `cmake/utils/qvm_tools.cmake`, `qvm_depfile.cmake` | Header edits left stale QVMs |
| Fonts known by file name and size, rendered at the screen's resolution | `renderercommon/tr_font.c` | Quake Live's HUD uses three TrueType faces at the same sizes |
| Vendored FreeType 2.14.3 (`USE_INTERNAL_FREETYPE`) | `code/thirdparty/freetype-2.14.3`, `cmake/libraries/freetype.cmake`, `misc/` | The macOS build is universal and Homebrew's FreeType isn't; players need no library |

### Tuning cvars and `CS_TUNING`

Weapon, item and movement constants are cvars with Quake Live's names and
defaults (`docs/baseql-cvars.md`). The ones client prediction depends on
(`weapon_reload_*`, `pmove_*`) reach clients in configstring 26, `CS_TUNING`:
the game sends the cvars' own strings, and both game and cgame apply them with
`BG_SetTuning` (`game/bg_tuning.c`). Sending strings rather than formatted
numbers means both sides parse identical text; a value that isn't a number
sends the default so it can't break the positional format.

### Factories and the map pool

Factories are Quake Live's JSON format, so Quake Live's own files load
unchanged (`docs/baseql-factories.md`). `qcommon/json.h` can't build as a QVM,
so `game/g_json.c` is a small reader that reports errors with line numbers.
On each full map load the game resets the cvars the previous factory set
(tracked in `g_factoryApplied`), then applies the selected one; settings never
leak between factories. The map pool (`sv_mapPoolFile`, `map|factory` lines)
sets `nextmap` to a random other entry on every map load.

### Duel flow

`duel.cfg` selects the duel factory and `mappool_duel.txt` (Quake Live's 26
duel maps), a 10 second warmup once both players have typed `readyup`, three
60 second timeouts per player, `bot_enable 0` and `sv_fps 40`. At the end of a
duel, players vote for the next map (see End-of-match vote); `ExitLevel` sends
the loser back to the queue as vanilla does, then goes on to `nextmap` instead
of restarting the same map (`game/g_main.c`). Without a map pool, there's no
vote and `nextmap` restarts the map, so vanilla behavior is unchanged.

### Weapons and items

- **Heavy Machinegun:** weapon 14, as in Quake Live, so `g_startingWeapons` bit
  8192 and Quake Live's `weapon_hmg`/`ammo_hmg` work. Its art comes from
  `quakelive.pk3`; without it, the cgame falls back to the machinegun's. Its
  spread is the machinegun's (Quake Live's isn't known). Bots can't use it:
  botlib's weapon data stops at weapon 13.
- **Shotgun:** Quake Live's fixed 20-pellet ring pattern in `BG_ShotgunPellet`,
  shared by server and cgame. Ring radii are an approximation.
- **Green Armor** (`item_armor_jacket`): 25 armor, respawning in 25 seconds.
  The respawn time is in Quake Live's bot data; the 25 armor is a guess (Quake
  2's jacket armor, whose classname Quake Live reused).
- Max health is `g_startingHealth` scaled by handicap (`G_MaxHealth` in
  `game/g_client.c`).

### Quake Live content: `make-ql-pak.py`

`code/baseql/tools/make-ql-pak.py` builds `quakelive.pk3` from the player's
own Quake Live, Quake III Arena and Team Arena (`docs/baseql-ql-pak.md`).

- It reads each map's BSP, the MD3s it uses and Quake Live's shader scripts to
  find every texture, model and sound needed, and copies Quake Live's shader
  definitions verbatim into `scripts/quakelive.shader`.
- Files the retail Quake 3 or Team Arena paks already have are left out, so
  their maps look the same.
- **Reproducible:** it compares only against the retail paks, writes entries
  in sorted order with a fixed timestamp, so two players with the same data
  build byte-identical pk3s, which pure servers require.
- MD3 surfaces over ioquake3's 999-vertex or 1999-triangle limits are split.
- Arena info is split into files under the game's 8 KB limit.
- The HUD scripts (`ui/hud*.txt`, `hud.menu`, `min_hud.menu`,
  `comp_hud.menu`) replace Team Arena's, along with the images they name and
  everything under `ui/assets/hud/`.

### HUD

Quake Live's HUD is Team Arena's `.menu` system extended, so baseql runs
Quake Live's own scripts rather than recreating them.

- **Definitions:** `code/baseql/data/ui/menudef.h` is Team Arena's file plus
  Quake Live's names. The scripts only use names, so baseql chooses the
  numbers: ownerdraws 100–123 and flags from `0x00100000` up. All baseql code
  includes this copy instead of the repo-root `ui/menudef.h`.
- **Widescreen:** a menu's `widescreen WIDESCREEN_LEFT|CENTER|RIGHT` keeps its
  proportions on wide screens, pinned to an edge or centered. `Menu_Paint`
  sets `DC->widescreen` while it paints that menu, and `CG_AdjustFrom640`
  scales uniformly by the height and offsets x to match.
- **Fonts:** Quake Live ships TrueType fonts, which `make-ql-pak.py` packs.
  An item's `font` picks the face: `FONT_DEFAULT` is Handel Gothic,
  `FONT_SANS` Noto Sans and `FONT_MONO` Droid Sans Mono (`cg_fontFiles` in
  `cgame/cg_main.c`). The asset block's `font`, `smallFont` and `bigFont` give
  the three sizes; the scale picks one, as in Team Arena. `Menu_Paint` sets
  `DC->font` while it paints each item, and `CG_TextFont` returns that face's
  font for the scale. `FONT_DEFAULT` is registered with the HUD's assets; the
  other faces when an item first asks for them (`DC->registerItemFont`).
- **Engine fonts:** with `USE_FREETYPE`, `RE_RegisterFont` renders a font
  file that exists and keys it by name and size, so several faces can share a
  size; names that aren't files (Team Arena's `fonts/font`) still load
  `fonts/fontImage_<size>.dat`. Glyphs render at the screen's resolution (the
  scale of 480 lines to the screen's height, up to 4), on pages of 256 to 1024
  pixels, so text stays sharp; `glyphScale` keeps the layout the same.
  `r_saveFontData` still writes 72 dpi fonts under the old names.
- **Without FreeType** or the TrueType files, `CG_RegisterFont` falls back to
  Team Arena's font at the nearest pre-rendered size; the engine warns once
  per missing size.
- **Ownerdraws** (`cgame/cg_newdraw.c`): health and armor bars for 100 and 200
  (the item's background image, cropped to the value), team- and armor-tier
  colored icons, the level timer, overtime, first and second place rows,
  the gametype icon, a kill feed (`CG_AddObituary`, 4 lines, 5 seconds) and a
  chat window (`CG_AddToNewChat`, 8 lines, 8 seconds). Ownerdraws for what
  baseql lacks (rounds, race, team player counts, owned flags, keys) are
  stubs that draw nothing.
- **Flags:** first place or not, red or blue in first place, on red or blue,
  chat visible. Domination, message, notice and players-remaining flags are
  always false for now.
- **Background:** ownerdraws that paint their item's background themselves
  (the bars and colorized icons) tell `Window_Paint` to skip it
  (`OwnerDraw_PaintsBackground` in `ui/ui_shared.c`).
- **Text placement:** Quake Live anchors aligned text at the rect's left edge:
  `ITEM_ALIGN_CENTER` centers on `rect.x`, `ITEM_ALIGN_RIGHT` ends there. The
  new ownerdraws and the place scores put the baseline at `rect.y`; health,
  armor and ammo keep Team Arena's `rect.y + rect.h`. Team Arena's own HUD
  never sets an alignment on these, so it draws as before.
- HUD choice: `cg_hudFiles ui/hud.txt` (standard), `ui/hud2.txt` (minimal),
  `ui/hud3.txt` (competitive), applied with `loadhud`.

### Lag compensation

`code/baseql/game/g_lagcomp.c`, after Quake Live's `g_lagHaxMs` and
`g_lagHaxHistory` (`docs/baseql-cvars.md`). The design is Unlagged's
(backward reconciliation), written afresh and kept small.

- A client draws the other players between its last two snapshots, at the
  time it sends in each command (`usercmd_t serverTime`, the same as the
  cgame's `cg.time`). With `cg_smoothClients 0`, the default, it interpolates
  between snapshot positions and never extrapolates.
- At the end of every server frame, `G_StoreLagFrames` records each player's
  snapped origin (`s.pos.trBase`, what snapshots carry), bounds and teleport
  bit, 32 frames deep. The client struct is cleared on spawn, so no frames
  from before a respawn survive.
- `FireWeapon` wraps the hitscan weapons, and `ClientThink_real` the
  gauntlet check, in `G_RewindClients`/`G_RestoreClients`. The rewind goes
  to the shooter's command time, at most `g_lagHaxMs` back and no further
  than the oldest of `g_lagHaxHistory` frames, interpolating between the two
  frames around it; across a teleport it takes the older frame, as clients
  don't interpolate there. Only the other players move; the shooter is where
  their own Pmove put them.
- A paused `Pmove` doesn't set bounds, so `ClientThink_real` keeps the last
  ones while frozen; otherwise frames stored during a pause would have empty
  hitboxes.

### Ready-up, timeouts and pauses

`code/baseql/game/g_match.c`.

- **Ready-up:** `CheckTournament` asks `G_ReadyToStart` before starting the
  warmup countdown, in duels and in the other gametypes. Ready flags live in
  `client->pers`, which a `map_restart` clears, so each match starts unready.
  While waiting, `CS_READYUP` (28) holds the number needed and a `0`/`1` per
  client, which the cgame turns into "Waiting for players to ready up" with
  who isn't ready.
- **Pause:** `level.pauseTime` marks a paused game; `CS_PAUSE` (29) tells
  clients when it paused, when it resumes and who called it. Players get
  `PM_FREEZE` (they can turn, not move or fire), can't respawn, and skip the
  once-a-second timer actions. Each frame, `G_RunPause` moves every clock
  forward by the frame's time: `level.startTime` (so the time limit waits),
  every `nextthink`, the trajectory times of everything but players (missiles,
  movers, dropped items hold still), and each player's powerups, respawn,
  air, inactivity, reward and kill times.
- **Clocks:** the server updates `CS_LEVEL_START_TIME` once, on resuming;
  meanwhile the cgame's `CG_LevelTime` stands at the pause time, so the match
  timer and the time limit warnings stop too. `G_LogPrintf`'s times count from
  `level.startTime`, so log times stand still during a pause as well.
- Resuming, after a timeout ends or by `timein`/`unpause`, runs a 3 second
  countdown. A server `pause` during a timeout keeps the original pause time.

### Hit feedback

- **Damage:** `G_Damage` adds what an enemy takes (health and armor) to the
  attacker's `persistant[PERS_DAMAGE_GIVEN]` (slot 15, modulo 32768, as
  persistant values travel as 16-bit shorts), next to `PERS_HITS`. The cgame
  turns the difference between snapshots into one of four beeps, in steps of
  25. QL's beeps fall from about 660 Hz (`hit0`) to 460 Hz (`hit3`), so more
  damage sounds lower.
- **Plums:** damage to each player is summed per frame and attacker, with the
  position the attacker saw (after any rewind), and sent at the end of the
  frame as `EV_DAMAGEPLUM`, a temp entity for that attacker only
  (`SVF_SINGLECLIENT`). Snapshots match that against `ps.clientNum`, so
  spectators following the attacker get it too. The event carries the damage
  in `s.time` and the means of death in `eventParm`, which `cg_damagePlum`
  filters by weapon.

### Forced models

- `CG_NewClientInfo` decides per player whether `cg_forceEnemyModel` or
  `cg_forceTeamModel` applies (`CG_ForcedModelSide`): outside team games
  everyone but the local player is an enemy; in team games it goes by the
  local player's team, read from their configstring since their own info may
  not be parsed yet, and spectators force neither. A change of the local
  player's team, or of either cvar, rebuilds every client info.
- A forced model is loaded at once rather than deferred to the scoreboard
  (`cg_deferPlayers`); all its players share it, so it loads once.
- Its skin is looked up as `<part>_<skin>.skin` first, also in team games,
  where vanilla would substitute the team skin. Quake Live's model icons are
  PNGs, which the icon lookup didn't try; without an icon the model failed to
  register and fell back to Sarge.
- `CG_Player` sets each part's `shaderRGBA` from the cvars (white for players
  who aren't forced). Quake Live's bright shaders tint the masked part of
  the texture with it (`rgbGen entity` behind `alphaFunc GE128`).

### End-of-match vote

- `BeginIntermission` calls `G_StartMapVote` (`g_match.c`), which takes up to
  three random map pool entries (`G_PickMapPoolEntries` in `g_factory.c`,
  which reads the pool and factories again after a `map_restart` restarted the
  game module). `CS_MAPVOTE` (30) carries the end time and each map, factory
  and vote count.
- `vote <n>` during intermission goes to `Cmd_MapVote_f` before the
  intermission's command filter, and the cgame's weapon keys send it.
  `CheckIntermissionExit` hands over to `G_MapVoteOver`, then `G_EndMapVote`
  sets `nextmap` to the winner's `set g_factory ...; map ...` before
  `ExitLevel`.
- The cgame draws its own vote panel under the scoreboard. Quake Live's
  `endgamevote.menu` uses the same `vote <n>` command, so it can replace the
  panel once the end scoreboard menus run.

### Scoreboard and shaders

- Quake Live's HUD files load no scoreboard menu, and Team Arena's cgame drew
  nothing without one; `CG_DrawScoreboard` now falls back to Quake 3's
  `CG_DrawOldScoreboard`.
- ioquake3 rejects a shader with an unknown general keyword, and a rejected
  shader has no stages, so it draws nothing. `make-ql-pak.py` leaves out the
  seven Quake Live uses (`strip_unknown_keywords`); `novlcollapse` alone was
  in 648 shaders, every bright skin and a number of map surfaces among them.

## Testing

There's no test suite. Every change has to build cleanly as native libraries
and QVMs; game code must also build with q3lcc.

- **Smoke test:** `code/baseql/tools/smoketest.sh <build/Release> <qlds dir>`
  runs `ioq3ded` with bots on a Quake Live map. It needs the free Quake Live
  dedicated server (steamcmd app 349090) and an x86_64 Linux host; Docker on
  the server box works, with `--network none`.
- **Client prediction:** a dedicated server and client on one machine over
  UDP with `sv_packetdelay 100` and `cl_packetdelay 100` (loopback ignores the
  delay), watching `cg_showmiss 1` and `cg_debugEvents 1`. Details in the plan
  under "How to check each phase".
- **Local duel:** one pure `ioq3ded` with `duel.cfg` and two windowed clients
  on `127.0.0.1`, with `+set fs_basepath <rig> +set fs_homepath <rig>/home-a`
  per client so they don't share configs. A rig directory holds `baseq3`,
  `missionpack`, and `baseql/` with `baseql.pk3` and `quakelive.pk3`. Shorten
  the match with `timelimit 1` from the server console.
- **HUD:** `+devmap <map> duel +exec hudtest.cfg` with a fixed
  `r_customwidth`/`r_customheight` (try 1280×720 and 4:3), where the cfg
  waits, adds a bot, gives armor and takes `screenshotJPEG`s. Repeat with
  `cg_hudFiles ui/hud2.txt` and `ui/hud3.txt`.
- **Fonts:** as for the HUD, also at 640×480 and 2560×1440 and with
  `cl_renderer opengl1`. For the other faces, put a copy of `hud.menu` with
  `font FONT_SANS` or `FONT_MONO` on an item loose in `baseql/ui/` (with
  `sv_pure 0`) and point `cg_hudFiles` at it. A `quakelive.pk3` without
  `fonts/` checks the fallback.
- **Lag compensation:** a `devmap` duel server with `g_debugLagComp 1`, one
  client with `cl_packetdelay 60` firing the machinegun and one strafing. A
  temporary cgame print of the other player's `lerpOrigin` at each `cg.time`
  (gated on `cg_debugposition 2`) gives where the shooter saw the target;
  the server log gives where it rewound the target to, for the same time.
  Result: 68 shots, rewinds of 75 to 121 ms, rewound positions within 1.7
  units of what the client drew (median 0), against 29 units (up to 41)
  without the rewind. The target must be in the shooter's view, and spawns
  are random, so it can take a second try.
- **Ready-up and timeouts:** the same setup, the clients' cfgs typing
  `readyup`, `timeout` and `timein`; the log shows the countdown waiting for
  both, and the match clock standing still through the timeout.
- **Pauses, plums and beeps:** two bots duel (they ready up by themselves)
  while a client spectates with `follow 0` once the match has started (a
  `map_restart` ends following). The server console sets `timelimit 1`,
  then `pause` and `unpause`; the match ended 75 s after it started, 60 plus
  the 15 s pause. Screenshots show plums and the pause screen; a temporary
  cgame print showed beeps in all four steps. With `s_initsound 0` no sounds
  register, so beeps fall back; use `s_volume 0` instead.
- **Forced models:** for a close look, temporarily let the forced model apply
  to the local player and use `cg_thirdPerson 1`; change the color cvars
  live. In a bot duel, a spectator following one bot sees the other.
- **Map vote:** the bot duel with a spectator whose cfg keeps pressing
  `weapon 2`; the test `quakelive.pk3` needs at least three pool maps besides
  the current one. The log shows `MapVote:` and the next map loading.
- **Vanilla baseline:** with `BUILD_BASEQL=OFF`, `baseq3` and `missionpack`
  must build and behave as before.

## Deployment

- **Linux binaries** build in an Ubuntu 22.04 container (gcc, Ninja,
  `libsdl2-dev`, CMake 3.31 from pip): `cmake -S . -B build-linux -G Ninja
  -DCMAKE_BUILD_TYPE=Release -DBUILD_BASEQL=ON -DUSE_FREETYPE=ON`. The result
  needs glibc 2.34 or newer and SDL2. FreeType is built in.
- **Player package:** `ioquake3`, both renderers, `baseql/baseql.pk3`,
  `make-ql-pak.py`, a README with setup steps, `COPYING.txt` and a source
  tarball of the same commit (GPL). No Quake Live, Quake 3 or Team Arena data.
- **Server directory:** `ioq3ded`, `baseq3/`, `missionpack/`, `baseql/` with
  `baseql.pk3` and `quakelive.pk3`, and a start script:

  ```sh
  #!/bin/sh
  cd "$(dirname "$0")"
  exec ./ioq3ded +set fs_basepath "$PWD" +set fs_homepath "$PWD/home" \
      +set fs_basegame missionpack +set fs_game baseql \
      +set sv_pure 1 +set sv_allowDownload 0 +set net_port 27960 \
      +set sv_hostname "baseql duel" +exec duel.cfg "$@" +map aerowalk duel
  ```

  Start it with `nohup ./run-duel-server.sh > server.log 2>&1 &`. Players
  outside the LAN need UDP 27960 forwarded to the server.
- **Releasing a change:** rebuild, copy the new `baseql.pk3` to the server
  and both players (with `sv_pure 1` they must match exactly), rebuild
  `quakelive.pk3` if `make-ql-pak.py` changed, restart the server.

## Known issues and open questions

Not verified yet:

- The HUD's chat window and kill feed in a real match (the HUD script hides the
  kill feed in duel).
- Which way the health and armor bars drain, and the colorized icons' color
  outside team games.
- The competitive HUD's vertical layout against real Quake Live.
- Chat may show twice, in the HUD and the console notify area;
  `con_notifytime 0` hides the latter.
- Lag compensation over a real internet link, and how the hit beeps sound
  (the tests checked which beep plays, with the volume at 0).
- Damage plums' size and placement in play; they grow with distance beyond
  300 units to stay readable.

Guesses to replace with Quake Live's real values when a source turns up:

- Green Armor's 25 armor; the HMG's spread; the shotgun's ring radii.
- Which TrueType font each `FONT_*` names, judged from the names and their
  use: the HUD asks for `FONT_DEFAULT` on its big numbers, and the scoreboards
  `FONT_SANS` on most text.
- `g_respawn_delay_max` as a forced respawn time; `pmove_WaterWadeScale`
  applying to wading.
- What `g_lagHaxMs` and `g_lagHaxHistory` do exactly (read as the most
  milliseconds rewound, and frames of history); what `g_damagePlums` 1 and 2
  mean; the damage steps of QL's hit beeps (25 here) and its plums' colors.
- Quake Live's remaining `pmove_*` cvars (air control, step, ramp and chain
  jumps, crouch slide) aren't implemented for lack of a description.

Decisions still open:

- Whether `g_gametype` numbers should match Quake Live's (0 ffa, 1 duel,
  2 race, 3 tdm, ...) for external tools that read serverinfo.
- Quake Live's `scripts/arenas.txt` (13 KB) is over the game's 8 KB limit;
  `make-ql-pak.py` splits it, but the engine limit stays.

## Pitfalls

Things that cost time once:

- QVMs went stale after header edits until `ac188a4c`; if behavior doesn't
  match the source, check the QVM was rebuilt.
- The QVM interpreter rounded float-to-int casts (fixed in `5b4594d7`). On
  non-x86 hosts such as Apple Silicon Macs, which have no QVM JIT, that broke
  `bg_lib`'s `%f` and so the tuning values the client parsed.
- Prediction bugs don't show over loopback; use real UDP with packet delay.
- Restore temporary debug edits from a saved copy, not `git checkout`, which
  also throws away uncommitted work in the same file.
- Tarring the tree on macOS for Linux: `COPYFILE_DISABLE=1 tar --no-xattrs`,
  or extended attributes come along as `._*` files.
- The macOS build is always universal (arm64 and x86_64,
  `cmake/platforms/macos.cmake`), so Homebrew's arm64-only libraries don't
  link; that's why FreeType is vendored.
- `misc/update-libs.sh` needs GNU `find`; run it on Linux.
- `duel.cfg` sets `bot_enable 0`, and it wins over a `+set bot_enable 1` on
  the command line; for bot tests, set the warmup cvars directly instead.
- `addbot` sent before the map finishes loading is silently ignored.
- In a duel, the first client to connect takes a player slot; a test
  spectator should connect after the bots and type `team spectator`.
- A model that fails to register (a missing skin or icon) silently falls back
  to Sarge while the client info keeps the requested names; check the model
  actually drawn, not the names.
- `cg_deferPlayers` keeps new models from loading until the scoreboard opens.
- Pure servers compare pk3s by checksum, so every player needs the server's
  exact `baseql.pk3` and a `quakelive.pk3` built from the same Quake Live
  data.

# Playing baseql duels

How to set up baseql for 1v1 duels on Quake Live's maps, on your own server.

## What each player needs

- This engine (`ioquake3` from this repo), built with `-DUSE_FREETYPE=ON` to
  draw the HUD in Quake Live's fonts. Stock ioquake3 can't load Quake Live
  maps.
- Quake III Arena: `baseq3/pak0.pk3` plus the 1.32 point release's
  `pak1.pk3`–`pak8.pk3`.
- Team Arena, whose menus baseql uses: `missionpack/pak0.pk3` plus the
  point release's `pak1.pk3`–`pak3.pk3`.
- Quake Live, to build `quakelive.pk3` (maps, HMG, Green Armor, HUD, fonts).
- `baseql.pk3`: the same file the server uses.

The point release files are free: `quake3-latest-pk3s.zip` from
<https://files.ioquake3.org/> holds `baseq3/pak1-8.pk3` and
`missionpack/pak1-3.pk3`.

```
quake3/
    ioquake3                 (the engine; ioquake3.app on macOS)
    baseq3/pak0.pk3 ... pak8.pk3
    missionpack/pak0.pk3 ... pak3.pk3
    baseql/baseql.pk3
    baseql/quakelive.pk3
```

Build `quakelive.pk3` with your own data (see `docs/baseql-ql-pak.md`):

```sh
python3 code/baseql/tools/make-ql-pak.py --quakelive "/path/to/Quake Live/baseq3" \
    --quake3 quake3/baseq3 --missionpack quake3/missionpack --output quake3/baseql/quakelive.pk3
```

Both players running it on the same game data get identical pk3s, which a pure
server checks.

## Playing

```sh
ioquake3 +set fs_basegame missionpack +set fs_game baseql
```

`fs_basegame missionpack` loads Team Arena's paks under baseql. Once in the
console, `seta snaps 40` gets a snapshot every server frame, then
`connect <server>`.

A duel starts with a 10 second countdown once both players are in. It runs 10
minutes; a tie at the end goes to sudden death. At the scoreboard, press fire:
the next map, picked at random from Quake Live's duel maps, loads when both
players have pressed it or 10 seconds after the first did.

The HUD is Quake Live's standard one. `seta cg_hudFiles ui/hud2.txt` switches
to the minimal HUD and `ui/hud3.txt` to the competitive one; `loadhud` applies
it.

## Running the server

```sh
ioq3ded +set fs_basegame missionpack +set fs_game baseql \
    +set sv_pure 1 +set sv_allowDownload 0 +exec duel.cfg +map aerowalk duel
```

- The server needs the same game data as the players: `baseq3`, `missionpack`,
  `baseql.pk3` and `quakelive.pk3`.
- `duel.cfg` (inside `baseql.pk3`) selects the duel factory and Quake Live's
  duel map pool (`mappool_duel.txt`), a 10 second warmup, no bots and 40 server
  frames a second.
- Keep `sv_allowDownload 0`: with downloads on, the server would send
  `quakelive.pk3`, Quake Live's assets, to players who lack it.
- With `sv_pure 1`, players must have exactly the server's `baseql.pk3`; copy
  it from the server rather than building your own.
- `startRandomMap` changes to a random duel map; `map <name> duel` to a given
  one.

## Not there yet

- Ready-up before the countdown, timeouts and pauses.
- Lag compensation for hitscan weapons, so leading targets depends on ping as
  in vanilla Quake 3.
- Forced enemy models and bright skins.
- Quake Live's vote for the next map at the end of a match.
- PQL movement, armor tiers and step/ramp jumps.
- Green Armor gives 25 armor, an educated guess; Quake Live's value isn't
  documented.

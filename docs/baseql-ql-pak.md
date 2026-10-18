# Quake Live content for baseql

`code/baseql/tools/make-ql-pak.py` builds `quakelive.pk3` from your own copy of
Quake Live: the Heavy Machinegun's model, sounds and icons, Quake Live's HUD,
fonts and hit beeps, and Quake Live's 149 maps with the textures, shaders, models and sounds they
use. It needs your
own copies of Quake III Arena and Team Arena (baseql's menus use Team Arena's
data) and Quake Live.

The pk3 contains Quake Live's copyrighted assets. It's for your own use; don't
redistribute it. The repo contains only the script.

## Building it

```sh
python3 code/baseql/tools/make-ql-pak.py \
    --quakelive "/path/to/Quake Live/baseq3" \
    --quake3 /path/to/quake3/baseq3 \
    --missionpack /path/to/quake3/missionpack \
    --output quakelive.pk3
```

- `--quakelive` is the `baseq3` directory holding Quake Live's `pak00.pk3`.
  Tested with the Quake Live dedicated server's (`steamcmd +login anonymous
  +app_update 349090`).
- `--maps aerowalk,bloodrun` packs only some maps; `--list-maps` lists them.
- It needs Python 3 and nothing else, and takes about half a minute. With all
  maps the pk3 is about 700 MB.

Put `quakelive.pk3` in your `baseql` directory. For a pure server, every player
needs the same pk3; the same Quake Live data gives the same files.

## What goes in

- The HMG: `models/weapons3/hmg/`, its ammo box, icons and sounds. Without the
  pk3, baseql uses the machinegun's model, icons and sounds for the HMG.
- Each map's `.bsp`, bot navigation (`.aas`) and levelshot, plus every texture,
  shader, model and sound it references, found by reading the map, its models
  and Quake Live's shader scripts.
- `scripts/quakelive.shader`: the Quake Live shader definitions those use,
  copied from Quake Live's scripts.
- `scripts/quakelive*.arena`: the maps' names and gametypes for the menus, in
  files under the game's 8 KB limit.
- Quake Live's HUD scripts, replacing Team Arena's, and the images under
  `ui/assets/hud/` they use: `ui/hud.txt` loads the standard HUD, `ui/hud2.txt`
  the minimal one and `ui/hud3.txt` the competitive one. Pick one with
  `seta cg_hudFiles ui/hud3.txt`, then `loadhud`.
- Quake Live's fonts, which the HUD's text draws in: `fonts/handelgothic.ttf`,
  `notosans-regular.ttf` and `droidsansmono.ttf`. The engine renders them when
  built with `-DUSE_FREETYPE=ON`.
- Quake Live's hit beeps, `sound/feedback/hit0.ogg` to `hit3.ogg`, pitched
  lower for more damage (`cg_hitBeep 2`).

Files Quake 3 or Team Arena already have are left out, and so are Quake Live
shader definitions for names Quake 3's maps use as plain images, so Quake 3's
maps look as before. Quake Live's versions of Quake 3 maps with the same file
name are skipped too.

MD3 models with more than 999 vertices or 1999 triangles in a surface, which
ioquake3's renderers refuse, have those surfaces split. The HMG's model is one.

## Limits

- Quake Live maps need baseql's engine, which loads their BSP version (47).
- A few dozen referenced files exist in neither game, mostly editor textures;
  the script lists them. The engine warns about a few Quake Live shader
  keywords it doesn't know.
- An engine built without FreeType draws the HUD in Team Arena's font, at
  the nearest size it has. HUD parts for gametypes baseql doesn't have yet
  (rounds, Domination, race) draw nothing.
- Quake Live entities baseql doesn't have yet (Domination points, race
  checkpoints, ammo packs, advertisements) are ignored with a warning.

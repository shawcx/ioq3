# baseql factories and map pool

Factories and the map pool work as in Quake Live (QL), so QL's own factory
files and map pools load unchanged, apart from factories for gametypes baseql
doesn't have yet.

## Factories

A factory is a named gametype preset: a base gametype plus cvar values. baseql
loads them from `scripts/factories.txt`, then from every `scripts/*.factories`
file, on each map load. Each file holds one factory object or a JSON array of
them:

```json
{
  "id": "instarail",
  "title": "Instarail",
  "author": "you",
  "description": "Railgun and gauntlet only.",
  "basegt": "ffa",
  "cvars": {
    "g_startingWeapons": "65",
    "g_damage_rg": "1000"
  }
}
```

- All six fields are required. Unknown fields are ignored.
- `id` is what `map` and map pools use. It and every cvar name may only
  contain letters, digits, `_`, `-`, `.` and `/`.
- `basegt` is one of `ffa`, `duel`, `tdm`, `ctf`, `oneflag` or `har`. QL's `race`,
  `ca`, `ft`, `dom`, `ad` and `rr` are rejected until baseql has them.
- A factory can't set `g_gametype`; `basegt` sets it.
- If two factories share an id, the first one loaded wins. A loose
  `scripts/factories.txt` takes precedence over one inside a pk3 in the same
  directory.
- Invalid factories are skipped, with the reason printed at map load, for
  example `scripts/x.factories: factory "myca" skipped: basegt "ca" isn't
  supported yet`, or the line number of a JSON syntax error.

The built-in factories in `code/baseql/data/scripts/factories.txt` are `ffa`,
`duel`, `tdm`, `ctf`, `oneflag` and `har`.

### Selecting a factory

- `map <map> <factory>`, for example `map q3dm17 duel`. The engine stores the
  second argument in `g_factory`.
- `set g_factory duel`, then `map q3dm17`. A factory stays selected until
  another is chosen.
- With `g_factory` empty (the default), no factory is used and `g_gametype`
  works as in vanilla Quake 3.

On each full map load (not `map_restart`), baseql resets every cvar the
previously applied factory set to its default, then sets the selected factory's
cvars and `g_gametype`. Settings from one factory don't leak into the next, but
changes made during a match, such as a vote, last until the next map.
`g_gametype` takes effect on the same map load.

An unknown id prints a warning and keeps the previous factory.

## Map pool

`sv_mapPoolFile` (default `mappool.txt`) lists one `map|factory` per line; `#`
starts a comment. Entries whose map or factory doesn't exist are skipped with a
warning. On each map load baseql sets `nextmap` to a random entry, avoiding the
current map and factory, so the pool rotates. A classic rotation script that
sets `nextmap` itself still takes precedence.

The default pool in `code/baseql/data/mappool.txt` uses Quake 3 maps.

## Cvars

| Cvar | Default | |
|---|---|---|
| `g_factory` | (empty) | id of the selected factory; in serverinfo |
| `g_factoryTitle` | (empty) | its title; in serverinfo, set by the game |
| `g_factoryApplied` | (empty) | internal: the factory whose cvars are currently set |
| `sv_mapPoolFile` | `mappool.txt` | map pool file, relative to the game directory |

## Commands

| Command | |
|---|---|
| `reload_factories` | reload factory files and the map pool, and pick a new `nextmap` |
| `reload_mappool` | reload the map pool and pick a new `nextmap` |
| `startRandomMap` | change to a random map pool entry now |

Reloading doesn't change the running match; the factory is applied on the next
map load.

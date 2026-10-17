# baseql cvars

Server cvars that baseql adds, named as in Quake Live (QL) so QL factories and
configs carry over. Defaults are QL's, from the QL dedicated server. Where
vanilla Quake 3 / Team Arena differs, its value is in parentheses.

## Per weapon

Each cvar name is the column's pattern with the weapon's suffix, for example
`g_damage_rg`. `weapon_reload_*` uses its own suffixes (`gauntlet`, `prox`).
An empty cell means the weapon has no such cvar.

| Weapon | suffix | `weapon_reload_*` | `g_damage_*` | `g_splashdamage_*` | `g_splashradius_*` | `g_velocity_*` | `g_knockback_*` | `g_knockback_*_self` | `g_startingAmmo_*` |
|---|---|---|---|---|---|---|---|---|---|
| Gauntlet | `g` | 400 | 50 |  |  |  | 1 |  | -1 |
| Machinegun | `mg` | 100 | 5 (7) |  |  |  | 1 |  | 100 |
| Shotgun | `sg` | 1000 | 5 (10) |  |  |  | 1 |  | 10 |
| Grenade Launcher | `gl` | 800 | 100 | 100 | 150 | 700 | 1.10 (1) |  | 10 |
| Rocket Launcher | `rl` | 800 | 100 | 84 (100) | 120 | 1000 (900) | 0.90 (1) | 1.10 (1) | 5 |
| Lightning Gun | `lg` | 50 | 6 (8) |  |  |  | 1.75 (1) |  | 100 |
| Railgun | `rg` | 1500 | 80 (100) |  |  |  | 0.85 (1) |  | 5 |
| Plasma Gun | `pg` | 100 | 20 | 15 | 20 | 2000 | 1.10 (1) | 1.30 (1) | 50 |
| BFG | `bfg` | 300 (200) | 100 | 100 | 80 (120) | 1800 (2000) | 1 |  | 10 |
| Grappling Hook | `gh` | 100 (400) |  |  |  | 1800 (800) | 1 |  | -1 |
| Nailgun | `ng` | 1000 | 12 (20) |  |  |  | 1 |  | 10 |
| Prox Launcher | `pl` | 800 | 0 | 100 | 150 |  | 1 |  | 5 |
| Chaingun | `cg` | 50 (30) | 8 (7) |  |  |  | 1 |  | 100 |

- `weapon_reload_*`: msec between shots.
- `g_velocity_*`: projectile speed.
- `g_knockback_*`: knockback scale. `g_knockback_*_self` replaces it when
  players hit themselves (rocket and plasma jumps).
- `g_startingAmmo_*`: ammo for weapons given by `g_startingWeapons`. `-1` is
  unlimited.
- The grappling hook is still vanilla's weapon hook. QL's is an offhand hook that
  damages and pulls players (`g_damage_gh`, `g_knockback_gh`), which baseql
  doesn't have yet.

## Shotgun and nailgun

The shotgun fires QL's fixed pattern: 20 pellets in three rings, 5 inner,
5 middle and 10 outer, spread 1000 (vanilla: 11 pellets at random, spread 700).
The ring layout is documented, but the radii are an approximation. The server
and cgame both use `BG_ShotgunPellet`.

| Cvar | Default | Meaning |
|---|---|---|
| `g_damage_sg_outer` | 5 | damage of each outer ring pellet; `g_damage_sg` is the inner two rings |
| `g_nailcount` | 10 (15) | nails per shot |
| `g_nailspread` | 400 (500) | nail spread |
| `g_nailspeed` | 1000 (555 to 2355 at random) | nail speed |
| `g_nailbounce` | 1 (0) | nails bounce off walls |
| `g_nailbouncepercentage` | 65 | percent of its speed a nail keeps on each bounce |

## Other

| Cvar | Default | Meaning |
|---|---|---|
| `pmove_WeaponDropTime` | 200 | msec to lower a weapon when switching |
| `pmove_WeaponRaiseTime` | 200 (250) | msec to raise the new weapon |
| `g_startingWeapons` | 3 | bitmask of weapons given at spawn: bit (weapon number − 1), so 1 = gauntlet, 2 = machinegun, 64 = railgun |
| `g_max_knockback` | 120 (200) | cap on knockback per hit, after scaling |
| `g_quadDamageFactor` | 3 | damage multiplier with Quad Damage (replaces `g_quadfactor`) |

Vanilla's team deathmatch rule (machinegun 5 damage, 50 starting ammo) is gone;
as in QL, the values come from these cvars and factories.

## How shared values reach clients

`weapon_reload_*`, `pmove_WeaponDropTime` and `pmove_WeaponRaiseTime` affect
player movement prediction, so clients need the same values as the server. The
game sends them in configstring `CS_TUNING` (26) whenever one changes, and the
game and cgame both apply that string with `BG_SetTuning`
(`code/baseql/game/bg_tuning.c`). The other cvars only matter on the server.

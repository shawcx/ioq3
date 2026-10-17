# baseql cvars

Server cvars that baseql adds, named as in Quake Live (QL) so QL factories and
configs carry over. Defaults are vanilla Quake 3 / Team Arena, so baseql plays
like `missionpack` until they're changed. Where QL's own default differs, it's
shown in parentheses; phase 1 of `docs/quakelive-plan.md` switches to those
together with the matching mechanics (for example QL's shotgun pattern).

## Per weapon

Each cvar name is the column's pattern with the weapon's suffix, for example
`g_damage_rg`. `weapon_reload_*` uses its own suffixes (`gauntlet`, `prox`).
An empty cell means the weapon has no such cvar.

| Weapon | suffix | `weapon_reload_*` | `g_damage_*` | `g_splashdamage_*` | `g_splashradius_*` | `g_velocity_*` | `g_knockback_*` | `g_knockback_*_self` | `g_startingAmmo_*` |
|---|---|---|---|---|---|---|---|---|---|
| Gauntlet | `g` | 400 | 50 |  |  |  | 1 |  | -1 |
| Machinegun | `mg` | 100 | 7 (5) |  |  |  | 1 |  | 100 |
| Shotgun | `sg` | 1000 | 10 (5) |  |  |  | 1 |  | 10 |
| Grenade Launcher | `gl` | 800 | 100 | 100 | 150 | 700 | 1 (1.10) |  | 10 |
| Rocket Launcher | `rl` | 800 | 100 | 100 (84) | 120 | 900 (1000) | 1 (0.90) | 1 (1.10) | 5 |
| Lightning Gun | `lg` | 50 | 8 (6) |  |  |  | 1 (1.75) |  | 100 |
| Railgun | `rg` | 1500 | 100 (80) |  |  |  | 1 (0.85) |  | 5 |
| Plasma Gun | `pg` | 100 | 20 | 15 | 20 | 2000 | 1 (1.10) | 1 (1.30) | 50 |
| BFG | `bfg` | 200 (300) | 100 | 100 | 120 (80) | 2000 (1800) | 1 |  | 10 |
| Grappling Hook | `gh` | 400 (100) |  |  |  | 800 (1800) | 1 (-5) |  | -1 |
| Nailgun | `ng` | 1000 | 20 (12) |  |  |  | 1 |  | 10 |
| Prox Launcher | `pl` | 800 | 0 | 100 | 150 |  | 1 |  | 5 |
| Chaingun | `cg` | 30 (50) | 7 (8) |  |  |  | 1 |  | 100 |

- `weapon_reload_*`: msec between shots.
- `g_velocity_*`: projectile speed.
- `g_knockback_*`: knockback scale. `g_knockback_*_self` replaces it when
  players hit themselves (rocket and plasma jumps).
- `g_startingAmmo_*`: ammo for weapons given by `g_startingWeapons`. `-1` is
  unlimited.
- Vanilla's team deathmatch machinegun rules (5 damage, 50 starting ammo) still
  apply in team deathmatch, regardless of `g_damage_mg` and `g_startingAmmo_mg`.

## Other

| Cvar | Default | Meaning |
|---|---|---|
| `pmove_WeaponDropTime` | 200 | msec to lower a weapon when switching |
| `pmove_WeaponRaiseTime` | 250 (200) | msec to raise the new weapon |
| `g_startingWeapons` | 3 | bitmask of weapons given at spawn: bit (weapon number − 1), so 1 = gauntlet, 2 = machinegun, 64 = railgun |
| `g_max_knockback` | 200 (120) | cap on knockback per hit, after scaling |
| `g_quadDamageFactor` | 3 | damage multiplier with Quad Damage (replaces `g_quadfactor`) |

## How shared values reach clients

`weapon_reload_*`, `pmove_WeaponDropTime` and `pmove_WeaponRaiseTime` affect
player movement prediction, so clients need the same values as the server. The
game sends them in configstring `CS_TUNING` (26) whenever one changes, and the
game and cgame both apply that string with `BG_SetTuning`
(`code/baseql/game/bg_tuning.c`). The other cvars only matter on the server.

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
| Heavy Machinegun | `hmg` | 75 | 8 |  |  |  | 1 |  | 50 |

- `weapon_reload_*`: msec between shots.
- `g_velocity_*`: projectile speed.
- `g_knockback_*`: knockback scale. `g_knockback_*_self` replaces it when
  players hit themselves (rocket and plasma jumps).
- `g_startingAmmo_*`: ammo for weapons given by `g_startingWeapons`. `-1` is
  unlimited.
- The grappling hook is still vanilla's weapon hook. QL's is an offhand hook that
  damages and pulls players (`g_damage_gh`, `g_knockback_gh`), which baseql
  doesn't have yet.

## Heavy Machinegun

QL's Heavy Machinegun is weapon 14, as in QL, so `g_startingWeapons` bit 8192
gives it and QL maps' `weapon_hmg` and `ammo_hmg` spawn. Its spread isn't
known, so it uses the machinegun's. QL's model isn't free to use, so for now it
borrows the machinegun's with an orange muzzle flash. Bots can't use it yet:
botlib's weapon data only describes weapons 1 to 13.

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

## Items and spawning

| Cvar | Default | Meaning |
|---|---|---|
| `g_weaponRespawn` | 5 | seconds before a weapon respawns. Vanilla's team deathmatch value (30, `g_weaponTeamRespawn`) is gone; the built-in `tdm` factory sets 30 |
| `g_ammoRespawn` | 40 | seconds before ammo respawns |
| `g_powerupRespawn` | 120 | seconds before a powerup respawns |
| `g_spawnDelay_powerup` | 45 | powerups first appear this many seconds into the map... |
| `g_spawnDelayRandom_powerup` | 15 | ...plus or minus up to this many (vanilla hardcodes 45 ± 15) |
| `g_spawnItemWeapons`, `g_spawnItemAmmo`, `g_spawnItemArmor`, `g_spawnItemHealth`, `g_spawnItemPowerup`, `g_spawnItemHoldable` | 1 | 0 stops that class of items spawning |
| `g_startingHealth` | 100 | max health, scaled by the player's handicap; health above it counts down |
| `g_startingHealthBonus` | 25 | extra health at spawn, which counts down (vanilla: max + 25) |
| `g_startingArmor` | 0 | armor at spawn |
| `g_infiniteAmmo` | 0 | 1 gives unlimited ammo for every weapon |
| `g_dropPowerups` | 1 | dead players drop their powerups (vanilla: not in team deathmatch) |
| `g_respawn_delay_min` | 2100 (1700) | msec after dying before a player can respawn |
| `g_respawn_delay_max` | 2400 | msec after dying when a player respawns without pressing anything. Replaces `g_forcerespawn` (20 s). This reading of QL's cvar is an interpretation |

Health, armor and holdable respawn times (35, 25 and 60 seconds) have no QL
cvars and stay vanilla.

## Movement

| Cvar | Default | Meaning |
|---|---|---|
| `pmove_JumpVelocity` | 275 (270) | upward speed of a jump |
| `pmove_StepHeight` | 22 (18) | highest step players walk up |
| `pmove_WalkAccel` | 10 | ground acceleration |
| `pmove_WalkFriction` | 6 | ground friction |
| `pmove_AirAccel` | 1 | air acceleration |
| `pmove_WaterSwimScale` | 0.6 (0.5) | speed scale when swimming |
| `pmove_WaterWadeScale` | 0.8 (0.5) | speed scale when walking in water. Vanilla used the swim scale here; splitting them is an interpretation of QL's two cvars |
| `pmove_velocity_gh` | 800 | grappling hook pull speed |
| `pmove_AutoHop` | 1 (0) | holding jump jumps again on landing |
| `pmove_noPlayerClip` | 0 | players pass through each other |

QL's other `pmove_*` cvars (step, ramp, chain and double jumps, air control,
crouch slide, the jump velocity formula) aren't implemented: there's no public
description of what they do to check against.

## Lag compensation

Hitscan weapons (gauntlet, machinegun, shotgun, lightning gun, railgun,
chaingun, Heavy Machinegun) hit the other players where the shooter saw them:
the server moves them back to the server time in the shooter's command, the
time the shooter's screen was showing, then puts them back
(`code/baseql/game/g_lagcomp.c`). Projectiles aren't compensated, as in QL.

| Cvar | Default | Meaning |
|---|---|---|
| `g_lagHaxMs` | 80 | most milliseconds the server goes back; beyond that, players lead their targets. 0 turns lag compensation off |
| `g_lagHaxHistory` | 4 | server frames of positions it keeps (up to 32); it can't go back further than the oldest. At `sv_fps 40` the defaults go back 75 ms |
| `g_debugLagComp` | 0 | 1 prints every rewind: who fired, how far back, and where each player was moved from and to |

What the two QL cvars do exactly isn't documented; this reading of them is an
interpretation. A player's view is about their ping plus up to one snapshot
behind, so the defaults fully cover pings up to about 50 ms. For players
further away, raise both, for example `g_lagHaxMs 150` and `g_lagHaxHistory 8`.

## Ready-up and timeouts

| Cvar | Default | Meaning |
|---|---|---|
| `sv_warmupReadyPercentage` | 0.51 | share of the players who must type `readyup` before the warmup countdown starts, rounded up, so 2 of 2 in a duel. 0 starts as soon as there are enough players, as vanilla does |
| `g_warmupDelay` | 15 | seconds after the map loads before a match may start, so everyone can connect |
| `g_warmupReadyDelay` | 0 | if set, seconds after the first player readies up before the others are dealt with by... |
| `g_warmupReadyDelayAction` | 1 | ...1: moving players who aren't ready to spectator, 2: making them ready |
| `bot_autoReady` | 1 | bots count as ready |
| `g_timeoutCount` | 0 | timeouts each player (each team in team games) may call per match; `duel.cfg` sets 3 |
| `g_timeoutLen` | 60 | seconds a timeout lasts; 0 lasts until `timein` |

Commands:

- `readyup`: readies up in warmup, or takes it back. At the end of a match, it
  readies for the next map, as the attack button does.
- `timeout`, `timein`: call a timeout, and end it early; the caller, or the
  caller's team, can end it.
- On the server console: `pause` pauses until `unpause`, `unpause` ends any
  pause or timeout, and `allready` readies up everyone in warmup.

A paused game resumes after a 3 second countdown. While paused, players can
look around but not move or fire, every game clock stops (the match timer,
item respawns, powerups, respawning), and missiles, movers and dropped items
hold still.

## Hit feedback

| Cvar | Default | Meaning |
|---|---|---|
| `g_damagePlums` | 2 | the server sends damage plums. Anything but 0 sends them; what QL's 1 and 2 mean isn't known |
| `cg_damagePlum` | `g mg sg gl rl lg rg pg bfg gh ng pl cg hmg` | client: weapons, by their cvar suffix, whose damage shows as plums; empty shows none |
| `cg_hitBeep` | 2 | client: 0 no hit beeps, 1 Team Arena's (pitched by the target's armor), 2 QL's, pitched lower for more damage |

A plum is the damage done to one player in one server frame, so a shotgun
blast shows as one number. It rises from where the attacker saw the target,
white under 25 damage, then yellow, orange and red from 75. The hit beep uses
QL's four sounds, from `quakelive.pk3`, in the same steps of 25; without them
it falls back to Team Arena's. The steps and the colors are baseql's choice.

## How shared values reach clients

`weapon_reload_*` and the `pmove_*` cvars affect player movement prediction, so
clients need the same values as the server. The game sends the cvars' strings
in configstring `CS_TUNING` (26) whenever one changes, and the game and cgame
both apply that string with `BG_SetTuning` (`code/baseql/game/bg_tuning.c`).
Sending the strings rather than formatting the values means both sides parse
the same text; a value with characters that can't be in a number sends the
default. The other cvars only matter on the server.

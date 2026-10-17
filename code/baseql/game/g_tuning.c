/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
//
// g_tuning.c -- baseql: Quake Live weapon tuning cvars

#include "g_local.h"

weaponTuning_t	g_weaponTuning[WP_NUM_WEAPONS];

// cvars shared with the cgame through CS_TUNING, see bg_tuning.c
static vmCvar_t	bgTuningCvars[BG_MAX_TUNING_VARS];
static int		bgTuningModificationCounts[BG_MAX_TUNING_VARS];

typedef struct {
	weapon_t	weapon;
	char		*suffix;		// Quake Live cvar name suffix
	char		*damage;		// NULL if the weapon has no such cvar
	char		*splashDamage;
	char		*splashRadius;
	char		*velocity;
	char		*knockback;
	char		*knockbackSelf;
	char		*startingAmmo;
} weaponTuningDef_t;

// Defaults are Quake Live's. The grappling hook's knockback stays at 1: in
// Quake Live it is an offhand hook that damages players, which baseql's isn't.
static weaponTuningDef_t	weaponTuningDefs[] = {
	// weapon				suffix	damage	splash	radius	velocity	kb		kb self	ammo
	{ WP_GAUNTLET,			"g",	"50",	NULL,	NULL,	NULL,		"1",	NULL,	"-1" },
	{ WP_MACHINEGUN,		"mg",	"5",	NULL,	NULL,	NULL,		"1",	NULL,	"100" },
	{ WP_SHOTGUN,			"sg",	"5",	NULL,	NULL,	NULL,		"1",	NULL,	"10" },
	{ WP_GRENADE_LAUNCHER,	"gl",	"100",	"100",	"150",	"700",		"1.10",	NULL,	"10" },
	{ WP_ROCKET_LAUNCHER,	"rl",	"100",	"84",	"120",	"1000",		"0.90",	"1.10",	"5" },
	{ WP_LIGHTNING,			"lg",	"6",	NULL,	NULL,	NULL,		"1.75",	NULL,	"100" },
	{ WP_RAILGUN,			"rg",	"80",	NULL,	NULL,	NULL,		"0.85",	NULL,	"5" },
	{ WP_PLASMAGUN,			"pg",	"20",	"15",	"20",	"2000",		"1.10",	"1.30",	"50" },
	{ WP_BFG,				"bfg",	"100",	"100",	"80",	"1800",		"1",	NULL,	"10" },
	{ WP_GRAPPLING_HOOK,	"gh",	NULL,	NULL,	NULL,	"1800",		"1",	NULL,	"-1" },
#ifdef MISSIONPACK
	{ WP_NAILGUN,			"ng",	"12",	NULL,	NULL,	NULL,		"1",	NULL,	"10" },
	{ WP_PROX_LAUNCHER,		"pl",	"0",	"100",	"150",	NULL,		"1",	NULL,	"5" },
	{ WP_CHAINGUN,			"cg",	"8",	NULL,	NULL,	NULL,		"1",	NULL,	"100" },
#endif
	{ WP_HMG,				"hmg",	"8",	NULL,	NULL,	NULL,		"1",	NULL,	"50" },
};

static const int	numWeaponTuningDefs = ARRAY_LEN( weaponTuningDefs );

static void G_RegisterWeaponCvar( vmCvar_t *vmCvar, const char *prefix, const char *suffix,
		const char *postfix, const char *defaultString ) {
	if ( defaultString ) {
		trap_Cvar_Register( vmCvar, va( "%s%s%s", prefix, suffix, postfix ), defaultString, 0 );
	}
}

static void G_UpdateWeaponCvar( vmCvar_t *vmCvar, const char *defaultString ) {
	if ( defaultString ) {
		trap_Cvar_Update( vmCvar );
	}
}

/*
================
G_SendTuning

Applies the shared tuning cvars locally and sends them to clients.
================
*/
static void G_SendTuning( void ) {
	char	values[MAX_STRING_CHARS];
	int		i;

	values[0] = '\0';
	for ( i = 0 ; i < BG_NumTuningVars() ; i++ ) {
		Q_strcat( values, sizeof( values ), va( "%s%i", i ? " " : "", bgTuningCvars[i].integer ) );
	}

	BG_SetTuning( values );
	trap_SetConfigstring( CS_TUNING, values );
}

/*
================
G_RegisterTuningCvars
================
*/
void G_RegisterTuningCvars( void ) {
	int					i;
	weaponTuningDef_t	*def;
	weaponTuning_t		*wt;

	if ( BG_NumTuningVars() > BG_MAX_TUNING_VARS ) {
		G_Error( "G_RegisterTuningCvars: %i tuning cvars, max is %i", BG_NumTuningVars(), BG_MAX_TUNING_VARS );
	}

	for ( i = 0 ; i < BG_NumTuningVars() ; i++ ) {
		trap_Cvar_Register( &bgTuningCvars[i], BG_TuningVarName( i ), BG_TuningVarDefault( i ), 0 );
		bgTuningModificationCounts[i] = bgTuningCvars[i].modificationCount;
	}

	for ( i = 0, def = weaponTuningDefs ; i < numWeaponTuningDefs ; i++, def++ ) {
		wt = &g_weaponTuning[def->weapon];
		G_RegisterWeaponCvar( &wt->damage, "g_damage_", def->suffix, "", def->damage );
		G_RegisterWeaponCvar( &wt->splashDamage, "g_splashdamage_", def->suffix, "", def->splashDamage );
		G_RegisterWeaponCvar( &wt->splashRadius, "g_splashradius_", def->suffix, "", def->splashRadius );
		G_RegisterWeaponCvar( &wt->velocity, "g_velocity_", def->suffix, "", def->velocity );
		G_RegisterWeaponCvar( &wt->knockback, "g_knockback_", def->suffix, "", def->knockback );
		G_RegisterWeaponCvar( &wt->knockbackSelf, "g_knockback_", def->suffix, "_self", def->knockbackSelf );
		G_RegisterWeaponCvar( &wt->startingAmmo, "g_startingAmmo_", def->suffix, "", def->startingAmmo );
		wt->hasKnockbackSelf = ( def->knockbackSelf != NULL );
	}

	G_SendTuning();
}

/*
================
G_UpdateTuningCvars
================
*/
void G_UpdateTuningCvars( void ) {
	int					i;
	qboolean			changed;
	weaponTuningDef_t	*def;
	weaponTuning_t		*wt;

	changed = qfalse;
	for ( i = 0 ; i < BG_NumTuningVars() ; i++ ) {
		trap_Cvar_Update( &bgTuningCvars[i] );
		if ( bgTuningModificationCounts[i] != bgTuningCvars[i].modificationCount ) {
			bgTuningModificationCounts[i] = bgTuningCvars[i].modificationCount;
			changed = qtrue;
		}
	}
	if ( changed ) {
		G_SendTuning();
	}

	for ( i = 0, def = weaponTuningDefs ; i < numWeaponTuningDefs ; i++, def++ ) {
		wt = &g_weaponTuning[def->weapon];
		G_UpdateWeaponCvar( &wt->damage, def->damage );
		G_UpdateWeaponCvar( &wt->splashDamage, def->splashDamage );
		G_UpdateWeaponCvar( &wt->splashRadius, def->splashRadius );
		G_UpdateWeaponCvar( &wt->velocity, def->velocity );
		G_UpdateWeaponCvar( &wt->knockback, def->knockback );
		G_UpdateWeaponCvar( &wt->knockbackSelf, def->knockbackSelf );
		G_UpdateWeaponCvar( &wt->startingAmmo, def->startingAmmo );
	}
}

/*
================
G_WeaponForMeansOfDeath
================
*/
weapon_t G_WeaponForMeansOfDeath( int mod ) {
	switch ( mod ) {
	case MOD_GAUNTLET:
		return WP_GAUNTLET;
	case MOD_MACHINEGUN:
		return WP_MACHINEGUN;
	case MOD_SHOTGUN:
		return WP_SHOTGUN;
	case MOD_GRENADE:
	case MOD_GRENADE_SPLASH:
		return WP_GRENADE_LAUNCHER;
	case MOD_ROCKET:
	case MOD_ROCKET_SPLASH:
		return WP_ROCKET_LAUNCHER;
	case MOD_LIGHTNING:
		return WP_LIGHTNING;
	case MOD_RAILGUN:
		return WP_RAILGUN;
	case MOD_PLASMA:
	case MOD_PLASMA_SPLASH:
		return WP_PLASMAGUN;
	case MOD_BFG:
	case MOD_BFG_SPLASH:
		return WP_BFG;
	case MOD_GRAPPLE:
		return WP_GRAPPLING_HOOK;
#ifdef MISSIONPACK
	case MOD_NAIL:
		return WP_NAILGUN;
	case MOD_PROXIMITY_MINE:
		return WP_PROX_LAUNCHER;
	case MOD_CHAINGUN:
		return WP_CHAINGUN;
#endif
	case MOD_HMG:
		return WP_HMG;
	default:
		return WP_NONE;
	}
}

/*
================
G_KnockbackScale

Scale for the knockback of damage dealt by a weapon, from g_knockback_<w>
or, for self damage, g_knockback_<w>_self where the weapon has one.
================
*/
float G_KnockbackScale( gentity_t *targ, gentity_t *attacker, int mod ) {
	weapon_t		weapon;
	weaponTuning_t	*wt;

	weapon = G_WeaponForMeansOfDeath( mod );
	if ( weapon == WP_NONE ) {
		return 1.0f;
	}

	wt = &g_weaponTuning[weapon];
	if ( targ == attacker && wt->hasKnockbackSelf ) {
		return wt->knockbackSelf.value;
	}
	return wt->knockback.value;
}

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
// bg_tuning.c -- baseql: tuning cvars shared by the game and cgame

#include "../../qcommon/q_shared.h"
#include "bg_public.h"

bgTuning_t	bg_tuning;

typedef struct {
	char		*cvarName;
	char		*defaultString;
	int			*value;
} bgTuningVar_t;

// Defaults are Quake Live's. The values in CS_TUNING are listed in
// the order of this table, so the game and cgame must be built from the same
// source; GAME_VERSION guards against mixing them.
static bgTuningVar_t	bgTuningVars[] = {
	{ "weapon_reload_gauntlet",	"400",	&bg_tuning.weaponReload[WP_GAUNTLET] },
	{ "weapon_reload_mg",		"100",	&bg_tuning.weaponReload[WP_MACHINEGUN] },
	{ "weapon_reload_sg",		"1000",	&bg_tuning.weaponReload[WP_SHOTGUN] },
	{ "weapon_reload_gl",		"800",	&bg_tuning.weaponReload[WP_GRENADE_LAUNCHER] },
	{ "weapon_reload_rl",		"800",	&bg_tuning.weaponReload[WP_ROCKET_LAUNCHER] },
	{ "weapon_reload_lg",		"50",	&bg_tuning.weaponReload[WP_LIGHTNING] },
	{ "weapon_reload_rg",		"1500",	&bg_tuning.weaponReload[WP_RAILGUN] },
	{ "weapon_reload_pg",		"100",	&bg_tuning.weaponReload[WP_PLASMAGUN] },
	{ "weapon_reload_bfg",		"300",	&bg_tuning.weaponReload[WP_BFG] },
	{ "weapon_reload_gh",		"100",	&bg_tuning.weaponReload[WP_GRAPPLING_HOOK] },
#ifdef MISSIONPACK
	{ "weapon_reload_ng",		"1000",	&bg_tuning.weaponReload[WP_NAILGUN] },
	{ "weapon_reload_prox",		"800",	&bg_tuning.weaponReload[WP_PROX_LAUNCHER] },
	{ "weapon_reload_cg",		"50",	&bg_tuning.weaponReload[WP_CHAINGUN] },
#endif
	{ "pmove_WeaponDropTime",	"200",	&bg_tuning.weaponDropTime },
	{ "pmove_WeaponRaiseTime",	"200",	&bg_tuning.weaponRaiseTime },
};

static const int	bgNumTuningVars = ARRAY_LEN( bgTuningVars );

int BG_NumTuningVars( void ) {
	return bgNumTuningVars;
}

const char *BG_TuningVarName( int index ) {
	if ( index < 0 || index >= bgNumTuningVars ) {
		return "";
	}
	return bgTuningVars[index].cvarName;
}

const char *BG_TuningVarDefault( int index ) {
	if ( index < 0 || index >= bgNumTuningVars ) {
		return "";
	}
	return bgTuningVars[index].defaultString;
}

/*
================
BG_SetTuning

Applies a space-separated list of values in bgTuningVars order, as sent in
CS_TUNING. Values missing from the end of the list get their defaults.
================
*/
void BG_SetTuning( const char *values ) {
	int		i;
	char	*token;
	char	*p;

	// any weapon without its own reload time fires like the gauntlet, as in vanilla
	for ( i = 0 ; i < WP_NUM_WEAPONS ; i++ ) {
		bg_tuning.weaponReload[i] = 400;
	}

	p = (char *)values;
	for ( i = 0 ; i < bgNumTuningVars ; i++ ) {
		token = COM_Parse( &p );
		if ( !token[0] ) {
			token = bgTuningVars[i].defaultString;
		}
		*bgTuningVars[i].value = atoi( token );
	}
}

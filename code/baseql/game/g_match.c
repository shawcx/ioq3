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
// g_match.c -- baseql: match flow as in Quake Live: readying up in warmup,
// timeouts and pauses, and the vote for the next map

#include "g_local.h"

// the countdown before a paused game goes on
#define PAUSE_RESUME_MSEC	3000

// how long the end-of-match vote for the next map lasts
#define MAPVOTE_MSEC		15000

/*
================
G_IsReady
================
*/
static qboolean G_IsReady( gentity_t *ent ) {
	return ent->client->pers.ready || ( ( ent->r.svFlags & SVF_BOT ) && bot_autoReady.integer );
}

/*
================
G_ClearReadyUp

Takes down the clients' "ready up" message
================
*/
void G_ClearReadyUp( void ) {
	trap_SetConfigstring( CS_READYUP, "" );
	level.readyDelayTime = 0;
}

/*
================
G_ReadyToStart

Whether a match waiting in warmup may start its countdown: enough players
have readied up (sv_warmupReadyPercentage, rounded up), and g_warmupDelay
has passed since the map loaded. Keeps CS_READYUP current meanwhile, for
the clients' "ready up" message.
================
*/
qboolean G_ReadyToStart( void ) {
	char		flags[MAX_CLIENTS + 1];
	int			i, players, ready, needed;
	gentity_t	*ent;

	players = ready = 0;
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		flags[i] = '0';
		if ( !ent->inuse || ent->client->pers.connected != CON_CONNECTED
				|| ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}
		players++;
		if ( G_IsReady( ent ) ) {
			flags[i] = '1';
			ready++;
		}
	}
	flags[level.maxclients] = '\0';

	needed = players * sv_warmupReadyPercentage.value;
	if ( needed < players * sv_warmupReadyPercentage.value ) {
		needed++;
	}
	if ( needed > players ) {
		needed = players;
	}

	// g_warmupReadyDelay: once someone is ready, the others have a while to
	// follow before they're made ready or sent to spectate
	if ( !ready ) {
		level.readyDelayTime = 0;
	} else if ( !level.readyDelayTime ) {
		level.readyDelayTime = level.time;
	} else if ( ready < needed && g_warmupReadyDelay.integer > 0
			&& level.time - level.readyDelayTime >= g_warmupReadyDelay.integer * 1000 ) {
		for ( i = 0 ; i < level.maxclients ; i++ ) {
			ent = &g_entities[i];
			if ( flags[i] == '1' || !ent->inuse || ent->client->pers.connected != CON_CONNECTED
					|| ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
				continue;
			}
			if ( g_warmupReadyDelayAction.integer == 2 ) {
				ent->client->pers.ready = qtrue;
			} else {
				SetTeam( ent, "spectator" );
			}
		}
		trap_SendServerCommand( -1, va( "print \"Players who weren't ready were %s.\n\"",
			g_warmupReadyDelayAction.integer == 2 ? "made ready" : "moved to spectator" ) );
		level.readyDelayTime = 0;
		return qfalse;
	}

	trap_SetConfigstring( CS_READYUP, needed ? va( "%i %s", needed, flags ) : "" );

	if ( ready < needed ) {
		return qfalse;
	}

	// give everyone time to connect after the map loads
	if ( level.time - level.startTime < g_warmupDelay.integer * 1000 ) {
		return qfalse;
	}

	G_ClearReadyUp();
	return qtrue;
}

/*
================
Cmd_ReadyUp_f

Readies up for the match in warmup, or takes it back. At the end of a match,
it's ready for the next map, as the attack button is.
================
*/
void Cmd_ReadyUp_f( gentity_t *ent ) {
	gclient_t	*client;

	client = ent->client;

	if ( level.intermissiontime ) {
		client->readyToExit = qtrue;
		return;
	}

	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent - g_entities, "print \"Spectators don't ready up.\n\"" );
		return;
	}

	if ( level.warmupTime >= 0 ) {
		trap_SendServerCommand( ent - g_entities, level.warmupTime
			? "print \"The match is about to start.\n\""
			: "print \"The match has already started.\n\"" );
		return;
	}

	client->pers.ready = !client->pers.ready;
	trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " is %s.\n\"",
		client->pers.netname, client->pers.ready ? "ready" : "no longer ready" ) );
	G_LogPrintf( "ReadyUp: %i %i: %s\n", (int)( ent - g_entities ), client->pers.ready, client->pers.netname );
}

/*
================
G_SetPauseConfigstring
================
*/
static void G_SetPauseConfigstring( void ) {
	trap_SetConfigstring( CS_PAUSE, va( "%i %i %i", level.pauseTime, level.pauseEnd, level.pauseClient ) );
}

/*
================
G_Pause

Pauses the game, for msec or until resumed; caller is -1 for the server
================
*/
static void G_Pause( int caller, int msec ) {
	level.pauseTime = level.time;
	level.pauseEnd = msec > 0 ? level.time + msec : 0;
	level.pauseClient = caller;
	G_SetPauseConfigstring();
}

/*
================
G_Resume

Resumes the game after a short countdown
================
*/
static void G_Resume( void ) {
	if ( !level.pauseEnd || level.pauseEnd > level.time + PAUSE_RESUME_MSEC ) {
		level.pauseEnd = level.time + PAUSE_RESUME_MSEC;
		G_SetPauseConfigstring();
	}
}

/*
================
G_MatchInProgress

Timeouts and pauses are for matches being played: not warmup or intermission
================
*/
static qboolean G_MatchInProgress( void ) {
	return !level.warmupTime && !level.intermissiontime && !level.intermissionQueued;
}

/*
================
G_RunPause

While paused, everything timed waits: each frame moves every clock forward
by the frame's time, and frozen trajectories keep missiles, movers and
dropped items in place. Players are frozen in ClientThink_real.
================
*/
void G_RunPause( void ) {
	int			i, j, msec;
	gentity_t	*ent;
	gclient_t	*client;

	if ( !level.pauseTime ) {
		return;
	}

	msec = level.time - level.previousTime;
	level.startTime += msec;

	for ( i = 0 ; i < level.num_entities ; i++ ) {
		ent = &g_entities[i];
		if ( !ent->inuse ) {
			continue;
		}

		if ( ent->nextthink > 0 ) {
			ent->nextthink += msec;
		}

		client = ent->client;
		if ( client ) {
			// powerups that run out; carried flags hold INT_MAX
			for ( j = 0 ; j < MAX_POWERUPS ; j++ ) {
				if ( client->ps.powerups[j] > level.previousTime && client->ps.powerups[j] < INT_MAX - msec ) {
					client->ps.powerups[j] += msec;
				}
			}
			client->respawnTime += msec;
			client->airOutTime += msec;
			client->inactivityTime += msec;
			client->rewardTime += msec;
			client->lastKillTime += msec;
			client->pers.enterTime += msec;
#ifdef MISSIONPACK
			client->invulnerabilityTime += msec;
#endif
			continue;
		}

		if ( ent->s.pos.trType != TR_STATIONARY ) {
			ent->s.pos.trTime += msec;
		}
		if ( ent->s.apos.trType != TR_STATIONARY ) {
			ent->s.apos.trTime += msec;
		}
	}

	if ( level.pauseEnd && level.time >= level.pauseEnd ) {
		level.pauseTime = 0;
		level.pauseEnd = 0;
		trap_SetConfigstring( CS_PAUSE, "" );
		trap_SetConfigstring( CS_LEVEL_START_TIME, va( "%i", level.startTime ) );
		trap_SendServerCommand( -1, "print \"The game goes on.\n\"" );
		G_LogPrintf( "Resume:\n" );
	}
}

/*
================
Cmd_Timeout_f

Pauses the match for g_timeoutLen seconds; each player, or each team in team
games, has g_timeoutCount of them per match
================
*/
void Cmd_Timeout_f( gentity_t *ent ) {
	gclient_t	*client;
	int			*used;

	client = ent->client;

	if ( client->sess.sessionTeam == TEAM_SPECTATOR ) {
		trap_SendServerCommand( ent - g_entities, "print \"Spectators can't call timeouts.\n\"" );
		return;
	}
	if ( !G_MatchInProgress() ) {
		trap_SendServerCommand( ent - g_entities, "print \"Timeouts are for matches in progress.\n\"" );
		return;
	}
	if ( level.pauseTime ) {
		trap_SendServerCommand( ent - g_entities, "print \"The game is already paused.\n\"" );
		return;
	}

	if ( g_gametype.integer >= GT_TEAM ) {
		used = &level.teamTimeouts[client->sess.sessionTeam];
	} else {
		used = &client->pers.timeouts;
	}
	if ( *used >= g_timeoutCount.integer ) {
		trap_SendServerCommand( ent - g_entities, g_timeoutCount.integer > 0
			? va( "print \"No timeouts left: %i per match.\n\"", g_timeoutCount.integer )
			: "print \"Timeouts are off (g_timeoutCount 0).\n\"" );
		return;
	}

	(*used)++;
	G_Pause( ent - g_entities, g_timeoutLen.integer * 1000 );
	trap_SendServerCommand( -1, va( "print \"%s" S_COLOR_WHITE " called a timeout, %i of %i.\n\"",
		client->pers.netname, *used, g_timeoutCount.integer ) );
	G_LogPrintf( "Timeout: %i: %s\n", (int)( ent - g_entities ), client->pers.netname );
}

/*
================
Cmd_Timein_f

Ends a timeout early: its caller can, or the caller's team, or anyone once the
caller has left. Only the server ends its own pauses.
================
*/
void Cmd_Timein_f( gentity_t *ent ) {
	gentity_t	*caller;

	if ( !level.pauseTime ) {
		trap_SendServerCommand( ent - g_entities, "print \"The game isn't paused.\n\"" );
		return;
	}
	if ( level.pauseClient < 0 ) {
		trap_SendServerCommand( ent - g_entities, "print \"Only the server can end this pause.\n\"" );
		return;
	}

	caller = &g_entities[level.pauseClient];
	if ( ent != caller && caller->inuse && caller->client->pers.connected == CON_CONNECTED
			&& !( g_gametype.integer >= GT_TEAM && OnSameTeam( ent, caller ) ) ) {
		trap_SendServerCommand( ent - g_entities, va( "print \"Only %s" S_COLOR_WHITE " can end this timeout.\n\"",
			caller->client->pers.netname ) );
		return;
	}

	G_Resume();
	G_LogPrintf( "Timein: %i: %s\n", (int)( ent - g_entities ), ent->client->pers.netname );
}

/*
================
Svcmd_Pause_f

pause: pauses the match until unpause
================
*/
void Svcmd_Pause_f( void ) {
	if ( !G_MatchInProgress() ) {
		G_Printf( "Only a match in progress can be paused.\n" );
		return;
	}

	if ( level.pauseTime ) {
		// a timeout becomes a pause; the clients' clocks stopped at pauseTime
		level.pauseEnd = 0;
		level.pauseClient = -1;
		G_SetPauseConfigstring();
	} else {
		G_Pause( -1, 0 );
	}
	trap_SendServerCommand( -1, "print \"The server paused the game.\n\"" );
	G_LogPrintf( "Pause:\n" );
}

/*
================
Svcmd_Unpause_f

unpause: ends a pause or timeout
================
*/
void Svcmd_Unpause_f( void ) {
	if ( !level.pauseTime ) {
		G_Printf( "The game isn't paused.\n" );
		return;
	}
	G_Resume();
}

/*
================
Svcmd_AllReady_f

allready: readies up every player in warmup
================
*/
void Svcmd_AllReady_f( void ) {
	int			i;
	gentity_t	*ent;

	if ( level.warmupTime >= 0 ) {
		G_Printf( "Players only ready up in warmup.\n" );
		return;
	}

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		if ( ent->inuse && ent->client->pers.connected == CON_CONNECTED ) {
			ent->client->pers.ready = qtrue;
		}
	}
	trap_SendServerCommand( -1, "print \"The server readied everyone up.\n\"" );
}

/*
================
G_UpdateMapVote

Counts the end-of-match votes into CS_MAPVOTE
================
*/
static void G_UpdateMapVote( int *votes ) {
	char		s[MAX_STRING_CHARS];
	int			i, choice;
	gclient_t	*cl;

	for ( i = 0 ; i < level.mapVoteCount ; i++ ) {
		votes[i] = 0;
	}
	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		choice = cl->pers.mapVote;
		if ( cl->pers.connected == CON_CONNECTED && choice >= 1 && choice <= level.mapVoteCount ) {
			votes[choice - 1]++;
		}
	}

	Com_sprintf( s, sizeof( s ), "%i", level.mapVoteEnd );
	for ( i = 0 ; i < level.mapVoteCount ; i++ ) {
		Q_strcat( s, sizeof( s ), va( " %s %s %i", level.mapVoteChoices[i].map,
			level.mapVoteChoices[i].factory, votes[i] ) );
	}
	trap_SetConfigstring( CS_MAPVOTE, s );
}

/*
================
G_StartMapVote

At the end of a match, as in Quake Live, players vote between three maps from
the map pool for the next one. Without two maps to choose from, nextmap stays.
================
*/
void G_StartMapVote( void ) {
	int		votes[MAX_MAPVOTE_CHOICES];

	if ( g_gametype.integer == GT_SINGLE_PLAYER ) {
		return;
	}

	level.mapVoteCount = G_PickMapPoolEntries( level.mapVoteChoices, MAX_MAPVOTE_CHOICES );
	if ( level.mapVoteCount < 2 ) {
		level.mapVoteCount = 0;
		return;
	}

	level.mapVoteEnd = level.time + MAPVOTE_MSEC;
	G_UpdateMapVote( votes );
}

/*
================
Cmd_MapVote_f

vote <1-3> during the end-of-match vote, as Quake Live's end scoreboard sends
================
*/
void Cmd_MapVote_f( gentity_t *ent ) {
	char	arg[MAX_TOKEN_CHARS];
	int		choice, votes[MAX_MAPVOTE_CHOICES];

	trap_Argv( 1, arg, sizeof( arg ) );
	choice = atoi( arg );
	if ( choice < 1 || choice > level.mapVoteCount ) {
		trap_SendServerCommand( ent - g_entities, va( "print \"Vote for the next map with vote 1 to vote %i.\n\"",
			level.mapVoteCount ) );
		return;
	}

	ent->client->pers.mapVote = choice;
	G_UpdateMapVote( votes );
}

/*
================
G_MapVoteOver

The vote ends when its time is up, or once every player has voted or readied
up, after at least the five seconds an intermission always lasts
================
*/
qboolean G_MapVoteOver( void ) {
	int			i;
	gclient_t	*cl;

	if ( level.time >= level.mapVoteEnd ) {
		return qtrue;
	}
	if ( level.time < level.intermissiontime + 5000 ) {
		return qfalse;
	}

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected != CON_CONNECTED || ( g_entities[i].r.svFlags & SVF_BOT ) ) {
			continue;
		}
		if ( !cl->pers.mapVote && !cl->readyToExit ) {
			return qfalse;
		}
	}
	return qtrue;
}

/*
================
G_EndMapVote

The map with the most votes is next; a tie goes to one of the tied at random
================
*/
void G_EndMapVote( void ) {
	int			votes[MAX_MAPVOTE_CHOICES];
	int			i, best, tied, winner;
	const char	*cmd;

	G_UpdateMapVote( votes );

	best = 0;
	for ( i = 1 ; i < level.mapVoteCount ; i++ ) {
		if ( votes[i] > votes[best] ) {
			best = i;
		}
	}
	tied = 0;
	winner = best;
	for ( i = 0 ; i < level.mapVoteCount ; i++ ) {
		if ( votes[i] == votes[best] && rand() % ++tied == 0 ) {
			winner = i;
		}
	}

	cmd = G_MapPoolCommand( &level.mapVoteChoices[winner] );
	if ( cmd ) {
		trap_Cvar_Set( "nextmap", cmd );
	}
	trap_SendServerCommand( -1, va( "print \"Next map: %s (%s), %i of %i votes.\n\"",
		level.mapVoteChoices[winner].map, level.mapVoteChoices[winner].factory, votes[winner],
		votes[0] + votes[1] + ( level.mapVoteCount > 2 ? votes[2] : 0 ) ) );
	G_LogPrintf( "MapVote: %s %s %i\n", level.mapVoteChoices[winner].map,
		level.mapVoteChoices[winner].factory, votes[winner] );
	level.mapVoteEnd = 0;
}

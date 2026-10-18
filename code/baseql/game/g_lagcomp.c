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
// g_lagcomp.c -- baseql: lag compensation for hitscan weapons, as Quake
// Live's g_lagHaxMs and g_lagHaxHistory
//
// A client draws the other players where they were a little while ago:
// snapshots are already old when they arrive, and the client draws between
// the last two. Each command it sends carries the server time it was
// drawing (usercmd_t serverTime). When a hitscan weapon fires, the server
// moves the other players back to where they were at that time, so the shot
// hits what the shooter saw, and then puts them back.
//
// The server keeps where each player was at the end of its last frames,
// which is what the snapshots carry, and interpolates between frames as the
// client does. It goes back at most g_lagHaxMs, and no further than the
// oldest of g_lagHaxHistory frames.

#include "g_local.h"

/*
================
G_StoreLagFrames

Remembers where every player is at the end of this frame
================
*/
void G_StoreLagFrames( void ) {
	int				i;
	gentity_t		*ent;
	gclient_t		*client;
	lagCompFrame_t	*frame;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		client = ent->client;
		if ( !ent->inuse || client->pers.connected != CON_CONNECTED
				|| client->sess.sessionTeam == TEAM_SPECTATOR ) {
			continue;
		}

		client->lagNewest = ( client->lagNewest + 1 ) % MAX_LAGCOMP_FRAMES;
		if ( client->lagCount < MAX_LAGCOMP_FRAMES ) {
			client->lagCount++;
		}

		frame = &client->lagFrames[client->lagNewest];
		frame->time = level.time;
		VectorCopy( ent->s.pos.trBase, frame->origin );		// snapped, as clients draw it
		VectorCopy( ent->r.mins, frame->mins );
		VectorCopy( ent->r.maxs, frame->maxs );
		frame->teleportBit = client->ps.eFlags & EF_TELEPORT_BIT;
	}
}

/*
================
G_LagFrameAt

Where a player was at a server time, between the frames around it
================
*/
static void G_LagFrameAt( gclient_t *client, int time, lagCompFrame_t *out ) {
	int				frames, i;
	float			frac;
	vec3_t			delta;
	lagCompFrame_t	*newer, *older;

	frames = g_lagHaxHistory.integer;
	if ( frames > client->lagCount ) {
		frames = client->lagCount;
	}

	newer = &client->lagFrames[client->lagNewest];
	if ( time >= newer->time ) {
		*out = *newer;
		return;
	}

	for ( i = 1 ; i < frames ; i++ ) {
		older = &client->lagFrames[( client->lagNewest - i + MAX_LAGCOMP_FRAMES ) % MAX_LAGCOMP_FRAMES];
		if ( older->time <= time ) {
			*out = *older;
			// clients don't interpolate across a teleport either
			if ( older->teleportBit == newer->teleportBit && newer->time > older->time ) {
				frac = (float)( time - older->time ) / ( newer->time - older->time );
				out->time = time;
				VectorSubtract( newer->origin, older->origin, delta );
				VectorMA( older->origin, frac, delta, out->origin );
			}
			return;
		}
		newer = older;
	}

	// further back than the history goes
	*out = *newer;
}

/*
================
G_RewindClients

Moves the other players back to where the shooter saw them, until
G_RestoreClients
================
*/
void G_RewindClients( gentity_t *shooter ) {
	int				time, i;
	gentity_t		*ent;
	gclient_t		*client;
	lagCompFrame_t	frame;

	if ( g_lagHaxMs.integer <= 0 || !shooter->client ) {
		return;
	}

	// the server time the shooter was drawing, as far back as allowed
	time = shooter->client->pers.cmd.serverTime;
	if ( time < level.time - g_lagHaxMs.integer ) {
		time = level.time - g_lagHaxMs.integer;
	}

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		client = ent->client;
		if ( ent == shooter || !ent->inuse || !ent->r.linked || client->pers.connected != CON_CONNECTED
				|| client->sess.sessionTeam == TEAM_SPECTATOR || client->ps.stats[STAT_HEALTH] <= 0
				|| !client->lagCount ) {
			continue;
		}

		G_LagFrameAt( client, time, &frame );

		if ( g_debugLagComp.integer ) {
			G_Printf( "lagcomp: %i fired at %i, %i ms back: %i from %.1f %.1f %.1f to %.1f %.1f %.1f\n",
				shooter->s.number, time, level.time - time, i,
				ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
				frame.origin[0], frame.origin[1], frame.origin[2] );
		}

		VectorCopy( ent->r.currentOrigin, client->lagSavedOrigin );
		VectorCopy( ent->r.mins, client->lagSavedMins );
		VectorCopy( ent->r.maxs, client->lagSavedMaxs );
		client->lagShifted = qtrue;

		VectorCopy( frame.origin, ent->r.currentOrigin );
		VectorCopy( frame.mins, ent->r.mins );
		VectorCopy( frame.maxs, ent->r.maxs );
		trap_LinkEntity( ent );
	}
}

/*
================
G_RestoreClients

Puts the players G_RewindClients moved back where they are
================
*/
void G_RestoreClients( void ) {
	int			i;
	gentity_t	*ent;
	gclient_t	*client;

	for ( i = 0 ; i < level.maxclients ; i++ ) {
		ent = &g_entities[i];
		client = ent->client;
		if ( !ent->inuse || !client->lagShifted ) {
			continue;
		}

		VectorCopy( client->lagSavedOrigin, ent->r.currentOrigin );
		VectorCopy( client->lagSavedMins, ent->r.mins );
		VectorCopy( client->lagSavedMaxs, ent->r.maxs );
		client->lagShifted = qfalse;
		trap_LinkEntity( ent );
	}
}

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
// g_factory.c -- baseql: Quake Live style factories and map pool
//
// A factory is a named gametype preset: a base gametype plus cvar values,
// loaded from scripts/factories.txt and scripts/*.factories (JSON, in Quake
// Live's format). g_factory selects one; it is applied on each full map load,
// after resetting the cvars the previous factory set, so settings don't leak
// from one match into the next. The map pool (sv_mapPoolFile) lists
// "map|factory" pairs that nextmap and startRandomMap choose from.

#include "g_local.h"

#define MAX_FACTORIES			128
#define FACTORY_POOL_SIZE		( 64 * 1024 )
#define FACTORY_FILE_SIZE		( 64 * 1024 )
#define MAX_FACTORY_FILES		64
#define MAX_MAPPOOL				256

typedef struct {
	char		id[MAX_QPATH];
	char		title[MAX_QPATH];
	char		author[MAX_QPATH];
	char		description[256];
	int			gametype;
	int			cvarOffset;		// into factoryPool, as name\0value\0 pairs
	int			numCvars;
} factory_t;

static factory_t		factories[MAX_FACTORIES];
static int				numFactories;
static char				factoryPool[FACTORY_POOL_SIZE];
static int				factoryPoolUsed;
static char				factoryFileBuffer[FACTORY_FILE_SIZE];

static mapPoolEntry_t	mapPool[MAX_MAPPOOL];
static int				mapPoolSize;

typedef struct {
	char		*name;
	int			gametype;
} baseGametype_t;

// Quake Live's basegt names for the gametypes baseql has
static baseGametype_t	baseGametypes[] = {
	{ "ffa",		GT_FFA },
	{ "duel",		GT_TOURNAMENT },
	{ "tdm",		GT_TEAM },
	{ "ctf",		GT_CTF },
#ifdef MISSIONPACK
	{ "oneflag",	GT_1FCTF },
	{ "har",		GT_HARVESTER },
#endif
};

// Quake Live gametypes that baseql doesn't have yet
static char		*unsupportedGametypes[] = { "race", "ca", "ft", "dom", "ad", "rr", NULL };

/*
================
G_IsSafeName

Cvar names, factory ids and map names end up in console commands, so they
may only contain characters that can't break a command apart.
================
*/
static qboolean G_IsSafeName( const char *s ) {
	if ( !*s ) {
		return qfalse;
	}
	for ( ; *s ; s++ ) {
		if ( !( ( *s >= 'a' && *s <= 'z' ) || ( *s >= 'A' && *s <= 'Z' ) || ( *s >= '0' && *s <= '9' )
				|| *s == '_' || *s == '-' || *s == '.' || *s == '/' ) ) {
			return qfalse;
		}
	}
	return qtrue;
}

static const char *G_GametypeName( int gametype ) {
	int		i;

	for ( i = 0 ; i < ARRAY_LEN( baseGametypes ) ; i++ ) {
		if ( baseGametypes[i].gametype == gametype ) {
			return baseGametypes[i].name;
		}
	}
	return "?";
}

static factory_t *G_FindFactory( const char *id ) {
	int		i;

	for ( i = 0 ; i < numFactories ; i++ ) {
		if ( !Q_stricmp( factories[i].id, id ) ) {
			return &factories[i];
		}
	}
	return NULL;
}

static qboolean G_FactoryPoolAdd( json_t *js, const char *s ) {
	int		len;

	len = strlen( s ) + 1;
	if ( factoryPoolUsed + len > FACTORY_POOL_SIZE ) {
		return JSON_Fail( js, "out of memory for factory cvars" );
	}
	memcpy( factoryPool + factoryPoolUsed, s, len );
	factoryPoolUsed += len;
	return qtrue;
}

static qboolean G_ReadFactoryString( json_t *js, const char *key, char *out, int size ) {
	if ( JSON_Peek( js ) != '"' ) {
		return JSON_Fail( js, va( "\"%s\" must be a string", key ) );
	}
	return JSON_ReadString( js, out, size );
}

/*
================
G_ParseFactory

Parses one factory object. Returns qfalse on a syntax error, which ends the
file. A factory that parses but isn't valid sets reason and is skipped.
================
*/
static qboolean G_ParseFactory( json_t *js, factory_t *f, char *reason, int reasonSize ) {
	char		key[MAX_QPATH];
	char		basegt[MAX_QPATH];
	char		name[MAX_QPATH];
	char		value[MAX_STRING_CHARS];
	qboolean	hasId, hasTitle, hasAuthor, hasDescription, hasBasegt, hasCvars;
	int			i;

	memset( f, 0, sizeof( *f ) );
	f->cvarOffset = factoryPoolUsed;
	hasId = hasTitle = hasAuthor = hasDescription = hasBasegt = hasCvars = qfalse;
	reason[0] = '\0';

	if ( JSON_Peek( js ) != '{' ) {
		return JSON_Fail( js, "a factory must be an object" );
	}
	JSON_Expect( js, '{' );
	if ( !JSON_Accept( js, '}' ) ) {
		do {
			if ( !JSON_ReadString( js, key, sizeof( key ) ) || !JSON_Expect( js, ':' ) ) {
				return qfalse;
			}
			if ( !Q_stricmp( key, "id" ) ) {
				hasId = G_ReadFactoryString( js, key, f->id, sizeof( f->id ) );
			} else if ( !Q_stricmp( key, "title" ) ) {
				hasTitle = G_ReadFactoryString( js, key, f->title, sizeof( f->title ) );
			} else if ( !Q_stricmp( key, "author" ) ) {
				hasAuthor = G_ReadFactoryString( js, key, f->author, sizeof( f->author ) );
			} else if ( !Q_stricmp( key, "description" ) ) {
				hasDescription = G_ReadFactoryString( js, key, f->description, sizeof( f->description ) );
			} else if ( !Q_stricmp( key, "basegt" ) ) {
				hasBasegt = G_ReadFactoryString( js, key, basegt, sizeof( basegt ) );
			} else if ( !Q_stricmp( key, "cvars" ) ) {
				if ( JSON_Peek( js ) != '{' ) {
					return JSON_Fail( js, "\"cvars\" must be an object" );
				}
				JSON_Expect( js, '{' );
				hasCvars = qtrue;
				if ( !JSON_Accept( js, '}' ) ) {
					do {
						if ( !JSON_ReadString( js, name, sizeof( name ) ) || !JSON_Expect( js, ':' )
								|| !JSON_ReadScalar( js, value, sizeof( value ) ) ) {
							return qfalse;
						}
						if ( !G_IsSafeName( name ) ) {
							Com_sprintf( reason, reasonSize, "bad cvar name \"%s\"", name );
						} else if ( !Q_stricmp( name, "g_gametype" ) ) {
							Com_sprintf( reason, reasonSize, "sets g_gametype; use \"basegt\"" );
						} else if ( !G_FactoryPoolAdd( js, name ) || !G_FactoryPoolAdd( js, value ) ) {
							return qfalse;
						}
						f->numCvars++;
					} while ( JSON_Accept( js, ',' ) );
					if ( !JSON_Expect( js, '}' ) ) {
						return qfalse;
					}
				}
			} else if ( !JSON_SkipValue( js ) ) {
				return qfalse;
			}
		} while ( JSON_Accept( js, ',' ) );
		if ( !JSON_Expect( js, '}' ) ) {
			return qfalse;
		}
	}

	if ( reason[0] ) {
		return qtrue;
	}
	if ( !hasId || !hasTitle || !hasAuthor || !hasDescription || !hasBasegt || !hasCvars ) {
		Com_sprintf( reason, reasonSize, "missing \"%s\"", !hasId ? "id" : !hasTitle ? "title" : !hasAuthor ? "author"
			: !hasDescription ? "description" : !hasBasegt ? "basegt" : "cvars" );
		return qtrue;
	}
	if ( !G_IsSafeName( f->id ) ) {
		Com_sprintf( reason, reasonSize, "bad id" );
		return qtrue;
	}
	if ( G_FindFactory( f->id ) ) {
		Com_sprintf( reason, reasonSize, "already defined" );
		return qtrue;
	}

	f->gametype = -1;
	for ( i = 0 ; i < ARRAY_LEN( baseGametypes ) ; i++ ) {
		if ( !Q_stricmp( basegt, baseGametypes[i].name ) ) {
			f->gametype = baseGametypes[i].gametype;
		}
	}
	if ( f->gametype < 0 ) {
		for ( i = 0 ; unsupportedGametypes[i] ; i++ ) {
			if ( !Q_stricmp( basegt, unsupportedGametypes[i] ) ) {
				Com_sprintf( reason, reasonSize, "basegt \"%s\" isn't supported yet", basegt );
				return qtrue;
			}
		}
		Com_sprintf( reason, reasonSize, "unknown basegt \"%s\"", basegt );
	}
	return qtrue;
}

/*
================
G_LoadFactoryFile

A factory file holds one factory object or an array of them.
================
*/
static void G_LoadFactoryFile( const char *path ) {
	fileHandle_t	fh;
	int				len;
	json_t			js;
	factory_t		*f;
	char			reason[MAX_STRING_CHARS];
	qboolean		isArray;
	int				loaded, poolMark;

	len = trap_FS_FOpenFile( path, &fh, FS_READ );
	if ( !fh ) {
		return;
	}
	if ( len >= FACTORY_FILE_SIZE ) {
		G_Printf( S_COLOR_YELLOW "%s: too large, max is %i bytes\n", path, FACTORY_FILE_SIZE - 1 );
		trap_FS_FCloseFile( fh );
		return;
	}
	trap_FS_Read( factoryFileBuffer, len, fh );
	factoryFileBuffer[len] = '\0';
	trap_FS_FCloseFile( fh );

	JSON_Init( &js, factoryFileBuffer );
	isArray = JSON_Accept( &js, '[' );
	loaded = 0;
	if ( !isArray || !JSON_Accept( &js, ']' ) ) {
		do {
			if ( numFactories >= MAX_FACTORIES ) {
				G_Printf( S_COLOR_YELLOW "%s: more than %i factories, ignoring the rest\n", path, MAX_FACTORIES );
				break;
			}
			poolMark = factoryPoolUsed;
			f = &factories[numFactories];
			if ( !G_ParseFactory( &js, f, reason, sizeof( reason ) ) ) {
				factoryPoolUsed = poolMark;
				break;
			}
			if ( reason[0] ) {
				G_Printf( S_COLOR_YELLOW "%s: factory \"%s\" skipped: %s\n", path, f->id, reason );
				factoryPoolUsed = poolMark;
				continue;
			}
			numFactories++;
			loaded++;
		} while ( isArray && JSON_Accept( &js, ',' ) );
		if ( isArray ) {
			JSON_Expect( &js, ']' );
		}
		if ( JSON_Peek( &js ) ) {
			JSON_Fail( &js, "unexpected data after the factories" );
		}
	}

	if ( !JSON_Ok( &js ) ) {
		G_Printf( S_COLOR_YELLOW "%s: %s; loaded %i factories before the error\n", path, js.error, loaded );
		return;
	}
	G_Printf( "loaded factories from %s, total %i\n", path, loaded );
}

/*
================
G_LoadFactories
================
*/
static void G_LoadFactories( void ) {
	char	list[4096];
	char	path[MAX_QPATH];
	char	*name;
	int		count, i;

	numFactories = 0;
	factoryPoolUsed = 0;

	G_LoadFactoryFile( "scripts/factories.txt" );

	count = trap_FS_GetFileList( "scripts", ".factories", list, sizeof( list ) );
	if ( count > MAX_FACTORY_FILES ) {
		count = MAX_FACTORY_FILES;
	}
	for ( i = 0, name = list ; i < count ; i++, name += strlen( name ) + 1 ) {
		Com_sprintf( path, sizeof( path ), "scripts/%s", name );
		G_LoadFactoryFile( path );
	}
}

static void G_ResetFactoryCvars( const factory_t *f ) {
	const char	*name;
	int			i;

	name = factoryPool + f->cvarOffset;
	for ( i = 0 ; i < f->numCvars ; i++ ) {
		trap_SendConsoleCommand( EXEC_NOW, va( "reset %s\n", name ) );
		name += strlen( name ) + 1;		// skip the name
		name += strlen( name ) + 1;		// and the value
	}
}

static void G_SetFactoryCvars( const factory_t *f ) {
	const char	*name, *value;
	int			i;

	name = factoryPool + f->cvarOffset;
	for ( i = 0 ; i < f->numCvars ; i++ ) {
		value = name + strlen( name ) + 1;
		trap_Cvar_Set( name, value );
		name = value + strlen( value ) + 1;
	}
}

/*
================
G_MapExists
================
*/
static qboolean G_MapExists( const char *map ) {
	fileHandle_t	fh;

	trap_FS_FOpenFile( va( "maps/%s.bsp", map ), &fh, FS_READ );
	if ( !fh ) {
		return qfalse;
	}
	trap_FS_FCloseFile( fh );
	return qtrue;
}

/*
================
G_LoadMapPool

Reads sv_mapPoolFile: one "map|factory" per line, # starts a comment.
================
*/
static void G_LoadMapPool( void ) {
	char			path[MAX_QPATH];
	fileHandle_t	fh;
	int				len, lineNum;
	char			*line, *next, *bar, *s;
	mapPoolEntry_t	*e;

	mapPoolSize = 0;

	trap_Cvar_VariableStringBuffer( "sv_mapPoolFile", path, sizeof( path ) );
	if ( !path[0] ) {
		return;
	}
	len = trap_FS_FOpenFile( path, &fh, FS_READ );
	if ( !fh ) {
		G_Printf( "no map pool: %s not found\n", path );
		return;
	}
	if ( len >= FACTORY_FILE_SIZE ) {
		G_Printf( S_COLOR_YELLOW "%s: too large, max is %i bytes\n", path, FACTORY_FILE_SIZE - 1 );
		trap_FS_FCloseFile( fh );
		return;
	}
	trap_FS_Read( factoryFileBuffer, len, fh );
	factoryFileBuffer[len] = '\0';
	trap_FS_FCloseFile( fh );

	for ( line = factoryFileBuffer, lineNum = 1 ; line ; line = next, lineNum++ ) {
		next = strchr( line, '\n' );
		if ( next ) {
			*next++ = '\0';
		}
		if ( ( s = strchr( line, '#' ) ) != NULL ) {
			*s = '\0';
		}
		// trim whitespace
		while ( *line == ' ' || *line == '\t' ) {
			line++;
		}
		for ( s = line + strlen( line ) ; s > line && ( s[-1] == ' ' || s[-1] == '\t' || s[-1] == '\r' ) ; s-- ) {
			s[-1] = '\0';
		}
		if ( !*line ) {
			continue;
		}

		bar = strchr( line, '|' );
		if ( !bar ) {
			G_Printf( S_COLOR_YELLOW "%s:%i: expected map|factory\n", path, lineNum );
			continue;
		}
		*bar++ = '\0';
		if ( !G_IsSafeName( line ) || !G_IsSafeName( bar ) ) {
			G_Printf( S_COLOR_YELLOW "%s:%i: bad map or factory name\n", path, lineNum );
			continue;
		}
		if ( !G_FindFactory( bar ) ) {
			G_Printf( S_COLOR_YELLOW "%s:%i: unknown factory \"%s\"\n", path, lineNum, bar );
			continue;
		}
		if ( !G_MapExists( line ) ) {
			G_Printf( S_COLOR_YELLOW "%s:%i: map \"%s\" not found\n", path, lineNum, line );
			continue;
		}
		if ( mapPoolSize >= MAX_MAPPOOL ) {
			G_Printf( S_COLOR_YELLOW "%s: more than %i entries, ignoring the rest\n", path, MAX_MAPPOOL );
			break;
		}
		e = &mapPool[mapPoolSize++];
		Q_strncpyz( e->map, line, sizeof( e->map ) );
		Q_strncpyz( e->factory, bar, sizeof( e->factory ) );
	}

	G_Printf( "loaded map pool from %s, total %i\n", path, mapPoolSize );
}

/*
================
G_MapChangeCommand

Command that changes to a map pool entry. It doesn't rely on the engine
taking the factory as the map command's second argument.
================
*/
static const char *G_MapChangeCommand( const mapPoolEntry_t *e ) {
	if ( !G_FindFactory( e->factory ) ) {
		return NULL;
	}
	return va( "set g_factory %s; map %s", e->factory, e->map );
}

/*
================
G_PickFromMapPool

A random map pool entry, avoiding the current map and factory if possible.
================
*/
static const mapPoolEntry_t *G_PickFromMapPool( void ) {
	char	map[MAX_QPATH], factory[MAX_QPATH];
	int		i, start;

	if ( !mapPoolSize ) {
		return NULL;
	}
	trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
	trap_Cvar_VariableStringBuffer( "g_factory", factory, sizeof( factory ) );

	start = rand() % mapPoolSize;
	for ( i = 0 ; i < mapPoolSize ; i++ ) {
		const mapPoolEntry_t *e = &mapPool[( start + i ) % mapPoolSize];
		if ( Q_stricmp( e->map, map ) || Q_stricmp( e->factory, factory ) ) {
			return e;
		}
	}
	return &mapPool[start];
}

/*
================
G_SetNextMapFromPool
================
*/
static void G_SetNextMapFromPool( void ) {
	const mapPoolEntry_t	*e;
	const char				*cmd;

	e = G_PickFromMapPool();
	if ( e && ( cmd = G_MapChangeCommand( e ) ) != NULL ) {
		trap_Cvar_Set( "nextmap", cmd );
	}
}

/*
================
G_InitFactories

Called on each full map load, before the game registers its cvars, so the
cvars a factory sets (including g_gametype) are in effect for this map.
================
*/
void G_InitFactories( void ) {
	char					requested[MAX_QPATH], applied[MAX_QPATH];
	factory_t				*f, *previous;
	int						gametype;

	trap_Cvar_Register( NULL, "g_factory", "", CVAR_SERVERINFO );
	trap_Cvar_Register( NULL, "g_factoryTitle", "", CVAR_SERVERINFO );
	trap_Cvar_Register( NULL, "g_factoryApplied", "", 0 );
	trap_Cvar_Register( NULL, "sv_mapPoolFile", "mappool.txt", CVAR_ARCHIVE );

	G_LoadFactories();
	G_LoadMapPool();

	gametype = trap_Cvar_VariableIntegerValue( "g_gametype" );
	if ( gametype == GT_SINGLE_PLAYER ) {
		return;
	}

	trap_Cvar_VariableStringBuffer( "g_factory", requested, sizeof( requested ) );
	trap_Cvar_VariableStringBuffer( "g_factoryApplied", applied, sizeof( applied ) );

	previous = applied[0] ? G_FindFactory( applied ) : NULL;
	f = requested[0] ? G_FindFactory( requested ) : NULL;
	if ( requested[0] && !f ) {
		G_Printf( S_COLOR_YELLOW "unknown factory \"%s\"%s\n", requested,
			previous ? va( ", keeping \"%s\"", previous->id ) : "" );
		f = previous;
	}

	// undo the previous factory, so its settings don't leak into this match
	if ( previous ) {
		G_ResetFactoryCvars( previous );
	}

	if ( !f ) {
		trap_Cvar_Set( "g_factory", "" );
		trap_Cvar_Set( "g_factoryTitle", "" );
		trap_Cvar_Set( "g_factoryApplied", "" );
	} else {
		G_SetFactoryCvars( f );
		trap_Cvar_Set( "g_factory", f->id );
		trap_Cvar_Set( "g_factoryTitle", f->title );
		trap_Cvar_Set( "g_factoryApplied", f->id );
		// g_gametype is latched, so this only sets the latched value, but the
		// engine applies it when G_RegisterCvars registers g_gametype next
		trap_Cvar_Set( "g_gametype", va( "%i", f->gametype ) );
		G_Printf( "factory: %s (%s, %i cvars)\n", f->id, G_GametypeName( f->gametype ), f->numCvars );
	}

	// classic nextmap rotation scripts set nextmap after this, and still win
	G_SetNextMapFromPool();
}

/*
================
G_MapPoolCommand

baseql: the command that changes to a map pool entry, NULL if its factory
isn't known
================
*/
const char *G_MapPoolCommand( const mapPoolEntry_t *e ) {
	if ( !numFactories ) {
		G_LoadFactories();
	}
	return G_MapChangeCommand( e );
}

/*
================
G_PickMapPoolEntries

baseql: up to max different map pool entries at random, for the end-of-match
vote; the current map is left out unless sv_includeCurrentMapInVote is set.
A map_restart restarts the game module, so the pool may need reading again.
================
*/
int G_PickMapPoolEntries( mapPoolEntry_t *out, int max ) {
	char	map[MAX_QPATH];
	int		candidates[MAX_MAPPOOL];
	int		i, n, count;

	if ( !numFactories ) {
		G_LoadFactories();
	}
	if ( !mapPoolSize ) {
		G_LoadMapPool();
	}

	trap_Cvar_VariableStringBuffer( "mapname", map, sizeof( map ) );
	n = 0;
	for ( i = 0 ; i < mapPoolSize ; i++ ) {
		if ( !sv_includeCurrentMapInVote.integer && !Q_stricmp( mapPool[i].map, map ) ) {
			continue;
		}
		if ( G_FindFactory( mapPool[i].factory ) ) {
			candidates[n++] = i;
		}
	}

	for ( count = 0 ; count < max && n > 0 ; count++ ) {
		i = rand() % n;
		out[count] = mapPool[candidates[i]];
		candidates[i] = candidates[--n];
	}
	return count;
}

/*
================
Svcmd_ReloadFactories_f
================
*/
void Svcmd_ReloadFactories_f( void ) {
	G_LoadFactories();
	G_LoadMapPool();
	G_SetNextMapFromPool();
}

/*
================
Svcmd_ReloadMapPool_f
================
*/
void Svcmd_ReloadMapPool_f( void ) {
	G_LoadMapPool();
	G_SetNextMapFromPool();
}

/*
================
Svcmd_StartRandomMap_f
================
*/
void Svcmd_StartRandomMap_f( void ) {
	const mapPoolEntry_t	*e;
	const char				*cmd;

	e = G_PickFromMapPool();
	if ( !e || ( cmd = G_MapChangeCommand( e ) ) == NULL ) {
		G_Printf( "the map pool is empty\n" );
		return;
	}
	trap_SendConsoleCommand( EXEC_APPEND, va( "%s\n", cmd ) );
}

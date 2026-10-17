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
// g_json.c -- baseql: minimal validating JSON reader, for factory files
//
// The caller walks the document with these functions. After the first error
// every function fails, so callers can check JSON_Ok() once at the end.

#include "g_local.h"

#define JSON_MAX_DEPTH	32

void JSON_Init( json_t *js, const char *text ) {
	// skip a UTF-8 byte order mark
	if ( text[0] == '\xef' && text[1] == '\xbb' && text[2] == '\xbf' ) {
		text += 3;
	}
	js->text = text;
	js->p = text;
	js->error[0] = '\0';
}

qboolean JSON_Ok( const json_t *js ) {
	return !js->error[0];
}

int JSON_Line( const json_t *js ) {
	const char	*s;
	int			line;

	line = 1;
	for ( s = js->text ; s < js->p ; s++ ) {
		if ( *s == '\n' ) {
			line++;
		}
	}
	return line;
}

qboolean JSON_Fail( json_t *js, const char *message ) {
	if ( !js->error[0] ) {
		Com_sprintf( js->error, sizeof( js->error ), "line %i: %s", JSON_Line( js ), message );
	}
	return qfalse;
}

// returns the next character after whitespace, or 0 at the end or after an error
char JSON_Peek( json_t *js ) {
	if ( js->error[0] ) {
		return 0;
	}
	while ( *js->p == ' ' || *js->p == '\t' || *js->p == '\n' || *js->p == '\r' ) {
		js->p++;
	}
	return *js->p;
}

// consumes c if it is next
qboolean JSON_Accept( json_t *js, char c ) {
	if ( JSON_Peek( js ) != c || !c ) {
		return qfalse;
	}
	js->p++;
	return qtrue;
}

qboolean JSON_Expect( json_t *js, char c ) {
	if ( JSON_Accept( js, c ) ) {
		return qtrue;
	}
	return JSON_Fail( js, va( "expected '%c'", c ) );
}

static int JSON_HexDigit( char c ) {
	if ( c >= '0' && c <= '9' ) {
		return c - '0';
	}
	if ( c >= 'a' && c <= 'f' ) {
		return c - 'a' + 10;
	}
	if ( c >= 'A' && c <= 'F' ) {
		return c - 'A' + 10;
	}
	return -1;
}

/*
================
JSON_ReadString

Reads a string value into out, decoding escapes. Characters outside ASCII
become '?'. Fails if the string doesn't fit. With out NULL, skips the string.
================
*/
qboolean JSON_ReadString( json_t *js, char *out, int size ) {
	int		len;
	int		i, digit, code;
	char	c;

	if ( !JSON_Expect( js, '"' ) ) {
		return qfalse;
	}

	len = 0;
	while ( 1 ) {
		c = *js->p;
		if ( !c || c == '\n' ) {
			return JSON_Fail( js, "unterminated string" );
		}
		js->p++;
		if ( c == '"' ) {
			break;
		}
		if ( c == '\\' ) {
			c = *js->p++;
			switch ( c ) {
			case '"': case '\\': case '/':
				break;
			case 'b': c = '\b'; break;
			case 'f': c = '\f'; break;
			case 'n': c = '\n'; break;
			case 'r': c = '\r'; break;
			case 't': c = '\t'; break;
			case 'u':
				code = 0;
				for ( i = 0 ; i < 4 ; i++ ) {
					digit = JSON_HexDigit( *js->p );
					if ( digit < 0 ) {
						return JSON_Fail( js, "bad \\u escape" );
					}
					code = code * 16 + digit;
					js->p++;
				}
				c = ( code > 0 && code < 128 ) ? (char)code : '?';
				break;
			default:
				return JSON_Fail( js, "bad escape in string" );
			}
		} else if ( c & 0x80 ) {
			// skip the rest of a UTF-8 sequence
			while ( ( *js->p & 0xc0 ) == 0x80 ) {
				js->p++;
			}
			c = '?';
		}
		if ( out ) {
			if ( len >= size - 1 ) {
				return JSON_Fail( js, "string too long" );
			}
			out[len++] = c;
		}
	}
	if ( out ) {
		out[len] = '\0';
	}
	return qtrue;
}

/*
================
JSON_ReadScalar

Reads a string, number, true, false or null as text. With out NULL, skips it.
================
*/
qboolean JSON_ReadScalar( json_t *js, char *out, int size ) {
	const char	*start;
	int			len;
	char		c;

	c = JSON_Peek( js );
	if ( c == '"' ) {
		return JSON_ReadString( js, out, size );
	}
	if ( c == '{' || c == '[' || !c ) {
		return JSON_Fail( js, "expected a string or number" );
	}

	start = js->p;
	while ( ( *js->p >= '0' && *js->p <= '9' ) || ( *js->p >= 'a' && *js->p <= 'z' )
			|| *js->p == '-' || *js->p == '+' || *js->p == '.' || *js->p == 'E' ) {
		js->p++;
	}
	len = js->p - start;
	if ( !len ) {
		return JSON_Fail( js, va( "unexpected '%c'", c ) );
	}
	if ( ( start[0] < '0' || start[0] > '9' ) && start[0] != '-'
			&& !( len == 4 && !Q_strncmp( start, "true", 4 ) )
			&& !( len == 5 && !Q_strncmp( start, "false", 5 ) )
			&& !( len == 4 && !Q_strncmp( start, "null", 4 ) ) ) {
		return JSON_Fail( js, "unexpected value" );
	}
	if ( out ) {
		if ( len >= size ) {
			return JSON_Fail( js, "value too long" );
		}
		memcpy( out, start, len );
		out[len] = '\0';
	}
	return qtrue;
}

static qboolean JSON_SkipValueDepth( json_t *js, int depth ) {
	char	c;

	if ( depth > JSON_MAX_DEPTH ) {
		return JSON_Fail( js, "nested too deeply" );
	}

	c = JSON_Peek( js );
	if ( c == '{' ) {
		js->p++;
		if ( JSON_Accept( js, '}' ) ) {
			return qtrue;
		}
		do {
			if ( !JSON_ReadString( js, NULL, 0 ) || !JSON_Expect( js, ':' )
					|| !JSON_SkipValueDepth( js, depth + 1 ) ) {
				return qfalse;
			}
		} while ( JSON_Accept( js, ',' ) );
		return JSON_Expect( js, '}' );
	}
	if ( c == '[' ) {
		js->p++;
		if ( JSON_Accept( js, ']' ) ) {
			return qtrue;
		}
		do {
			if ( !JSON_SkipValueDepth( js, depth + 1 ) ) {
				return qfalse;
			}
		} while ( JSON_Accept( js, ',' ) );
		return JSON_Expect( js, ']' );
	}
	return JSON_ReadScalar( js, NULL, 0 );
}

qboolean JSON_SkipValue( json_t *js ) {
	return JSON_SkipValueDepth( js, 0 );
}

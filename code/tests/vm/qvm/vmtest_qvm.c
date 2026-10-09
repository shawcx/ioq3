/*
===========================================================================
Copyright (C) 2026 Matthew Shaw

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
// vmtest_qvm.c -- QVM side of the vmtest harness, built with q3lcc
//
// Each test logs its results through trap_Log so the harness can compare
// the interpreter with the bytecode compiler value by value. The operands
// live in globals so q3lcc can't fold the operations away.

void	trap_Log( int value );
void	trap_LogFloat( float value );
void	trap_Error( const char *message );
int	trap_Sum15( int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8,
	int a9, int a10, int a11, int a12, int a13, int a14, int a15 );
int	trap_Reenter( int value );
void	trap_LogString( const char *s );

// commands, see the enum in vmtest.c
#define VMTEST_SUITE		0
#define VMTEST_REENTER		1
#define VMTEST_BAD_CALL		2
#define VMTEST_RECURSE_FOREVER	3
#define VMTEST_BENCH		4
#define VMTEST_NAN		5

int	RunSuite( void );
int	Reenter( int value );
int	BadCall( void );
int	RecurseForever( int depth );
int	Bench( void );
int	TestNaN( void );

/*
================
vmMain

Must stay the first function so it is instruction 0
================
*/
int vmMain( int command, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5,
	int arg6, int arg7, int arg8, int arg9, int arg10, int arg11 ) {
	switch ( command ) {
	case VMTEST_SUITE:
		return RunSuite();
	case VMTEST_REENTER:
		return Reenter( arg0 );
	case VMTEST_BAD_CALL:
		return BadCall();
	case VMTEST_RECURSE_FOREVER:
		return RecurseForever( 0 );
	case VMTEST_BENCH:
		return Bench();
	case VMTEST_NAN:
		return TestNaN();
	}

	trap_Error( "unknown command" );
	return -1;
}

static int checksum;

static void Log( int value ) {
	trap_Log( value );
	checksum = checksum * 31 + value;
}

static int FloatBits( float f ) {
	return *(int *)&f;
}

static void LogFloat( float f ) {
	trap_LogFloat( f );
	checksum = checksum * 31 + FloatBits( f );
}

/*
================
Integer arithmetic, logic, shifts and comparisons
================
*/

static int intEdge[] = {
	0, 1, -1, 2, -2, 3, 7, -7, 31, 32, 33, 100, -100, 255, 256, 65535, 65536,
	-65536, 0x7fffffff, 0x80000000, 0x12345678, 0xdeadbeef, 0x55555555
};
#define NUM_INT_EDGE	( sizeof( intEdge ) / sizeof( intEdge[0] ) )

static int ga, gb;

static void TestIntegers( void ) {
	int i, j;
	unsigned ua, ub;

	for ( i = 0; i < NUM_INT_EDGE; i++ ) {
		ga = intEdge[i];
		ua = ga;

		Log( -ga );
		Log( ~ga );
		Log( !ga );

		for ( j = 0; j < NUM_INT_EDGE; j++ ) {
			gb = intEdge[j];
			ub = gb;

			Log( ga + gb );
			Log( ga - gb );
			Log( ga * gb );
			Log( (int)( ua * ub ) );

			// skip the cases C leaves undefined, the interpreter traps on
			// them on x86
			if ( gb != 0 && !( ga == (int)0x80000000 && gb == -1 ) ) {
				Log( ga / gb );
				Log( ga % gb );
			}
			if ( ub != 0 ) {
				Log( (int)( ua / ub ) );
				Log( (int)( ua % ub ) );
			}

			Log( ga & gb );
			Log( ga | gb );
			Log( ga ^ gb );
			Log( ga << ( gb & 31 ) );
			Log( ga >> ( gb & 31 ) );
			Log( (int)( ua >> ( gb & 31 ) ) );

			Log( ga == gb );
			Log( ga != gb );
			Log( ga < gb );
			Log( ga <= gb );
			Log( ga > gb );
			Log( ga >= gb );
			Log( ua < ub );
			Log( ua <= ub );
			Log( ua > ub );
			Log( ua >= ub );
		}
	}
}

/*
================
8 and 16 bit loads and stores, sign extension
================
*/

static signed char	sc[8];
static unsigned char	uc[8];
static short		ss[8];
static unsigned short	us[8];

static void TestSmallTypes( void ) {
	int i, v;

	for ( i = 0; i < NUM_INT_EDGE; i++ ) {
		v = intEdge[i];

		sc[i & 7] = v;
		uc[i & 7] = v;
		ss[i & 7] = v;
		// q3asm has no CVUU2, so store the unsigned short as a short
		*(short *)&us[i & 7] = v;

		Log( sc[i & 7] );
		Log( uc[i & 7] );
		Log( ss[i & 7] );
		Log( us[i & 7] );
		Log( (signed char)( v + 1 ) );
		Log( (short)( v - 1 ) );
	}
}

/*
================
Floating point
================
*/

static float floatEdge[] = {
	0.0f, -0.0f, 1.0f, -1.0f, 0.5f, -0.5f, 1.5f, -1.5f, 1.75f, -1.75f,
	2.5f, -2.5f, 3.14159f, 1e10f, -1e10f, 1e-10f, 123456.789f, 16777217.0f,
	2147483520.0f, -2147483648.0f
};
#define NUM_FLOAT_EDGE	( sizeof( floatEdge ) / sizeof( floatEdge[0] ) )

static float fa, fb, fzero;

static void TestFloats( void ) {
	int i, j;

	for ( i = 0; i < NUM_FLOAT_EDGE; i++ ) {
		fa = floatEdge[i];

		LogFloat( -fa );

		// float to int truncates toward zero, values must fit in an int
		if ( fa > -2147483648.5f && fa < 2147483520.5f ) {
			Log( (int)fa );
		}

		for ( j = 0; j < NUM_FLOAT_EDGE; j++ ) {
			fb = floatEdge[j];

			LogFloat( fa + fb );
			LogFloat( fa - fb );
			LogFloat( fa * fb );
			LogFloat( fa / fb );

			Log( fa == fb );
			Log( fa != fb );
			Log( fa < fb );
			Log( fa <= fb );
			Log( fa > fb );
			Log( fa >= fb );
		}
	}

	for ( i = 0; i < NUM_INT_EDGE; i++ ) {
		LogFloat( (float)intEdge[i] );
	}
}

/*
================
Structure copies (OP_BLOCK_COPY)
================
*/

// no padding, uninitialized padding would hold stack contents that differ
// between the interpreter and the compilers
typedef struct {
	int	words[9];
	char	bytes[4];
} blob_t;

static blob_t	blobs[4];

static blob_t MakeBlob( int seed ) {
	blob_t b;
	int i;

	for ( i = 0; i < 9; i++ ) {
		b.words[i] = seed * ( i + 1 );
	}
	b.bytes[0] = seed;
	b.bytes[1] = seed >> 8;
	b.bytes[2] = seed >> 16;
	b.bytes[3] = seed >> 24;

	return b;
}

static int SumBlob( blob_t b ) {
	int i, sum;

	sum = b.bytes[0] + b.bytes[1] + b.bytes[2] + b.bytes[3];
	for ( i = 0; i < 9; i++ ) {
		sum += b.words[i];
	}

	return sum;
}

static void TestBlockCopy( void ) {
	int i;

	for ( i = 0; i < 4; i++ ) {
		blobs[i] = MakeBlob( 0x10203 * ( i + 1 ) );
	}
	blobs[0] = blobs[3];
	blobs[2] = blobs[1];

	for ( i = 0; i < 4; i++ ) {
		Log( SumBlob( blobs[i] ) );
	}
}

/*
================
Dense switch statements compile to jump tables (OP_JUMP)
================
*/

static int Switch( int v ) {
	switch ( v ) {
	case 0: return 10;
	case 1: return 11;
	case 2: return 12;
	case 3: return 13;
	case 4: return 14;
	case 5: return 15;
	case 6: return 16;
	case 7: return 17;
	case 8: return 18;
	case 9: return 19;
	default: return -1;
	}
}

static void TestSwitch( void ) {
	int i;

	for ( i = -3; i < 13; i++ ) {
		Log( Switch( i ) );
	}
}

/*
================
Calls: function pointers, recursion, many arguments, discarded results
================
*/

typedef int (*intFunc_t)( int );

static int Add1( int x ) { return x + 1; }
static int Twice( int x ) { return x * 2; }
static int Square( int x ) { return x * x; }

static intFunc_t funcs[] = { Add1, Twice, Square };

static int Fib( int n ) {
	if ( n < 2 ) {
		return n;
	}
	return Fib( n - 1 ) + Fib( n - 2 );
}

static int Many( int a, int b, int c, int d, int e, int f, int g, int h,
	int i, int j, int k, int l ) {
	return a + b * 2 + c * 3 + d * 4 + e * 5 + f * 6 + g * 7 + h * 8 +
		i * 9 + j * 10 + k * 11 + l * 12;
}

static float Lerp( float a, float b, float t ) {
	return a + ( b - a ) * t;
}

static void TestCalls( void ) {
	int i;

	for ( i = 0; i < 9; i++ ) {
		Log( funcs[i % 3]( i ) );
	}

	Log( Fib( 15 ) );
	Log( Many( 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12 ) );
	LogFloat( Lerp( 1.0f, 3.0f, 0.25f ) );

	// discarded return values leave OP_POP behind
	Add1( 3 );
	Fib( 4 );

	Log( trap_Sum15( 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 ) );
	Log( trap_Sum15( -1, 0x7fffffff, 3, 0x80000000, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, -15 ) );
}

/*
================
Deep expressions use many opStack slots
================
*/

static int g[16];

static void TestDeepExpressions( void ) {
	int i;

	for ( i = 0; i < 16; i++ ) {
		g[i] = i * 7 - 20;
	}

	Log( g[0] + ( g[1] - ( g[2] * ( g[3] + ( g[4] ^ ( g[5] | ( g[6] & ( g[7] + ( g[8] - ( g[9] *
		( g[10] + ( g[11] ^ ( g[12] | ( g[13] + ( g[14] - g[15] ) ) ) ) ) ) ) ) ) ) ) ) ) ) );

	Log( ( ( g[1] + g[2] ) * ( g[3] + g[4] ) ) - ( ( g[5] + g[6] ) * ( g[7] - g[8] ) ) +
		( ( g[9] ^ g[10] ) | ( g[11] & g[12] ) ) * ( ( g[13] << 2 ) - ( g[14] >> 1 ) ) );
}

/*
================
Locals, address-of and pointer arithmetic
================
*/

static void Fill( int *p, int n ) {
	int i;

	for ( i = 0; i < n; i++ ) {
		p[i] = i * i - n;
	}
}

static void TestLocals( void ) {
	int array[64];
	char text[32];
	int i, sum;

	Fill( array, 64 );

	sum = 0;
	for ( i = 0; i < 64; i++ ) {
		sum += array[i] * ( i & 1 ? -1 : 1 );
	}
	Log( sum );

	for ( i = 0; i < 31; i++ ) {
		text[i] = 'a' + ( i % 26 );
	}
	text[31] = 0;
	trap_LogString( text );
}

/*
================
Addresses wrap at the size of the data segment
================
*/

static int maskTarget;

static void TestMasking( void ) {
	int *p;

	// the segment size is a power of two below 0x10000000, so this
	// address masks down to &maskTarget
	p = (int *)( 0xf0000000 + (int)&maskTarget );
	*p = 1234;
	Log( maskTarget );

	maskTarget = 5678;
	Log( *p );
}

/*
================
Re-entering the VM from a system call
================
*/

int Reenter( int value ) {
	trap_Log( value );
	return value * value + Fib( 10 );
}

static void TestReenter( void ) {
	int before, after;

	before = Fib( 8 );
	Log( trap_Reenter( 7 ) );
	after = Fib( 8 );

	// our own frame must survive the nested call
	Log( before == after );
}

/*
================
TestNaN

Comparisons with NaN are false except !=
================
*/

int TestNaN( void ) {
	int i;
	float nan;

	checksum = 0;
	nan = fzero / fzero;

	for ( i = 0; i < NUM_FLOAT_EDGE + 1; i++ ) {
		fa = i < NUM_FLOAT_EDGE ? floatEdge[i] : nan;
		fb = nan;

		LogFloat( fa + fb );
		LogFloat( fb * fa );

		Log( fa == fb );
		Log( fa != fb );
		Log( fa < fb );
		Log( fa <= fb );
		Log( fa > fb );
		Log( fa >= fb );
		Log( fb == fa );
		Log( fb != fa );
		Log( fb < fa );
		Log( fb <= fa );
		Log( fb > fa );
		Log( fb >= fa );
	}

	return checksum;
}

int RunSuite( void ) {
	checksum = 0;

	TestIntegers();
	TestSmallTypes();
	TestFloats();
	TestBlockCopy();
	TestSwitch();
	TestCalls();
	TestDeepExpressions();
	TestLocals();
	TestMasking();
	TestReenter();

	return checksum;
}

/*
================
Error cases
================
*/

int BadCall( void ) {
	intFunc_t f;

	f = (intFunc_t)0x7fffffff;
	return f( 1 );
}

int RecurseForever( int depth ) {
	int pad[16];

	pad[depth & 15] = depth;
	return RecurseForever( depth + 1 ) + pad[0];
}

/*
================
Bench

Integer, float and call heavy work for timing
================
*/

int Bench( void ) {
	int i, j, sum;
	float f;

	sum = 0;
	f = 0.0f;

	for ( i = 0; i < 2000; i++ ) {
		for ( j = 0; j < 100; j++ ) {
			sum += ( i * j ) ^ ( sum >> 3 );
			f = f * 0.5f + (float)j;
		}
		sum += Fib( 10 ) + (int)f;
	}

	return sum;
}

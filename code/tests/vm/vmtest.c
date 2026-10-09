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
// vmtest.c -- runs QVM code on the interpreter and the bytecode compiler
// and checks that both behave the same
//
//   vmtest <vmtest.qvm>
//     run the test program in qvm/vmtest_qvm.c
//   vmtest --smoke game|cgame|ui <module.qvm> <command> [args...]
//     run a game module entry point against stub system calls
//   vmtest --bench <vmtest.qvm> <iterations>
//     time the benchmark command of the test program

#include "../../qcommon/vm_local.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

/*
=================
Engine stubs
=================
*/

static cvar_t		developer;
cvar_t			*com_developer = &developer;

static jmp_buf		abortFrame;
static char		errorMessage[1024];
static const char	*qvmPath;
static qboolean		verbose;

void QDECL Com_Error(int code, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	Q_vsnprintf(errorMessage, sizeof(errorMessage), fmt, ap);
	va_end(ap);

	longjmp(abortFrame, 1);
}

void QDECL Com_Printf(const char *fmt, ...)
{
	va_list ap;

	if(!verbose)
		return;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

void QDECL Com_DPrintf(const char *fmt, ...)
{
}

cvar_t *Cvar_Get(const char *name, const char *value, int flags)
{
	static cvar_t dummy;

	return &dummy;
}

void Cmd_AddCommand(const char *name, xcommand_t function)
{
}

int Cmd_Argc(void)
{
	return 0;
}

char *Cmd_Argv(int arg)
{
	return "";
}

void *Hunk_Alloc(int size, ha_pref preference)
{
	return calloc(1, size);
}

int Hunk_MemoryRemaining(void)
{
	return 0;
}

void *Z_Malloc(int size)
{
	return calloc(1, size);
}

void Z_Free(void *ptr)
{
	free(ptr);
}

void * QDECL Sys_LoadGameDll(const char *name, vmMainProc *entryPoint,
	intptr_t (QDECL *systemcalls)(intptr_t, ...))
{
	return NULL;
}

void Sys_UnloadDll(void *dllHandle)
{
}

int FS_FindVM(void **startSearch, char *found, int foundlen, const char *name, int enableDll)
{
	if(*startSearch)
		return -1;

	*startSearch = (void *) qvmPath;
	Q_strncpyz(found, qvmPath, foundlen);

	return VMI_COMPILED;
}

long FS_ReadFileDir(const char *qpath, void *searchPath, qboolean unpure, void **buffer)
{
	FILE *f;
	long len;
	byte *data;

	// VM_LoadQVM asks for vm/<name>.qvm, always hand it the file under test
	f = fopen(qvmPath, "rb");
	if(!f)
	{
		if(buffer)
			*buffer = NULL;
		return -1;
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if(!buffer)
	{
		fclose(f);
		return len;
	}

	data = malloc(len + 1);
	if(fread(data, 1, len, f) != (size_t) len)
	{
		fclose(f);
		free(data);
		*buffer = NULL;
		return -1;
	}
	data[len] = 0;
	fclose(f);

	*buffer = data;
	return len;
}

long FS_ReadFile(const char *qpath, void **buffer)
{
	// only used for .map symbol files, which the tests don't need
	if(buffer)
		*buffer = NULL;

	return -1;
}

void FS_FreeFile(void *buffer)
{
	free(buffer);
}

qboolean FS_Which(const char *filename, void *searchPath)
{
	return qfalse;
}

#if idx64
int (*Q_VMftol)(void) = qvmftolsse;
#elif id386
long (QDECL *Q_ftol)(float f) = qftolsse;
int (QDECL *Q_VMftol)(void) = qvmftolsse;
void (QDECL *Q_SnapVector)(vec3_t vec) = qsnapvectorsse;
#endif

/*
=================
Traces

Every system call appends to the trace of the current run, the runs
match when their traces match
=================
*/

typedef struct {
	int	*data;
	int	count;
	int	size;
} vmTrace_t;

static vmTrace_t	*activeTrace;
static int	syscallCount;
static int	maxSyscalls = 1000000;

static void TraceAdd(int v)
{
	vmTrace_t *t = activeTrace;

	if(t->count == t->size)
	{
		t->size = t->size ? t->size * 2 : 1024;
		t->data = realloc(t->data, t->size * sizeof(*t->data));
	}

	t->data[t->count++] = v;
}

static unsigned HashBytes(const byte *p, int len)
{
	unsigned h = 2166136261u;
	int i;

	for(i = 0; i < len; i++)
		h = (h ^ p[i]) * 16777619u;

	return h;
}

// a pointer argument checked against the VM's data segment
static void *CheckedPtr(intptr_t vmAddr, intptr_t len)
{
	if(vmAddr < 0 || len < 0 || vmAddr + len > currentVM->dataMask + 1)
		Com_Error(ERR_DROP, "system call pointer out of range");

	return currentVM->dataBase + vmAddr;
}

static int FloatBits(float f)
{
	floatint_t fi;

	fi.f = f;
	return fi.i;
}

static float BitsFloat(intptr_t i)
{
	floatint_t fi;

	fi.i = (int) i;
	return fi.f;
}

/*
=================
TestSyscalls

System calls of the test program, numbered as in qvm/vmtest_syscalls.asm
=================
*/

static intptr_t TestSyscalls(intptr_t *args)
{
	intptr_t r;
	int i;

	syscallCount++;

	switch(args[0])
	{
	case 0:		// trap_Log( int value )
		TraceAdd(0x1000);
		TraceAdd(args[1]);
		return 0;

	case 1:		// trap_LogFloat( float value )
		TraceAdd(0x1001);
		TraceAdd(args[1]);
		return 0;

	case 2:		// trap_Error( const char *message )
		Com_Error(ERR_DROP, "trap_Error: %s", (char *) VM_ArgPtr(args[1]));

	case 3:		// trap_Sum15( 15 ints )
		r = 0;
		for(i = 1; i <= 15; i++)
			r += args[i] * i;
		TraceAdd(0x1003);
		TraceAdd(r);
		return r;

	case 4:		// trap_Reenter( int value )
		r = VM_Call(currentVM, 1, (int) args[1]);
		TraceAdd(0x1004);
		TraceAdd(r);
		return r;

	case 5:		// trap_LogString( const char *s )
		TraceAdd(0x1005);
		TraceAdd(HashBytes(VM_ArgPtr(args[1]), strlen(VM_ArgPtr(args[1]))));
		return 0;

	default:
		Com_Error(ERR_DROP, "unknown test system call %d", (int) args[0]);
	}
}

/*
=================
SmokeSyscalls

Stand-in for the engine when running real game modules: every call is
traced, the libc and math calls the modules rely on are implemented, and
everything else returns 0
=================
*/

static int smokePrintCall, smokeErrorCall;

static intptr_t SmokeSyscalls(intptr_t *args)
{
	int num = args[0];

	if(++syscallCount > maxSyscalls)
		Com_Error(ERR_DROP, "system call limit reached");

	// argument slots a call doesn't use hold stale stack contents, which
	// differ because only the interpreter stores return addresses there,
	// so only the calls handled below trace their arguments
	TraceAdd(num);

	if(num == smokePrintCall)
	{
		const char *s = VM_ArgPtr(args[1]);

		TraceAdd(HashBytes((const byte *) s, strlen(s)));
		if(verbose)
			printf("%s", s);
		return 0;
	}

	if(num == smokeErrorCall)
		Com_Error(ERR_DROP, "module error: %s", (char *) VM_ArgPtr(args[1]));

	if(num >= 100 && num <= 106)
	{
		TraceAdd(args[1]);
		TraceAdd(args[2]);
		TraceAdd(args[3]);
	}

	switch(num)
	{
	case 100:	// memset
		memset(CheckedPtr(args[1], args[3]), args[2], args[3]);
		return args[1];
	case 101:	// memcpy
		memcpy(CheckedPtr(args[1], args[3]), CheckedPtr(args[2], args[3]), args[3]);
		return args[1];
	case 102:	// strncpy
		strncpy(CheckedPtr(args[1], args[3]), VM_ArgPtr(args[2]), args[3]);
		return args[1];
	case 103:
		return FloatBits(sin(BitsFloat(args[1])));
	case 104:
		return FloatBits(cos(BitsFloat(args[1])));
	case 105:
		return FloatBits(atan2(BitsFloat(args[1]), BitsFloat(args[2])));
	case 106:
		return FloatBits(sqrt(BitsFloat(args[1])));
	default:
		return 0;
	}
}

/*
=================
RunVM
=================
*/

typedef struct {
	qboolean	loaded;		// VM_Create succeeded
	qboolean	error;
	char		message[1024];
	intptr_t	result;
	vmTrace_t	trace;
	unsigned	memoryHash;
	double		seconds;
} runResult_t;

static double Now(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void RunVM(vmInterpret_t interpret, intptr_t (*syscalls)(intptr_t *),
	const int *args, int iterations, runResult_t *res)
{
	vm_t * volatile vm = NULL;
	double start;
	int i;

	memset(res, 0, sizeof(*res));
	activeTrace = &res->trace;
	syscallCount = 0;

	if(setjmp(abortFrame))
	{
		res->error = qtrue;
		Q_strncpyz(res->message, errorMessage, sizeof(res->message));

		// the VM may have been left mid call or half created
		VM_Forced_Unload_Start();
		VM_Clear();
		VM_Forced_Unload_Done();
		return;
	}

	vm = VM_Create(interpret == VMI_COMPILED ? "vmtest_jit" : "vmtest_interp", syscalls, interpret);
	if(!vm)
		Com_Error(ERR_DROP, "VM_Create failed");

	if(vm->compiled != (interpret == VMI_COMPILED))
		Com_Error(ERR_DROP, "the VM was not %s", interpret == VMI_COMPILED ? "compiled" : "interpreted");

	res->loaded = qtrue;

	start = Now();
	for(i = 0; i < iterations; i++)
	{
		res->result = VM_Call(vm, args[0], args[1], args[2], args[3], args[4], args[5],
			args[6], args[7], args[8], args[9], args[10], args[11], args[12]);
	}
	res->seconds = Now() - start;

	// the stack holds return addresses only the interpreter writes, so
	// compare the data segment below it
	res->memoryHash = HashBytes(vm->dataBase, vm->stackBottom);

	VM_Free(vm);
}

/*
=================
Compare
=================
*/

static int failures;

static void Report(const char *name, qboolean ok, const char *detail)
{
	printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name, detail ? ": " : "", detail ? detail : "");

	if(!ok)
		failures++;
}

static void Compare(const char *name, const runResult_t *a, const runResult_t *b)
{
	char detail[2048];
	int i;

	if(!a->loaded || !b->loaded)
	{
		Com_sprintf(detail, sizeof(detail), "couldn't load the VM: %s", !a->loaded ? a->message : b->message);
		Report(name, qfalse, detail);
		return;
	}

	// the interpreter and the compilers word their errors differently,
	// so only require that both stopped
	if(a->error != b->error)
	{
		Com_sprintf(detail, sizeof(detail), "interpreter %s \"%s\", compiler %s \"%s\"",
			a->error ? "failed" : "succeeded", a->message,
			b->error ? "failed" : "succeeded", b->message);
		Report(name, qfalse, detail);
		return;
	}

	for(i = 0; i < a->trace.count && i < b->trace.count; i++)
	{
		if(a->trace.data[i] != b->trace.data[i])
			break;
	}

	if(i < a->trace.count || i < b->trace.count)
	{
		// VMTEST_DUMP=1 lists the entries around the first difference
		if(getenv("VMTEST_DUMP"))
		{
			int j;

			for(j = (i > 8 ? i - 8 : 0); j < i + 8; j++)
			{
				printf("  %6d %c %08x %08x\n", j, j == i ? '>' : ' ',
					j < a->trace.count ? a->trace.data[j] : 0,
					j < b->trace.count ? b->trace.data[j] : 0);
			}
		}

		Com_sprintf(detail, sizeof(detail), "traces differ at entry %d of %d/%d (0x%x vs 0x%x)",
			i, a->trace.count, b->trace.count,
			i < a->trace.count ? a->trace.data[i] : 0,
			i < b->trace.count ? b->trace.data[i] : 0);
		Report(name, qfalse, detail);
		return;
	}

	if(!a->error && a->result != b->result)
	{
		Com_sprintf(detail, sizeof(detail), "results differ (%d vs %d)", (int) a->result, (int) b->result);
		Report(name, qfalse, detail);
		return;
	}

	if(!a->error && a->memoryHash != b->memoryHash)
	{
		Com_sprintf(detail, sizeof(detail), "data segments differ (results %d / %d)", (int) a->result, (int) b->result);
		Report(name, qfalse, detail);
		return;
	}

	if(a->error)
		Com_sprintf(detail, sizeof(detail), "%d trace entries, both stopped (\"%s\" / \"%s\")",
			a->trace.count, a->message, b->message);
	else
		Com_sprintf(detail, sizeof(detail), "%d trace entries", a->trace.count);
	Report(name, qtrue, detail);
}

static void FreeResult(runResult_t *r)
{
	free(r->trace.data);
	r->trace.data = NULL;
}

static void CompareCommand(const char *name, intptr_t (*syscalls)(intptr_t *), const int *args)
{
	runResult_t interp, jit;

	RunVM(VMI_BYTECODE, syscalls, args, 1, &interp);
	RunVM(VMI_COMPILED, syscalls, args, 1, &jit);
	Compare(name, &interp, &jit);
	FreeResult(&interp);
	FreeResult(&jit);
}

/*
=================
Test modes
=================
*/

// commands of the test program, see qvm/vmtest_qvm.c
enum {
	VMTEST_SUITE,
	VMTEST_REENTER,
	VMTEST_BAD_CALL,
	VMTEST_RECURSE_FOREVER,
	VMTEST_BENCH,
	VMTEST_NAN
};

static void RunSuite(void)
{
	int args[MAX_VMMAIN_ARGS] = { 0 };
	runResult_t jit;

	args[0] = VMTEST_SUITE;
	CompareCommand("suite", TestSyscalls, args);

	args[0] = VMTEST_BAD_CALL;
	CompareCommand("call outside VM", TestSyscalls, args);

	args[0] = VMTEST_NAN;
	CompareCommand("NaN comparisons", TestSyscalls, args);

#if defined(__aarch64__)
	// only the AArch64 compiler checks the program stack, the interpreter
	// would never return and the x86 compiler overflows the native stack
	args[0] = VMTEST_RECURSE_FOREVER;
	RunVM(VMI_COMPILED, TestSyscalls, args, 1, &jit);
	Report("runaway recursion", jit.loaded && jit.error && strstr(jit.message, "stack overflow"), jit.message);
	FreeResult(&jit);
#endif
}

static void RunSmoke(const char *module, int argc, char **argv)
{
	int args[MAX_VMMAIN_ARGS] = { 0 };
	char name[64];
	int i;

	if(!Q_stricmp(module, "ui"))
	{
		smokeErrorCall = 0;
		smokePrintCall = 1;
	}
	else
	{
		smokePrintCall = 0;
		smokeErrorCall = 1;
	}

	for(i = 0; i < argc && i < MAX_VMMAIN_ARGS; i++)
		args[i] = atoi(argv[i]);

	Com_sprintf(name, sizeof(name), "%s command %d", module, args[0]);
	CompareCommand(name, SmokeSyscalls, args);
}

static void RunBench(int iterations)
{
	int args[MAX_VMMAIN_ARGS] = { 0 };
	runResult_t interp, jit;
	char detail[256];

	args[0] = VMTEST_BENCH;
	RunVM(VMI_BYTECODE, TestSyscalls, args, iterations, &interp);
	RunVM(VMI_COMPILED, TestSyscalls, args, iterations, &jit);
	Compare("bench", &interp, &jit);

	if(!interp.error && !jit.error)
	{
		Com_sprintf(detail, sizeof(detail), "interpreter %.3fs, compiler %.3fs, %.1fx faster",
			interp.seconds, jit.seconds, interp.seconds / jit.seconds);
		Report("bench timing", qtrue, detail);
	}

	FreeResult(&interp);
	FreeResult(&jit);
}

int main(int argc, char **argv)
{
	if(argc > 1 && !strcmp(argv[1], "-v"))
	{
		verbose = qtrue;
		argc--;
		argv++;
	}

	if(argc >= 5 && !strcmp(argv[1], "--smoke"))
	{
		qvmPath = argv[3];
		RunSmoke(argv[2], argc - 4, argv + 4);
	}
	else if(argc == 4 && !strcmp(argv[1], "--bench"))
	{
		qvmPath = argv[2];
		RunBench(atoi(argv[3]));
	}
	else if(argc == 2)
	{
		qvmPath = argv[1];
		RunSuite();
	}
	else
	{
		fprintf(stderr,
			"usage: vmtest [-v] <vmtest.qvm>\n"
			"       vmtest [-v] --smoke game|cgame|ui <module.qvm> <command> [args...]\n"
			"       vmtest [-v] --bench <vmtest.qvm> <iterations>\n");
		return 2;
	}

	return failures ? 1 : 0;
}

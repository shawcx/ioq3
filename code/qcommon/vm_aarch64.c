/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
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
// vm_aarch64.c -- load time compiler and execution environment for AArch64

#if defined(__aarch64__)

#include "vm_local.h"

#include <sys/mman.h>

#ifdef __APPLE__
  #include <pthread.h>
  #include <libkern/OSCacheControl.h>
#endif

#ifndef MAP_ANONYMOUS
  #define MAP_ANONYMOUS MAP_ANON
#endif

/*

  Register usage in generated code:

  w0-w2, x9, x16	scratch
  s0-s1			scratch
  x19			vm->dataBase
  w20			vm->dataMask
  x21			opStack base
  w22			opStack index, kept in 0-255 like the interpreter's uint8_t
  w23			programStack
  x24			vm->instructionPointers (absolute addresses once compiled)
  w25			vm->instructionCount
  w26			vm->stackBottom
  x29, x30		frame pointer and link register

  OP_ENTER pushes x29/x30 and OP_LEAVE pops them, so calls inside the VM
  use the native call stack. x19-x26 are callee-saved in both AAPCS64 and
  the Apple ABI, so they survive calls out to C. x18 is never touched as it
  is reserved on Apple platforms.

  Layout of the generated code:

  ErrJump, StackOverflow and StackUnderflow error stubs
  CallProc	OP_CALL target, dispatches VM calls and system calls
  entry		vm->entryOfs, called from VM_CallCompiled
  instructions	one block per bytecode instruction
*/

#define rDATABASE	19
#define rDATAMASK	20
#define rOPSTACK	21
#define rOPSTACKOFS	22
#define rPSTACK		23
#define rINSTRPTRS	24
#define rINSTRCOUNT	25
#define rSTACKBOTTOM	26
#define rFP		29
#define rLR		30
#define rSP		31	// as a base register or add/sub immediate operand
#define rZR		31	// everywhere else

// condition codes
#define cEQ	0x0
#define cNE	0x1
#define cHS	0x2
#define cLO	0x3
#define cMI	0x4
#define cPL	0x5
#define cHI	0x8
#define cLS	0x9
#define cGE	0xA
#define cLT	0xB
#define cGT	0xC
#define cLE	0xD

// the inverse of every condition is the same code with bit 0 flipped
#define INVERT(cond)	((cond) ^ 1)

// arithmetic
#define ADDi_W(rd, rn, imm)	(0x11000000 | ((imm) << 10) | ((rn) << 5) | (rd))
#define SUBi_W(rd, rn, imm)	(0x51000000 | ((imm) << 10) | ((rn) << 5) | (rd))
#define ADDi_X(rd, rn, imm)	(0x91000000 | ((imm) << 10) | ((rn) << 5) | (rd))
#define ADD_W(rd, rn, rm)	(0x0B000000 | ((rm) << 16) | ((rn) << 5) | (rd))
#define SUB_W(rd, rn, rm)	(0x4B000000 | ((rm) << 16) | ((rn) << 5) | (rd))
#define CMP_W(rn, rm)		(0x6B00001F | ((rm) << 16) | ((rn) << 5))
#define NEG_W(rd, rm)		SUB_W(rd, rZR, rm)
#define MUL_W(rd, rn, rm)	(0x1B007C00 | ((rm) << 16) | ((rn) << 5) | (rd))
#define MSUB_W(rd, rn, rm, ra)	(0x1B008000 | ((rm) << 16) | ((ra) << 10) | ((rn) << 5) | (rd))
#define SDIV_W(rd, rn, rm)	(0x1AC00C00 | ((rm) << 16) | ((rn) << 5) | (rd))
#define UDIV_W(rd, rn, rm)	(0x1AC00800 | ((rm) << 16) | ((rn) << 5) | (rd))

// logic and shifts
#define AND_W(rd, rn, rm)	(0x0A000000 | ((rm) << 16) | ((rn) << 5) | (rd))
#define ORR_W(rd, rn, rm)	(0x2A000000 | ((rm) << 16) | ((rn) << 5) | (rd))
#define EOR_W(rd, rn, rm)	(0x4A000000 | ((rm) << 16) | ((rn) << 5) | (rd))
#define MVN_W(rd, rm)		(0x2A2003E0 | ((rm) << 16) | (rd))
#define ANDi_W_FF(rd, rn)	(0x12001C00 | ((rn) << 5) | (rd))	// and wd, wn, #0xff
#define LSLV_W(rd, rn, rm)	(0x1AC02000 | ((rm) << 16) | ((rn) << 5) | (rd))
#define LSRV_W(rd, rn, rm)	(0x1AC02400 | ((rm) << 16) | ((rn) << 5) | (rd))
#define ASRV_W(rd, rn, rm)	(0x1AC02800 | ((rm) << 16) | ((rn) << 5) | (rd))
#define SXTB_W(rd, rn)		(0x13001C00 | ((rn) << 5) | (rd))
#define SXTH_W(rd, rn)		(0x13003C00 | ((rn) << 5) | (rd))

// moves
#define MOV_W(rd, rm)		ORR_W(rd, rZR, rm)
#define MOV_X(rd, rm)		(0xAA0003E0 | ((rm) << 16) | (rd))
#define MOV_X_SP(rd, rn)	ADDi_X(rd, rn, 0)	// mov via add so it works with sp
#define MOVZ_W(rd, imm, hw)	(0x52800000 | ((hw) << 21) | ((imm) << 5) | (rd))
#define MOVK_W(rd, imm, hw)	(0x72800000 | ((hw) << 21) | ((imm) << 5) | (rd))
#define MOVN_W(rd, imm, hw)	(0x12800000 | ((hw) << 21) | ((imm) << 5) | (rd))
#define MOVZ_X(rd, imm, hw)	(0xD2800000 | ((hw) << 21) | ((imm) << 5) | (rd))
#define MOVK_X(rd, imm, hw)	(0xF2800000 | ((hw) << 21) | ((imm) << 5) | (rd))

// loads and stores, [xn, wm, uxtw] and [xn, wm, uxtw #scale]
#define LDR_W_R(rt, rn, rm)	(0xB8604800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define LDRH_W_R(rt, rn, rm)	(0x78604800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define LDRB_W_R(rt, rn, rm)	(0x38604800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define STR_W_R(rt, rn, rm)	(0xB8204800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define STRH_W_R(rt, rn, rm)	(0x78204800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define STRB_W_R(rt, rn, rm)	(0x38204800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define LDR_W_R2(rt, rn, rm)	(0xB8605800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define STR_W_R2(rt, rn, rm)	(0xB8205800 | ((rm) << 16) | ((rn) << 5) | (rt))
#define LDR_S_R2(st, rn, rm)	(0xBC605800 | ((rm) << 16) | ((rn) << 5) | (st))
#define STR_S_R2(st, rn, rm)	(0xBC205800 | ((rm) << 16) | ((rn) << 5) | (st))
#define LDR_X_R3(rt, rn, rm)	(0xF8605800 | ((rm) << 16) | ((rn) << 5) | (rt))

// loads and stores, [xn, #ofs]
#define LDR_W_I(rt, rn, ofs)	(0xB9400000 | (((ofs) >> 2) << 10) | ((rn) << 5) | (rt))
#define STR_W_I(rt, rn, ofs)	(0xB9000000 | (((ofs) >> 2) << 10) | ((rn) << 5) | (rt))
#define LDR_X_I(rt, rn, ofs)	(0xF9400000 | (((ofs) >> 3) << 10) | ((rn) << 5) | (rt))
#define STR_X_I(rt, rn, ofs)	(0xF9000000 | (((ofs) >> 3) << 10) | ((rn) << 5) | (rt))

// register pairs: signed offset, pre-index and post-index
#define STP_X(rt, rt2, rn, ofs)		(0xA9000000 | ((((ofs) >> 3) & 0x7F) << 15) | ((rt2) << 10) | ((rn) << 5) | (rt))
#define LDP_X(rt, rt2, rn, ofs)		(0xA9400000 | ((((ofs) >> 3) & 0x7F) << 15) | ((rt2) << 10) | ((rn) << 5) | (rt))
#define STP_X_PRE(rt, rt2, rn, ofs)	(0xA9800000 | ((((ofs) >> 3) & 0x7F) << 15) | ((rt2) << 10) | ((rn) << 5) | (rt))
#define LDP_X_POST(rt, rt2, rn, ofs)	(0xA8C00000 | ((((ofs) >> 3) & 0x7F) << 15) | ((rt2) << 10) | ((rn) << 5) | (rt))

// branches
#define BCOND_SKIP1(cond)	(0x54000040 | (cond))	// b.cond over the next instruction
#define TBNZ_W31(rt, ofs)	(0x37F80000 | ((((ofs) >> 2) & 0x3FFF) << 5) | (rt))
#define BR(rn)			(0xD61F0000 | ((rn) << 5))
#define BLR(rn)			(0xD63F0000 | ((rn) << 5))
#define RET			0xD65F03C0
#define BRK(imm)		(0xD4200000 | ((imm) << 5))

// floating point
#define SCVTF_S_W(sd, rn)	(0x1E220000 | ((rn) << 5) | (sd))
#define FCVTZS_W_S(rd, sn)	(0x1E380000 | ((sn) << 5) | (rd))
#define FADD_S(sd, sn, sm)	(0x1E202800 | ((sm) << 16) | ((sn) << 5) | (sd))
#define FSUB_S(sd, sn, sm)	(0x1E203800 | ((sm) << 16) | ((sn) << 5) | (sd))
#define FMUL_S(sd, sn, sm)	(0x1E200800 | ((sm) << 16) | ((sn) << 5) | (sd))
#define FDIV_S(sd, sn, sm)	(0x1E201800 | ((sm) << 16) | ((sn) << 5) | (sd))
#define FNEG_S(sd, sn)		(0x1E214000 | ((sn) << 5) | (sd))
#define FCMP_S(sn, sm)		(0x1E202000 | ((sm) << 16) | ((sn) << 5))

// B and BL reach +-128MB, keep the whole code block inside that
#define MAX_CODE_LENGTH	(1 << 27)

static	uint32_t	*buf;		// NULL during the sizing pass
static	int		bufLength;	// in bytes
static	int		compiledOfs;	// in bytes
static	byte		*code;
static	int		codeLength;
static	int		pc;

// offsets of the helper routines, kept between passes so that forward
// references inside the helpers resolve in the second pass
static	int	ofsErrJump, ofsStackOverflow, ofsStackUnderflow;
static	int	ofsCallProc, ofsCallProcSyscall;

typedef int (*vmEntry_t)(int *opStack, int *programStack, vm_t *vm);

/*
=================
Error handlers called from generated code
=================
*/

static void Q_NO_RETURN ErrJump(void)
{
	Com_Error(ERR_DROP, "program tried to execute code outside VM");
}

static void Q_NO_RETURN StackOverflow(void)
{
	Com_Error(ERR_DROP, "VM program stack overflow");
}

static void Q_NO_RETURN StackUnderflow(void)
{
	Com_Error(ERR_DROP, "VM returned past its entry point");
}

/*
=================
DoSyscall

Called from generated code with the negative instruction number of the
system call and the current programStack
=================
*/

static int DoSyscall(int syscallNum, int programStack)
{
	vm_t		*savedVM;
	int		*data;
	intptr_t	args[MAX_VMSYSCALL_ARGS];
	int		i;
	int		ret;

	// save currentVM so as to allow for recursive VM entry
	savedVM = currentVM;
	// modify VM stack pointer for recursive VM entry
	currentVM->programStack = programStack - 4;

	data = (int *) (savedVM->dataBase + programStack + 4);

	args[0] = ~syscallNum;
	for(i = 1; i < ARRAY_LEN(args); i++)
		args[i] = data[i];

	ret = savedVM->systemCall(args);

	currentVM = savedVM;

	return ret;
}

/*
=================
Instruction emitters
=================
*/

static void Emit(uint32_t isn)
{
	if(buf)
	{
		// both passes must emit the same code
		if(compiledOfs >= bufLength)
			Com_Error(ERR_FATAL, "VM_CompileAArch64: code buffer overflow");

		buf[compiledOfs >> 2] = isn;
	}

	compiledOfs += 4;
}

static uint32_t BranchImm26(int target)
{
	return ((target - compiledOfs) >> 2) & 0x3FFFFFF;
}

static void EmitB(int target)
{
	Emit(0x14000000 | BranchImm26(target));
}

static void EmitBL(int target)
{
	Emit(0x94000000 | BranchImm26(target));
}

static void EmitBcond(int cond, int target)
{
	int delta = (target - compiledOfs) >> 2;

	if(buf && (delta < -(1 << 18) || delta >= (1 << 18)))
		Com_Error(ERR_DROP, "VM_CompileAArch64: conditional branch out of range");

	Emit(0x54000000 | ((delta & 0x7FFFF) << 5) | cond);
}

static void EmitTbnz31(int rt, int target)
{
	Emit(TBNZ_W31(rt, target - compiledOfs));
}

static void EmitAdr(int rd, int target)
{
	int delta = target - compiledOfs;

	Emit(0x10000000 | ((delta & 3) << 29) | (((delta >> 2) & 0x7FFFF) << 5) | rd);
}

// rd = constant, one or two instructions
static void EmitMovW(int rd, uint32_t v)
{
	if(!(v & 0xFFFF0000))
		Emit(MOVZ_W(rd, v, 0));
	else if(!(v & 0xFFFF))
		Emit(MOVZ_W(rd, v >> 16, 1));
	else if((v & 0xFFFF0000) == 0xFFFF0000)
		Emit(MOVN_W(rd, ~v & 0xFFFF, 0));
	else
	{
		Emit(MOVZ_W(rd, v & 0xFFFF, 0));
		Emit(MOVK_W(rd, v >> 16, 1));
	}
}

// rd = 64-bit constant, always four instructions
static void EmitMovPtr(int rd, intptr_t ptr)
{
	uint64_t v = (uint64_t) ptr;

	Emit(MOVZ_X(rd, v & 0xFFFF, 0));
	Emit(MOVK_X(rd, (v >> 16) & 0xFFFF, 1));
	Emit(MOVK_X(rd, (v >> 32) & 0xFFFF, 2));
	Emit(MOVK_X(rd, (v >> 48) & 0xFFFF, 3));
}

static void EmitCallC(intptr_t func)
{
	EmitMovPtr(16, func);
	Emit(BLR(16));
}

// rd = rn + imm with imm taken as a two's complement value, clobbers w9
static void EmitAddImmW(int rd, int rn, uint32_t imm)
{
	uint32_t op = 0x11000000;	// add
	uint32_t v = imm;

	if(imm & 0x80000000)
	{
		op = 0x51000000;	// sub
		v = 0 - imm;
	}

	if(v < 0x1000)
		Emit(op | (v << 10) | (rn << 5) | rd);
	else if(v < 0x1000000)
	{
		Emit(op | (1 << 22) | ((v >> 12) << 10) | (rn << 5) | rd);
		if(v & 0xFFF)
			Emit(op | ((v & 0xFFF) << 10) | (rd << 5) | rd);
	}
	else
	{
		EmitMovW(9, imm);
		Emit(ADD_W(rd, rn, 9));
	}
}

// load a field of the vm_t pointed to by x2
static void EmitLoadVmField(int rt, size_t ofs, size_t size)
{
	if(ofs % size || ofs / size > 0xFFF)
		Com_Error(ERR_FATAL, "VM_CompileAArch64: vm_t field not addressable");

	if(size == 8)
		Emit(LDR_X_I(rt, 2, ofs));
	else
		Emit(LDR_W_I(rt, 2, ofs));
}

/*
=================
opStack helpers
=================
*/

static void EmitIncOpStack(void)
{
	Emit(ADDi_W(rOPSTACKOFS, rOPSTACKOFS, 1));
	Emit(ANDi_W_FF(rOPSTACKOFS, rOPSTACKOFS));
}

static void EmitDecOpStack(void)
{
	Emit(SUBi_W(rOPSTACKOFS, rOPSTACKOFS, 1));
	Emit(ANDi_W_FF(rOPSTACKOFS, rOPSTACKOFS));
}

// rt = top of opStack
static void EmitPeek(int rt)
{
	Emit(LDR_W_R2(rt, rOPSTACK, rOPSTACKOFS));
}

// top of opStack = rt
static void EmitPoke(int rt)
{
	Emit(STR_W_R2(rt, rOPSTACK, rOPSTACKOFS));
}

static void EmitPop(int rt)
{
	EmitPeek(rt);
	EmitDecOpStack();
}

static void EmitPush(int rt)
{
	EmitIncOpStack();
	EmitPoke(rt);
}

static void EmitPopF(int st)
{
	Emit(LDR_S_R2(st, rOPSTACK, rOPSTACKOFS));
	EmitDecOpStack();
}

// w0 = second operand, w1 = top; isn computes w0 = w0 op w1
static void EmitBinaryOp(uint32_t isn)
{
	EmitPop(1);
	EmitPeek(0);
	Emit(isn);
	EmitPoke(0);
}

// s0 = second operand, s1 = top; isn computes s0 = s0 op s1
static void EmitBinaryOpF(uint32_t isn)
{
	EmitPopF(1);
	Emit(LDR_S_R2(0, rOPSTACK, rOPSTACKOFS));
	Emit(isn);
	Emit(STR_S_R2(0, rOPSTACK, rOPSTACKOFS));
}

// w0 = op top
static void EmitUnaryOp(uint32_t isn)
{
	EmitPeek(0);
	Emit(isn);
	EmitPoke(0);
}

// w0 = dataBase[top & dataMask]
static void EmitLoad(uint32_t isn)
{
	EmitPeek(0);
	Emit(AND_W(0, 0, rDATAMASK));
	Emit(isn);
	EmitPoke(0);
}

// dataBase[second & dataMask] = top
static void EmitStore(uint32_t isn)
{
	EmitPop(0);
	EmitPop(1);
	Emit(AND_W(1, 1, rDATAMASK));
	Emit(isn);
}

static void EmitCondJump(vm_t *vm, int cond, int target, qboolean isFloat)
{
	if(target < 0 || target >= vm->instructionCount)
		Com_Error(ERR_DROP, "VM_CompileAArch64: jump target out of range at offset %d", pc);

	if(isFloat)
	{
		EmitPopF(1);
		EmitPopF(0);
		Emit(FCMP_S(0, 1));
	}
	else
	{
		EmitPop(1);
		EmitPop(0);
		Emit(CMP_W(0, 1));
	}

	Emit(BCOND_SKIP1(INVERT(cond)));
	EmitB(vm->instructionPointers[target]);
}

/*
=================
EmitHelpers

Error stubs, the OP_CALL dispatcher and the entry point
=================
*/

static void EmitHelpers(vm_t *vm)
{
	// error stubs, entered with bl so the link register shows where
	// the error happened in a debugger
	ofsErrJump = compiledOfs;
	EmitCallC((intptr_t) ErrJump);

	ofsStackOverflow = compiledOfs;
	EmitCallC((intptr_t) StackOverflow);

	// a return past the entry point lands here
	ofsStackUnderflow = compiledOfs;
	EmitCallC((intptr_t) StackUnderflow);

	// OP_CALL: the target instruction number is on top of the opStack.
	// VM calls tail-jump to the target, whose OP_ENTER saves the return
	// address of the call site.
	ofsCallProc = compiledOfs;
	EmitPop(0);
	EmitTbnz31(0, ofsCallProcSyscall);
	Emit(CMP_W(0, rINSTRCOUNT));
	EmitBcond(cHS, ofsErrJump);
	Emit(LDR_X_R3(16, rINSTRPTRS, 0));
	Emit(BR(16));

	// system call: w0 = syscall number, w1 = programStack
	ofsCallProcSyscall = compiledOfs;
	Emit(STP_X_PRE(rFP, rLR, rSP, -16));
	Emit(MOV_X_SP(rFP, rSP));
	Emit(MOV_W(1, rPSTACK));
	EmitCallC((intptr_t) DoSyscall);
	EmitPush(0);
	Emit(LDP_X_POST(rFP, rLR, rSP, 16));
	Emit(RET);

	// int entry(int *opStack, int *programStack, vm_t *vm)
	// returns the final opStack index and writes back programStack
	vm->entryOfs = compiledOfs;
	Emit(STP_X_PRE(rFP, rLR, rSP, -96));
	Emit(MOV_X_SP(rFP, rSP));
	Emit(STP_X(19, 20, rSP, 16));
	Emit(STP_X(21, 22, rSP, 32));
	Emit(STP_X(23, 24, rSP, 48));
	Emit(STP_X(25, 26, rSP, 64));
	Emit(STR_X_I(1, rSP, 80));

	Emit(MOV_X(rOPSTACK, 0));
	Emit(LDR_W_I(rPSTACK, 1, 0));
	EmitLoadVmField(rDATABASE, offsetof(vm_t, dataBase), sizeof(vm->dataBase));
	EmitLoadVmField(rDATAMASK, offsetof(vm_t, dataMask), sizeof(vm->dataMask));
	EmitLoadVmField(rINSTRPTRS, offsetof(vm_t, instructionPointers), sizeof(vm->instructionPointers));
	EmitLoadVmField(rINSTRCOUNT, offsetof(vm_t, instructionCount), sizeof(vm->instructionCount));
	EmitLoadVmField(rSTACKBOTTOM, offsetof(vm_t, stackBottom), sizeof(vm->stackBottom));
	Emit(MOVZ_W(rOPSTACKOFS, 0, 0));

	// guard frame: one OP_LEAVE too many returns into StackUnderflow
	// instead of back into C with the VM's registers
	EmitAdr(9, ofsStackUnderflow);
	Emit(STP_X_PRE(rFP, 9, rSP, -16));

	// vmMain is instruction 0
	EmitBL(vm->instructionPointers[0]);

	// drop the guard frame
	Emit(ADDi_X(rSP, rSP, 16));

	Emit(LDR_X_I(1, rSP, 80));
	Emit(STR_W_I(rPSTACK, 1, 0));
	Emit(MOV_W(0, rOPSTACKOFS));
	Emit(LDP_X(25, 26, rSP, 64));
	Emit(LDP_X(23, 24, rSP, 48));
	Emit(LDP_X(21, 22, rSP, 32));
	Emit(LDP_X(19, 20, rSP, 16));
	Emit(LDP_X_POST(rFP, rLR, rSP, 96));
	Emit(RET);
}

/*
=================
Code memory
=================
*/

// returns writable memory that VM_FinishCodeMemory turns executable
static byte *VM_AllocCodeMemory(int size)
{
	void *mem;

#ifdef __APPLE__
	// Apple silicon enforces W^X, MAP_JIT pages flip per thread
	mem = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
	if(mem == MAP_FAILED)
		Com_Error(ERR_FATAL, "VM_CompileAArch64: can't mmap memory");
	pthread_jit_write_protect_np(0);
#else
	mem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(mem == MAP_FAILED)
		Com_Error(ERR_FATAL, "VM_CompileAArch64: can't mmap memory");
#endif

	return mem;
}

static void VM_FinishCodeMemory(byte *mem, int size)
{
#ifdef __APPLE__
	pthread_jit_write_protect_np(1);
	sys_icache_invalidate(mem, size);
#else
	if(mprotect(mem, size, PROT_READ | PROT_EXEC))
		Com_Error(ERR_FATAL, "VM_CompileAArch64: mprotect failed");
	__builtin___clear_cache((char *) mem, (char *) mem + size);
#endif
}

static void VM_Destroy_Compiled(vm_t *self)
{
	if(self->codeBase)
		munmap(self->codeBase, self->codeLength);

	self->codeBase = NULL;
}

/*
=================
VM_Compile
=================
*/

static int Constant4(void)
{
	int v;

	if(pc + 4 > codeLength)
		Com_Error(ERR_DROP, "VM_CompileAArch64: truncated instruction at offset %d", pc);

	v = code[pc] | (code[pc + 1] << 8) | (code[pc + 2] << 16) | ((unsigned) code[pc + 3] << 24);
	pc += 4;

	return v;
}

static int Constant1(void)
{
	if(pc + 1 > codeLength)
		Com_Error(ERR_DROP, "VM_CompileAArch64: truncated instruction at offset %d", pc);

	return code[pc++];
}

void VM_Compile(vm_t *vm, vmHeader_t *header)
{
	int	pass;
	int	instruction;
	int	op;
	int	v;

	code = (byte *) header + header->codeOffset;
	codeLength = header->codeLength;
	buf = NULL;

	// the first pass only measures, both passes emit the same sizes
	for(pass = 0; pass < 2; pass++)
	{
		if(pass)
		{
			if(compiledOfs > MAX_CODE_LENGTH)
				Com_Error(ERR_DROP, "VM_CompileAArch64: %s is too large", vm->name);

			vm->codeLength = compiledOfs;
			vm->codeBase = VM_AllocCodeMemory(vm->codeLength);
			buf = (uint32_t *) vm->codeBase;
			bufLength = vm->codeLength;
		}

		compiledOfs = 0;
		EmitHelpers(vm);

		pc = 0;

		for(instruction = 0; instruction < header->instructionCount; instruction++)
		{
			if(pc >= codeLength)
				Com_Error(ERR_DROP, "VM_CompileAArch64: pc > header->codeLength");

			vm->instructionPointers[instruction] = compiledOfs;

			op = code[pc++];

			switch(op)
			{
			case OP_UNDEF:
			case OP_IGNORE:
				break;

			case OP_BREAK:
				Emit(BRK(0));
				break;

			case OP_ENTER:
				v = Constant4();
				Emit(STP_X_PRE(rFP, rLR, rSP, -16));
				Emit(MOV_X_SP(rFP, rSP));
				EmitAddImmW(rPSTACK, rPSTACK, 0 - (uint32_t) v);
				Emit(CMP_W(rPSTACK, rSTACKBOTTOM));
				Emit(BCOND_SKIP1(cGE));
				EmitBL(ofsStackOverflow);
				break;

			case OP_LEAVE:
				v = Constant4();
				EmitAddImmW(rPSTACK, rPSTACK, v);
				Emit(LDP_X_POST(rFP, rLR, rSP, 16));
				Emit(RET);
				break;

			case OP_CALL:
				EmitBL(ofsCallProc);
				break;

			case OP_PUSH:
				EmitIncOpStack();
				break;

			case OP_POP:
				EmitDecOpStack();
				break;

			case OP_CONST:
				EmitMovW(0, Constant4());
				EmitPush(0);
				break;

			case OP_LOCAL:
				EmitAddImmW(0, rPSTACK, Constant4());
				EmitPush(0);
				break;

			case OP_JUMP:
				EmitPop(0);
				Emit(CMP_W(0, rINSTRCOUNT));
				Emit(BCOND_SKIP1(cLO));
				EmitBL(ofsErrJump);
				Emit(LDR_X_R3(16, rINSTRPTRS, 0));
				Emit(BR(16));
				break;

			case OP_EQ:	EmitCondJump(vm, cEQ, Constant4(), qfalse); break;
			case OP_NE:	EmitCondJump(vm, cNE, Constant4(), qfalse); break;
			case OP_LTI:	EmitCondJump(vm, cLT, Constant4(), qfalse); break;
			case OP_LEI:	EmitCondJump(vm, cLE, Constant4(), qfalse); break;
			case OP_GTI:	EmitCondJump(vm, cGT, Constant4(), qfalse); break;
			case OP_GEI:	EmitCondJump(vm, cGE, Constant4(), qfalse); break;
			case OP_LTU:	EmitCondJump(vm, cLO, Constant4(), qfalse); break;
			case OP_LEU:	EmitCondJump(vm, cLS, Constant4(), qfalse); break;
			case OP_GTU:	EmitCondJump(vm, cHI, Constant4(), qfalse); break;
			case OP_GEU:	EmitCondJump(vm, cHS, Constant4(), qfalse); break;

			// MI, LS, GT and GE are false when either operand is NaN,
			// matching C comparisons
			case OP_EQF:	EmitCondJump(vm, cEQ, Constant4(), qtrue); break;
			case OP_NEF:	EmitCondJump(vm, cNE, Constant4(), qtrue); break;
			case OP_LTF:	EmitCondJump(vm, cMI, Constant4(), qtrue); break;
			case OP_LEF:	EmitCondJump(vm, cLS, Constant4(), qtrue); break;
			case OP_GTF:	EmitCondJump(vm, cGT, Constant4(), qtrue); break;
			case OP_GEF:	EmitCondJump(vm, cGE, Constant4(), qtrue); break;

			case OP_LOAD1:	EmitLoad(LDRB_W_R(0, rDATABASE, 0)); break;
			case OP_LOAD2:	EmitLoad(LDRH_W_R(0, rDATABASE, 0)); break;
			case OP_LOAD4:	EmitLoad(LDR_W_R(0, rDATABASE, 0)); break;

			case OP_STORE1:	EmitStore(STRB_W_R(0, rDATABASE, 1)); break;
			case OP_STORE2:	EmitStore(STRH_W_R(0, rDATABASE, 1)); break;
			case OP_STORE4:	EmitStore(STR_W_R(0, rDATABASE, 1)); break;

			case OP_ARG:
				v = Constant1();
				EmitPop(0);
				Emit(ADDi_W(1, rPSTACK, v));
				Emit(AND_W(1, 1, rDATAMASK));
				Emit(STR_W_R(0, rDATABASE, 1));
				break;

			case OP_BLOCK_COPY:
				// VM_BlockCopy(dest, src, n) with dest below src on the opStack
				v = Constant4();
				EmitPop(1);
				EmitPop(0);
				EmitMovW(2, v);
				EmitCallC((intptr_t) VM_BlockCopy);
				break;

			case OP_SEX8:	EmitUnaryOp(SXTB_W(0, 0)); break;
			case OP_SEX16:	EmitUnaryOp(SXTH_W(0, 0)); break;
			case OP_NEGI:	EmitUnaryOp(NEG_W(0, 0)); break;
			case OP_BCOM:	EmitUnaryOp(MVN_W(0, 0)); break;

			case OP_ADD:	EmitBinaryOp(ADD_W(0, 0, 1)); break;
			case OP_SUB:	EmitBinaryOp(SUB_W(0, 0, 1)); break;
			case OP_MULI:
			case OP_MULU:	EmitBinaryOp(MUL_W(0, 0, 1)); break;
			case OP_DIVI:	EmitBinaryOp(SDIV_W(0, 0, 1)); break;
			case OP_DIVU:	EmitBinaryOp(UDIV_W(0, 0, 1)); break;
			case OP_BAND:	EmitBinaryOp(AND_W(0, 0, 1)); break;
			case OP_BOR:	EmitBinaryOp(ORR_W(0, 0, 1)); break;
			case OP_BXOR:	EmitBinaryOp(EOR_W(0, 0, 1)); break;
			case OP_LSH:	EmitBinaryOp(LSLV_W(0, 0, 1)); break;
			case OP_RSHI:	EmitBinaryOp(ASRV_W(0, 0, 1)); break;
			case OP_RSHU:	EmitBinaryOp(LSRV_W(0, 0, 1)); break;

			case OP_MODI:
			case OP_MODU:
				// w0 = w0 - (w0 / w1) * w1
				EmitPop(1);
				EmitPeek(0);
				Emit(op == OP_MODI ? SDIV_W(2, 0, 1) : UDIV_W(2, 0, 1));
				Emit(MSUB_W(0, 2, 1, 0));
				EmitPoke(0);
				break;

			case OP_NEGF:
				Emit(LDR_S_R2(0, rOPSTACK, rOPSTACKOFS));
				Emit(FNEG_S(0, 0));
				Emit(STR_S_R2(0, rOPSTACK, rOPSTACKOFS));
				break;

			case OP_ADDF:	EmitBinaryOpF(FADD_S(0, 0, 1)); break;
			case OP_SUBF:	EmitBinaryOpF(FSUB_S(0, 0, 1)); break;
			case OP_DIVF:	EmitBinaryOpF(FDIV_S(0, 0, 1)); break;
			case OP_MULF:	EmitBinaryOpF(FMUL_S(0, 0, 1)); break;

			case OP_CVIF:
				EmitPeek(0);
				Emit(SCVTF_S_W(0, 0));
				Emit(STR_S_R2(0, rOPSTACK, rOPSTACKOFS));
				break;

			case OP_CVFI:
				// truncate toward zero like C and the x86 compiler
				Emit(LDR_S_R2(0, rOPSTACK, rOPSTACKOFS));
				Emit(FCVTZS_W_S(0, 0));
				EmitPoke(0);
				break;

			default:
				Com_Error(ERR_DROP, "VM_CompileAArch64: bad opcode %i at offset %i", op, pc);
			}
		}
	}

	VM_FinishCodeMemory(vm->codeBase, vm->codeLength);

	// offset all the instruction pointers for the new location
	for(instruction = 0; instruction < header->instructionCount; instruction++)
		vm->instructionPointers[instruction] += (intptr_t) vm->codeBase;

	buf = NULL;
	vm->destroy = VM_Destroy_Compiled;

	Com_Printf("VM file %s compiled to %i bytes of code\n", vm->name, vm->codeLength);
}

/*
==============
VM_CallCompiled
==============
*/

int VM_CallCompiled(vm_t *vm, int *args)
{
	byte	stack[OPSTACK_SIZE + 15];
	int	*opStack;
	int	opStackOfs;
	int	programStack, stackOnEntry;
	byte	*image;
	int	arg;

	currentVM = vm;

	vm->currentlyInterpreting = qtrue;

	// we might be called recursively, so this might not be the very top
	programStack = stackOnEntry = vm->programStack;

	// set up the stack frame
	image = vm->dataBase;

	programStack -= ( 8 + 4 * MAX_VMMAIN_ARGS );

	for ( arg = 0; arg < MAX_VMMAIN_ARGS; arg++ )
		*(int *)&image[ programStack + 8 + arg * 4 ] = args[ arg ];

	*(int *)&image[ programStack + 4 ] = 0;	// return stack
	*(int *)&image[ programStack ] = -1;	// will terminate the loop on return

	opStack = PADP(stack, 16);
	*opStack = 0xDEADBEEF;

	opStackOfs = ((vmEntry_t) (vm->codeBase + vm->entryOfs))(opStack, &programStack, vm);

	if(opStackOfs != 1 || *opStack != 0xDEADBEEF)
		Com_Error(ERR_DROP, "opStack corrupted in compiled code");

	if(programStack != stackOnEntry - (8 + 4 * MAX_VMMAIN_ARGS))
		Com_Error(ERR_DROP, "programStack corrupted in compiled code");

	vm->programStack = stackOnEntry;
	vm->currentlyInterpreting = qfalse;

	return opStack[opStackOfs];
}

#endif // __aarch64__

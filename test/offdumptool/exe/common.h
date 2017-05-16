/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   common.h

Environment:
   User Mode

Author:
   radutta
--*/
#pragma once

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>
#ifndef GUID_DEFINED
#include <guiddef.h>
#endif

#include <WinEvt.h>
#include <wchar.h>

#include <LogLib.hpp>
#include <devioctl.h>

#include <LegacyHeaders.h>
#include "Device_Specific.h"
#include "KdDebuggerData.h"
#include "Dump_Header.h"
#include "Device_IO.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef ULONGLIMIT
#define ULONGLIMIT                              (4*1024*1024)
#endif

#define SIZE_1MB                                (1024*1024)
#define EXTENDED_HIDDEN                         2048
#define DEFUALT_ALIGNMENT                       (1024*1024)
#define MIN_PARTITION_SIZE                      2*1024*1024*1024LL // 2GB min
#define DEFUALT_SECTOR_SZ                       512
#define DEFAULT_DMP_BUF_SZ                      (2*1024*1024)
#define VOLUME_NAME_SIZE                        50
#define DEVICE_NAME_SIZE                        40
#define MAX_FILE_SIZE                           0xFFFFFFF0000 // a ntfs limit
#define MAX_ALLOWED_DDR_SECTIONS                10

//  This is the current number of partitions in a raw dump file.
#define PARTITION_INFORMATION_SECTION_COUNT     16

#define DDEBUGGER_DATA64_CURRENT_SIZE sizeof(KDDEBUGGER_DATA64)
#define DDEBUGGER_DATA64_WIN81_SIZE   sizeof(KDDEBUGGER_DATA64_WIN81)
#define DDEBUGGER_DATA64_WIN80_SIZE   sizeof(KDDEBUGGER_DATA64_WIN80)
#define DDEBUGGER_DATA64_WIN70_SIZE   sizeof(KDDEBUGGER_DATA64_WIN70)
#define DDEBUGGER_DATA64_LEGACY_SIZE  sizeof(KDDEBUGGER_DATA64_LEGACY)

#define SECTIONS_COUNT(hdr)                     (((PRAW_DUMP_HEADER)(hdr))->SectionsCount)
#define SECTION_ENTRY_PTR(pTable, idx)          ((PRAW_DUMP_SECTION_HEADER)(&pTable[idx]))

#define UPDATE_IF_ZERO(dst, src)                (dst = ((0 == dst) ? src : dst))
#define UPDATE_IF_DIFFERENT(dst, src)           (dst = ((dst != src) ? src : dst))
#define UPDATE_IF_ZERO_OR_DIFFERENT(dst, src)   (dst = ((0 == dst) ? src : ((dst != src) ? src : dst)))

typedef struct _DDRSection{
    PWSTR DDRFileName;
    LARGE_INTEGER DDRBase;
    BOOL AlreadyMerged;
}DDRSection;

typedef struct{
    BOOL DisplayUsage;
    BOOL ParseDump;
    BOOL PasePartition;
    BOOL CreatePart;
    BOOL DumpRawPartition;
    BOOL ParseWPdump;
    BOOL GenDMPFormDDR;
    BOOL CheckDebugPolicy;
    BOOL CheckOfflineDumpEvent;
    BOOL CheckOfflineDumpEventUI;
    BOOL CheckValidDump;
    BOOL WipeRawDumpHeader;
    BOOL DumpDDR;
    BOOL DoNotParseAPREG;
    BOOL sharedIMEM;
    BOOL apreg64;
    LARGE_INTEGER apreg64TableAddr;
    DDRSection ddr[MAX_ALLOWED_DDR_SECTIONS];
    PWSTR DDRDataFileName;
    UINT32 DDRCount;
    PWSTR APREGFileName;
    PWSTR FileName;
} COMMAND_LINE_ARGS, *PCOMMAND_LINE_ARGS;

#pragma pack(1)

typedef struct
{
    UINT64      BaseOffset;
    UINT64      Length;
} DISK_REGION, *PDISK_REGION;

#ifdef ANY_SIZE
#undef ANY_SIZE
#endif
#define ANY_SIZE 1

#define PAGE_SIZE                                       0x1000

typedef struct
{
    UINT32      RegionCount;
    DISK_REGION Regions[ANY_SIZE];
} DISK_MAP, *PDISK_MAP;

#define FW_TABLE_OFFLINE_CRASHDUMP 111

typedef enum{
    OfflineDumpVersionUndefined = 0x0,
    OfflineDumpVersionA = 0xA,
    OfflineDumpVersionB = 0xB
} OFFLINE_DUMP_VERSION;

typedef struct _GPT_PARTITION_TABLE {
    UCHAR       Signature[8];
    ULONG       Revision;
    ULONG       HeaderSize;
    ULONG       HeaderCRC;
    ULONG       Reserved;
    unsigned __int64 MyLBA;
    unsigned __int64 AlternateLBA;
    unsigned __int64 FirstUsableLBA;
    unsigned __int64 LastUsableLBA;
    UCHAR       DiskGuid[16];
    unsigned __int64 PartitionEntryLBA;
    ULONG       PartitionCount;
    ULONG       PartitionEntrySize;
    ULONG       PartitionEntryArrayCRC;
    UCHAR       ReservedEnd[1];    // will extend till block size
} GPT_PARTITION_TABLE, *PGPT_PARTITION_TABLE;

//
// AP Reg defines.
//
#define AP_REG_STRUCTURE_MAGIC_VALUE    0x44434151
#define AP_REG_STRUCTURE_V2             0x2
#define AP_REG_STRUCTURE_V4             0x4
#define AP_REG_MAX_CPUS                 0x4

enum CPUMode
{
    usr_mode = 0x10,
    fiq_mode = 0x11,
    irq_mode = 0x12,
    svc_mode = 0x13,
    mon_mode = 0x16,
    abt_mode = 0x17,
    hyp_mode = 0x1a,
    und_mode = 0x1b,
    sys_mode = 0x1f,
};

//copied  from ntdbg.h
#define ARM_CONTEXT_ARM                 0x00200000L
#define ARM_CONTEXT_CONTROL             (ARM_CONTEXT_ARM | 0x00000001L)
#define ARM_CONTEXT_INTEGER             (ARM_CONTEXT_ARM | 0x00000002L)
#define ARM_CONTEXT_FLOATING_POINT      (ARM_CONTEXT_ARM | 0x00000004L)
#define ARM_CONTEXT_DEBUG_REGISTERS     (ARM_CONTEXT_ARM | 0x00000008L)
#define ARM_CONTEXT_FULL \
    (ARM_CONTEXT_CONTROL | ARM_CONTEXT_INTEGER | ARM_CONTEXT_FLOATING_POINT)

#define ARM_CONTEXT_ALL \
    (ARM_CONTEXT_FULL | ARM_CONTEXT_DEBUG_REGISTERS)

#define ARM_CONTEXT_EXCEPTION_ACTIVE      0x08000000L
#define ARM_CONTEXT_SERVICE_ACTIVE        0x10000000L
#define ARM_CONTEXT_UNWOUND_TO_CALL       0x20000000L
#define ARM_CONTEXT_EXCEPTION_REQUEST     0x40000000L
#define ARM_CONTEXT_EXCEPTION_REPORTING   0x80000000L

//
// Special Registers for ARM (NT case)
//

#define ARM_MAX_BREAKPOINTS     8
#define ARM_MAX_WATCHPOINTS     1


typedef struct _ARM_NEON128 {
    ULONGLONG Low;
    LONGLONG High;
} ARM_NEON128, *PARM_NEON128;

typedef struct _ARM_CONTEXT {
    //
    // The flags values within this flag control the contents of
    // a CONTEXT record.
    //
    // If the context record is used as an input parameter, then
    // for each portion of the context record controlled by a flag
    // whose value is set, it is assumed that that portion of the
    // context record contains valid context. If the context record
    // is being used to modify a thread's context, then only that
    // portion of the threads context will be modified.
    //
    // If the context record is used as an IN OUT parameter to capture
    // the context of a thread, then only those portions of the thread's
    // context corresponding to set flags will be returned.
    //
    // The context record is never used as an OUT only parameter.
    //

    ULONG ContextFlags;

    //
    // Debug registers
    //

    //
    // This section is specified/returned if the ContextFlags word contains
    // the flag CONTEXT_INTEGER.
    //
    ULONG R0;
    ULONG R1;
    ULONG R2;
    ULONG R3;
    ULONG R4;
    ULONG R5;
    ULONG R6;
    ULONG R7;
    ULONG R8;
    ULONG R9;
    ULONG R10;
    ULONG R11;
    ULONG R12;

    //
    // This section is specified/returned if the ContextFlags word contains
    // the flag CONTEXT_CONTROL.
    //
    ULONG Sp;
    ULONG Lr;
    ULONG Pc;
    ULONG Cpsr;

    //
    // This section is specified/returned if the ContextFlags word contains
    // the flag CONTEXT_FLOATING_POINT.
    //
    ULONG Fpscr;
    ULONG Padding;
    union {
        ARM_NEON128 Q[16];
        ULONGLONG D[32];
        ULONG S[32];
    } DUMMYUNIONNAME;

    //
    // This section is specified/returned if the ContextFlags word contains
    // the flag CONTEXT_DEBUG_REGISTERS.
    //
    ULONG Bvr[ARM_MAX_BREAKPOINTS];
    ULONG Bcr[ARM_MAX_BREAKPOINTS];
    ULONG Wvr[ARM_MAX_WATCHPOINTS];
    ULONG Wcr[ARM_MAX_WATCHPOINTS];

    ULONG Padding2[2];

} ARM_CONTEXT, *PARM_CONTEXT;


#define ARM64_MAX_BREAKPOINTS     8
#define ARM64_MAX_WATCHPOINTS     2

typedef union _ARM64_NEON128 {
    struct _DUMMYSTRUCTNAME {
        ULONGLONG Low;
        LONGLONG High;
    } DUMMYSTRUCTNAME;
    double D[2];
    float S[4];
    USHORT H[8];
    UCHAR B[16];
} ARM64_NEON128, *PARM64_NEON128;

typedef struct _ARM64_CONTEXT {

    //
    // Control flags.
    //

    /* +0x000 */ ULONG ContextFlags;

    //
    // Integer registers
    //

    /* +0x004 */ ULONG Cpsr;       // NZVF + DAIF + CurrentEL + SPSel
    /* +0x008 */ ULONG64 X[29];
    /* +0x0f0 */ ULONG64 Fp;
    /* +0x0f8 */ ULONG64 Lr;
    /* +0x100 */ ULONG64 Sp;
    /* +0x108 */ ULONG64 Pc;

    //
    // Floating Point/NEON Registers
    //

    /* +0x110 */ ARM64_NEON128 V[32];
    /* +0x310 */ ULONG Fpsr;
    /* +0x314 */ ULONG Fpcr;

    //
    // Debug registers
    //

    /* +0x318 */ ULONG Bcr[ARM64_MAX_BREAKPOINTS];
    /* +0x338 */ ULONG64 Bvr[ARM64_MAX_BREAKPOINTS];
    /* +0x378 */ ULONG Wcr[ARM64_MAX_WATCHPOINTS];
    /* +0x380 */ ULONG64 Wvr[ARM64_MAX_WATCHPOINTS];
    /* +0x390 */

} ARM64_CONTEXT, *PARM64_CONTEXT;

//
// sc_status
//
typedef struct _CPU_STATUS
{
    UINT32      NS : 1;
    UINT32      WDT : 1;
    UINT32      SGI : 1;
    UINT32      WarmBoot : 1;
    UINT32      DBI_RSVD : 1;
    UINT32      CPU_Context : 1;
    UINT32      Reserved : 26;
}CPU_STATUS, *PCPU_STATUS;


//
// tzbsp_mon_cpu_ctx_t
//
typedef struct _SECURE_CPU_CONTEXT
{
    UINT32      Mon_Lr;
    UINT32      Mon_Spsr;
    UINT32      Usr_R0;
    UINT32      Usr_R1;
    UINT32      Usr_R2;
    UINT32      Usr_R3;
    UINT32      Usr_R4;
    UINT32      Usr_R5;
    UINT32      Usr_R6;
    UINT32      Usr_R7;
    UINT32      Usr_R8;
    UINT32      Usr_R9;
    UINT32      Usr_R10;
    UINT32      Usr_R11;
    UINT32      Usr_R12;
    UINT32      Usr_R13;
    UINT32      Usr_R14;
    UINT32      Irq_Spsr;
    UINT32      Irq_R13;
    UINT32      Irq_R14;
    UINT32      Svc_Spsr;
    UINT32      Svc_R13;
    UINT32      Svc_R14;
    UINT32      Abt_Spsr;
    UINT32      Abt_R13;
    UINT32      Abt_R14;
    UINT32      Und_Spsr;
    UINT32      Und_R13;
    UINT32      Und_R14;
    UINT32      Fiq_Spsr;
    UINT32      Fiq_R8;
    UINT32      Fiq_R9;
    UINT32      Fiq_R10;
    UINT32      Fiq_R11;
    UINT32      Fiq_R12;
    UINT32      Fiq_R13;
    UINT32      Fiq_R14;
}SECURE_CPU_CONTEXT, *PSECURE_CPU_CONTEXT;

#define IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define IMAGE_FILE_MACHINE_ARMNT             0x01c4  // ARM Thumb-2 Little-Endian
#define IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.

#define X86_SIZE_OF_80387_REGISTERS      80
#define MAXIMUM_SUPPORTED_EXTENSION     512
#define X86_CONTEXT_ALIGN               4

// @@BEGIN_NOTPUBLICCODE
typedef struct _X86_FLOATING_SAVE_AREA {
    ULONG   ControlWord;
    ULONG   StatusWord;
    ULONG   TagWord;
    ULONG   ErrorOffset;
    ULONG   ErrorSelector;
    ULONG   DataOffset;
    ULONG   DataSelector;
    UCHAR   RegisterArea[X86_SIZE_OF_80387_REGISTERS];
    ULONG   Cr0NpxState;
} X86_FLOATING_SAVE_AREA;

//
// Simulated context structure for the 16-bit environment
//

typedef struct _X86CONTEXT {
    //
    // The flags values within this flag control the contents of
    // a CONTEXT record.
    //
    // If the context record is used as an input parameter, then
    // for each portion of the context record controlled by a flag
    // whose value is set, it is assumed that that portion of the
    // context record contains valid context. If the context record
    // is being used to modify a threads context, then only that
    // portion of the threads context will be modified.
    //
    // If the context record is used as an IN OUT parameter to capture
    // the context of a thread, then only those portions of the thread's
    // context corresponding to set flags will be returned.
    //
    // The context record is never used as an OUT only parameter.
    //

    ULONG ContextFlags;

    //
    // This section is specified/returned if CONTEXT_DEBUG_REGISTERS is
    // set in ContextFlags.  Note that CONTEXT_DEBUG_REGISTERS is NOT
    // included in CONTEXT_FULL.
    //

    ULONG   Dr0;
    ULONG   Dr1;
    ULONG   Dr2;
    ULONG   Dr3;
    ULONG   Dr6;
    ULONG   Dr7;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_FLOATING_POINT.
    //

    X86_FLOATING_SAVE_AREA FloatSave;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_SEGMENTS.
    //

    ULONG   SegGs;
    ULONG   SegFs;
    ULONG   SegEs;
    ULONG   SegDs;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_INTEGER.
    //

    ULONG   Edi;
    ULONG   Esi;
    ULONG   Ebx;
    ULONG   Edx;
    ULONG   Ecx;
    ULONG   Eax;

    //
    // This section is specified/returned if the
    // ContextFlags word contians the flag CONTEXT_CONTROL.
    //

    ULONG   Ebp;
    ULONG   Eip;
    ULONG   SegCs;              // MUST BE SANITIZED
    ULONG   EFlags;             // MUST BE SANITIZED
    ULONG   Esp;
    ULONG   SegSs;

    //
    // This section is specified/returned if the ContextFlags word
    // contains the flag CONTEXT_EXTENDED_REGISTERS.
    // The format and contexts are processor specific
    //

    UCHAR   ExtendedRegisters[MAXIMUM_SUPPORTED_EXTENSION];

} X86_CONTEXT, *PX86_CONTEXT;


//
// taken from %SDXROOT%\publicint\sdktools\inc\ntdbg.h
//
typedef struct _AMD64_M128 {
    ULONGLONG Low;
    LONGLONG High;
} AMD64_M128, *PAMD64_M128;

typedef struct _AMD64_XMM_SAVE_AREA32 {
    USHORT ControlWord;
    USHORT StatusWord;
    UCHAR TagWord;
    UCHAR Reserved1;
    USHORT ErrorOpcode;
    ULONG ErrorOffset;
    USHORT ErrorSelector;
    USHORT Reserved2;
    ULONG DataOffset;
    USHORT DataSelector;
    USHORT Reserved3;
    ULONG MxCsr;
    ULONG MxCsr_Mask;
    AMD64_M128 FloatRegisters[8];
    AMD64_M128 XmmRegisters[16];
    UCHAR Reserved4[96];
} AMD64_XMM_SAVE_AREA32, *PAMD64_XMM_SAVE_AREA32;


#pragma warning(disable:4201) // nonstandard extension: nameless struct/union
typedef struct _AMD64_CONTEXT {

    //
    // Register parameter home addresses.
    //

    ULONG64 P1Home;
    ULONG64 P2Home;
    ULONG64 P3Home;
    ULONG64 P4Home;
    ULONG64 P5Home;
    ULONG64 P6Home;

    //
    // Control flags.
    //

    ULONG ContextFlags;
    ULONG MxCsr;

    //
    // Segment Registers and processor flags.
    //

    USHORT SegCs;
    USHORT SegDs;
    USHORT SegEs;
    USHORT SegFs;
    USHORT SegGs;
    USHORT SegSs;
    ULONG EFlags;

    //
    // Debug registers
    //

    ULONG64 Dr0;
    ULONG64 Dr1;
    ULONG64 Dr2;
    ULONG64 Dr3;
    ULONG64 Dr6;
    ULONG64 Dr7;

    //
    // Integer registers.
    //

    ULONG64 Rax;
    ULONG64 Rcx;
    ULONG64 Rdx;
    ULONG64 Rbx;
    ULONG64 Rsp;
    ULONG64 Rbp;
    ULONG64 Rsi;
    ULONG64 Rdi;
    ULONG64 R8;
    ULONG64 R9;
    ULONG64 R10;
    ULONG64 R11;
    ULONG64 R12;
    ULONG64 R13;
    ULONG64 R14;
    ULONG64 R15;

    //
    // Program counter.
    //

    ULONG64 Rip;

    //
    // Floating point state.
    //

    union {
        AMD64_XMM_SAVE_AREA32 FltSave;
        struct {
            AMD64_M128 Header[2];
            AMD64_M128 Legacy[8];
            AMD64_M128 Xmm0;
            AMD64_M128 Xmm1;
            AMD64_M128 Xmm2;
            AMD64_M128 Xmm3;
            AMD64_M128 Xmm4;
            AMD64_M128 Xmm5;
            AMD64_M128 Xmm6;
            AMD64_M128 Xmm7;
            AMD64_M128 Xmm8;
            AMD64_M128 Xmm9;
            AMD64_M128 Xmm10;
            AMD64_M128 Xmm11;
            AMD64_M128 Xmm12;
            AMD64_M128 Xmm13;
            AMD64_M128 Xmm14;
            AMD64_M128 Xmm15;
        };
    };

    //
    // Vector registers.
    //

    AMD64_M128 VectorRegister[26];
    ULONG64 VectorControl;

    //
    // Special debug control registers.
    //

    ULONG64 DebugControl;
    ULONG64 LastBranchToRip;
    ULONG64 LastBranchFromRip;
    ULONG64 LastExceptionToRip;
    ULONG64 LastExceptionFromRip;
} AMD64_CONTEXT, *PAMD64_CONTEXT;

// @@END_NOTPUBLICCODE

//
// tzbsp_cpu_ctx_t
//
typedef struct _NON_SECURE_CPU_CONTEXT
{
    SECURE_CPU_CONTEXT  Saved_Ctx;
    UINT32              Mon_Sp;
    UINT32              Wdog_Pc;
}NON_SECURE_CPU_CONTEXT, *PNON_SECURE_CPU_CONTEXT;

//
// wdt_sts
//
typedef struct _WDOG_STATUS
{
    UINT32  Status;
} WDOG_STATUS, *PWDOG_STATUS;

typedef struct _AP_REG_B_FAMILY_HEADER
{
    UINT32                      Magic;
    UINT32                      Version;
    UINT32                      CPU_Count;
    //  Followed by array of CPU_STATUS
    //  Followed by array of NON_SECURE_CPU_CONTEXT
    //  Followed by one secure cpu context
    //  Followed by Array of WDOG_STATUS
}AP_REG_B_FAMILY_HEADER, *PAP_REG_B_FAMILY_HEADER;

# pragma pack ()


//
// ARM hardware structures
//
// The following bit arrangements and semantics assume a ARMv7 style MMU
// configured for the NT profile which is defined here as:
//    Subpages disabled (CP15 Reg1 XP=1)
//
// A Page Table Entry on an ARM has the following definition.
//

typedef struct _HARDWARE_PTE {
    ULONG Valid : 2;            // Bit 0 is NX only if bit 1 is true
    ULONG CacheType : 2;
    ULONG Accessed : 1;
    ULONG Owner : 1;
    ULONG TypeExtension : 1;    // always zero
    ULONG Writable : 1;         // OS managed
    ULONG CopyOnWrite : 1;      // OS managed
    ULONG ReadOnly : 1;         // Inverse of Dirty
    ULONG LargePage : 1;        // OS managed
    ULONG NonGlobal : 1;
    ULONG PageFrameNumber : 20;
} HARDWARE_PTE, *PHARDWARE_PTE;


typedef struct _ARM64_HARDWARE_PTE {
    ULONGLONG Valid : 1;
    ULONGLONG NotLargePage : 1;         // ARM Large page bit is inverted !
    ULONGLONG CacheType : 2;            // Lower 2 bits for cache type encoding
    ULONGLONG OsAvailable2 : 1;         // Memory Attribute Index (can be Mm usage)
    ULONGLONG NonSecure : 1;
    ULONGLONG Owner : 1;                // 0 == kernel, 1 = user
    ULONGLONG NotDirty : 1;             // 0 == modified (written), 1 == clean (ronly)
    ULONGLONG Shareability : 2;
    ULONGLONG Accessed : 1;
    ULONGLONG NonGlobal : 1;
    ULONGLONG PageFrameNumber : 36;
    ULONGLONG reserved1 : 4;
    ULONGLONG ContiguousBit : 1;
    ULONGLONG PrivilegedNoExecute : 1;
    ULONGLONG UserNoExecute : 1;
    ULONGLONG Writable : 1;             // OS managed
    ULONGLONG CopyOnWrite : 1;          // OS managed
    ULONGLONG OsAvailable : 2;          // 2 bits available for Mm usage
    ULONGLONG PxnTable : 1;
    ULONGLONG UxnTable : 1;
    ULONGLONG ApTable : 2;
    ULONGLONG NsTable : 1;
} ARM64_HARDWARE_PTE, *PARM64_HARDWARE_PTE;

#define ARM_PDE_MASK 0xFFC00000
#define ARM_LARGE_PAGE_ADDR_OFFSET_MASK 0x003FFFFF

#define ARM64_1GB_PPE_PAGE_MASK 0x0000FFFFC0000000UI64
#define ARM64_1GB_PPE_ADDR_OFFSET_MASK 0x3FFFFFFF

#define ARM64_LARGE_PAGE_PDE_MASK 0x0000FFFFFFE00000UI64
#define ARM64_LARGE_PAGE_ADDR_OFFSET_MASK 0x1FFFFF

#define  ARM64_OFFSET_WITHIN_PAGE_MASK 0x0000000000000FFF

//
// Device Name
//
#define NAME_EXECUTABLE L"offdumptool.exe"

VOID
DumpGUID(
    _In_ GUID*  Guid
);



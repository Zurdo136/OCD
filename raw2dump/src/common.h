#pragma once
#include <initguid.h>
#include <stdlib.h>
#include <stdio.h>
#include <devioctl.h>

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#ifndef ULONGLIMIT
#define ULONGLIMIT (4*1024*1024)
#endif

#define EXTENDED_HIDDEN 2048
#define DEFUALT_ALIGNMENT (1024*1024)
#define MIN_PARTITION_SIZE 2*1024*1024*1024LL // 2GB min
#define DEFUALT_SECTOR_SZ 512
#define DEFAULT_DMP_BUF_SZ (2*1024*1024)
#define VOLUME_NAME_SIZE 50
#define DEVICE_NAME_SIZE 40
#define MAX_FILE_SIZE 0xFFFFFFF0000 // a ntfs limit
#define MAX_ALLOWED_DDR_SECTIONS 10
// {0x66C9B323-F7FC-48B6-BF96-6F32E335A428}
static const GUID RAWDUMP_GUID = 
{ 0x66C9B323, 0xF7FC, 0x48B6, { 0xBF, 0x96, 0x6F, 0x32, 0xE3, 0x35, 0xA4, 0x28 } };

typedef struct _DDRSection{
     PWSTR DDRFileName;
     LARGE_INTEGER DDRBase;
     BOOL AlreadyMerged;
}DDRSection, *pDDRSsection;

#ifdef ANY_SIZE
#undef ANY_SIZE
#endif
#define ANY_SIZE 1



typedef enum
{    RAW_DUMP_SECTION_TYPE_Reserved = 0,
    RAW_DUMP_SECTION_TYPE_Maximum = 4
} RAW_DUMP_SECTION_TYPE;


#define RAW_DUMP_SECTION_HEADER_NAME_LENGTH 0x14

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

//
// These structures are defined by Qualcomm
//
typedef union _ARM64_NEON128 {
    struct _LONG128 {
        ULONGLONG Low;
        LONGLONG High;
    } LONG128;
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

#define ARM_PDE_MASK 0xFFC00000
#define ARM_LARGE_PAGE_ADDR_OFFSET_MASK 0x003FFFFF

#define ARM64_1GB_PPE_PAGE_MASK 0x0000FFFFC0000000UI64
#define ARM64_1GB_PPE_ADDR_OFFSET_MASK 0x3FFFFFFF

#define ARM64_LARGE_PAGE_PDE_MASK 0x0000FFFFFFE00000UI64
#define ARM64_LARGE_PAGE_ADDR_OFFSET_MASK 0x1FFFFF

//
// sc_status
//
//typedef struct _CPU_STATUS
//{
//    UINT32      NS:1;
//    UINT32      WDT:1;
//    UINT32      SGI:1;
//    UINT32      WarmBoot:1;
//    UINT32      DBI_RSVD:1;
//    UINT32      CPU_Context:1;
//    UINT32      Reserved:26;
//}CPU_STATUS, *PCPU_STATUS;


#define IMAGE_FILE_MACHINE_ARM               0x01c0  // ARM Little-Endian
#define IMAGE_FILE_MACHINE_ARMNT             0x01c4  // ARM Thumb-2 Little-Endian
#define IMAGE_FILE_MACHINE_I386              0x014c  // Intel 386.

#define X86_SIZE_OF_80387_REGISTERS      80
#define MAXIMUM_SUPPORTED_EXTENSION     512
#define X86_CONTEXT_ALIGN               4

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


#define AP_REG_STRUCTURE_MAGIC_VALUE    0x44434151
#define AP_REG_STRUCTURE_V2             0x2
#define AP_REG_STRUCTURE_V3             0x3
#define AP_REG_STRUCTURE_V4             0x4
#define AP_REG_MAX_CPUS                 0x4

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

#define ARM64_BASE_VIRT                0xFFFFF6FB7DBED000UI64
#define ARM64_PAGE_SIZE                0x1000
#define ARM64_PAGE_SHIFT               12L
#define ARM64_MM_PTE_TRANSITION_MASK   0x800
#define ARM64_MM_PTE_PROTOTYPE_MASK    0x400
#define ARM64_VALID_PFN_MASK           0x0000FFFFFFFFF000UI64
#define ARM64_VALID_PFN_SHIFT          12
#define ARM64_USED_VA_BITS             48
#define ARM64_UNUSED_VA_MASK           0xffff
#define ARM64_PML4E_SHIFT              39
#define ARM64_PML4E_MASK               0x1ff
#define ARM64_PDPE_SHIFT               30
#define ARM64_PDPE_MASK                0x1ff
#define ARM64_PDE_SHIFT                21
#define ARM64_PDE_MASK                 0x1ff
#define ARM64_PTE_SHIFT                12
#define ARM64_PTE_MASK                 0x1ff
#define ARM64_LARGE_PAGE_MASK          0x80
#define ARM64_LARGE_PAGE_SIZE          (2 * 1024 * 1024)
#define ARM64_1GB_PAGE_MASK            0x80
#define ARM64_1GB_PAGE_SIZE            (1024 * 1024 * 1024)
#define ARM64_PDBR_MASK                ARM64_VALID_PFN_MASK

VOID
DumpGUID(
    _In_ GUID*  Guid
    );


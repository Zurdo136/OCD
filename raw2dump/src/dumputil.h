/*++

Copyright (c) October 2014 Microsoft Corporation, All Rights Reserved

Module Name: dumputil.h

Environment: User Mode

--*/

#pragma once

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>
#include <rawdump.h>
#include <wdbgexts.h>

#include "DEVICE_IO.h"
#include "Device_Specific.h"
#include "KdDebuggerData.h"
#include "DbgClient.h"
#include "ntiodump.h"
#include "common.h"
#include "logging.h"

// Add TraceLogging header files
#include <TraceLoggingProvider.h>
#include <telemetry\MicrosoftTelemetry.h>
#include <TraceLoggingActivity.h>

TRACELOGGING_DECLARE_PROVIDER(g_hRaw2DumpTraceLoggingProvider);


#define Add2Ptr(_Ptr, _Length) ((PVOID)((PUCHAR)(_Ptr) + (_Length)))

#define PAGE_SHIFT 12L
#define PAGES_TO_BYTES(Pages)             ((((UINT64)(Pages)) << (PAGE_SHIFT)))
#define PFN_TO_PHYSADDR(Pages)            PAGES_TO_BYTES(Pages)

#define RawDumpTableSize(nSections)       (nSections * sizeof(RAW_DUMP_SECTION_HEADER))

// 
// buffer size for context's io buffer.
// 
#define IO_BUFFER_SIZE 0x800000

// only for test. to be replaced by ETW logging
//#define LogLibInfoPrintf wprintf
#define LogLibInfoPrintf __noop
#define VERBOSE_MSGS 1


#define LOCAL_DEBUGGING
//
// TraceError:
// DescriptionText - high level explanation of the error
// ErrorClass - one of "HRESULT", "NTSTATUS" or "WIN32"
// ErrorCode - actual error code
//
#ifdef LOCAL_DEBUGGING
#define TraceError(DescriptionText, ErrorClass, ErrorCode)         \
        printf("%s (%s == 0x%x)\n", DescriptionText, ErrorClass, ErrorCode)
#else
#define TraceError(DescriptionText, ErrorClass, ErrorCode)         \
            TraceLoggingWrite(                                     \
                g_hRaw2DumpTraceLoggingProvider,                   \
                "Raw2DumpError",                                   \
                TraceLoggingValue(DescriptionText, "Description"), \
                TraceLoggingValue(ErrorCode, ErrorClass),          \
                TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY)); \
                DmpLog("%s %s 0x%x\n", DescriptionText, ErrorClass, ErrorCode)
#endif 

#define TraceHRESULT(DescriptionText, ErrorCode)  TraceError(DescriptionText, "HRESULT", ErrorCode)
#define TraceNTSTATUS(DescriptionText, ErrorCode) TraceError(DescriptionText, "NTSTATUS", ErrorCode)

//
// TraceInfo:
// Log a single string at "info" level
//
#ifdef LOCAL_DEBUGGING
#define TraceInfo(DescriptionText) \
        printf("%s\n", DescriptionText)
#else
#define TraceInfo(DescriptionText) \
            TraceLoggingWrite(                                     \
                g_hRaw2DumpTraceLoggingProvider,                   \
                "Raw2DumpInfo",                                    \
                TraceLoggingValue(DescriptionText, "Description"), \
                TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY))
#endif

//
// TraceInfo*:
// Log 1-4 values at "info" level
//
#define TraceInfo1(DescriptionText, ValueName1, Value1) \
            TraceLoggingWrite(                                     \
                g_hRaw2DumpTraceLoggingProvider,                   \
                "Raw2DumpInfo",                                    \
                TraceLoggingValue(DescriptionText, "Description"), \
                TraceLoggingValue(Value1, ValueName1),             \
                TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY)); \
                DmpLog("%s %s 0x%x\n", DescriptionText, ValueName1, Value1)

#define TraceInfo2(DescriptionText, ValueName1, Value1, ValueName2, Value2) \
            TraceLoggingWrite(                                     \
                g_hRaw2DumpTraceLoggingProvider,                   \
                "Raw2DumpInfo",                                    \
                TraceLoggingValue(DescriptionText, "Description"), \
                TraceLoggingValue(Value1, ValueName1),             \
                TraceLoggingValue(Value2, ValueName2),             \
                TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY)); \
                DmpLog("%s %s 0x%x %s 0x%x\n", DescriptionText, ValueName1, Value1, ValueName2, Value2)

#define TraceInfo3(DescriptionText, ValueName1, Value1, ValueName2, Value2, ValueName3, Value3) \
            TraceLoggingWrite(                                     \
                g_hRaw2DumpTraceLoggingProvider,                   \
                "Raw2DumpInfo",                                    \
                TraceLoggingValue(DescriptionText, "Description"), \
                TraceLoggingValue(Value1, ValueName1),             \
                TraceLoggingValue(Value2, ValueName2),             \
                TraceLoggingValue(Value3, ValueName3),             \
                TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY)); \
                DmpLog("%s %s %I64u %s %I64u %s %I64u \n", DescriptionText, ValueName1, Value1, ValueName2, Value2, ValueName3, Value3)

#define TraceInfo4(DescriptionText, ValueName1, Value1, ValueName2, Value2, ValueName3, Value3, ValueName4, Value4) \
            TraceLoggingWrite(                                     \
                g_hRaw2DumpTraceLoggingProvider,                   \
                "Raw2DumpInfo",                                    \
                TraceLoggingValue(DescriptionText, "Description"), \
                TraceLoggingValue(Value1, ValueName1),             \
                TraceLoggingValue(Value2, ValueName2),             \
                TraceLoggingValue(Value3, ValueName3),             \
                TraceLoggingValue(Value4, ValueName4),             \
                TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY)); \
                DmpLog("%s %s %I64u %s %I64u %s %I64u %s %I64u \n", DescriptionText, ValueName1, Value1, ValueName2, Value2, ValueName3, Value3, ValueName4, Value4)

//
// TraceExpectedActual:
// Log expected values vs actual when a mismatch is encountered.
//
#define TraceExpectedActual(DescriptionText, ExpectedValue, ActualValue) \
            TraceInfo2(DescriptionText, "Expected", ExpectedValue, "Actual", ActualValue)

#pragma warning(disable: 4201) // nonstandard extension used : nameless struct/union
// 
// Physical Address Extension
typedef struct _PAE_ADDRESS {
    union {
        struct {
            ULONG Offset:12;                  // 0  .. 11
            ULONG Table:9;                    // 12 .. 20
            ULONG Directory:9;                // 21 .. 29
            ULONG DirectoryPointer:2;         // 30 .. 31
        };
        struct {
            ULONG Offset:21;
            ULONG Directory:9;
            ULONG DirectoryPointer:2;
        } LargeAddress;

        ULONG DwordPart;
    };
} PAE_ADDRESS, *PPAE_ADDRESS;


typedef enum {
    MEMORY_OS    = 1,
    MEMORY_NONOS = 2,
    MEMORY_NA = 3
} MEMORY_TYPE;


// Enum to track the dump header status in case a fake dump header may be needed
typedef enum {
    DHS_UNKNOWN   = 0,
    DHS_NOT_FOUND = 1,
    DHS_INVALID   = 2,
    DHS_NO_SVINFO = 3,
    DHS_VALID     = 4,
} DUMP_HEADER_STATUS;

// Enum for BugCheckParameter2 for the fake dump header case
typedef enum {
   FAKE_PARAM2_NO_AP_REG = 0,
   FAKE_PARAM2_INVALID_DUMP_HEADER = 1
} FAKE_PARAM2_REASON;

//
// DDR Section Memory Map
// Based on DDR Seciton plus some 
// additional info to facilitate reading
// DDR sections
//
typedef struct _DDR_MEMORY_MAP
{
    UINT64      Base;
    //
    // End is Base + Size - 1
    //
    UINT64      End;
    UINT64      Size;
    UINT64      Offset;

    //
    // Contiguous tells us whether the current section is 
    // contiguous from previous section in terms of physical
    // addresses. What that means is that previous section's
    // End is just one byte less than this section's Base. 
    //
    BOOLEAN     Contiguous;
    MEMORY_TYPE Type;
    UINT32      DDRIndex;
}DDR_MEMORY_MAP, *PDDR_MEMORY_MAP;


//
// Struct for in memory diag buffer.
// TODO: Use WP header file
typedef struct _IN_MEM_DATA_INFO {
    PVOID               DataVA;
    PHYSICAL_ADDRESS    DataPA;
    UINT32              Size;
} IN_MEM_DATA_INFO, *PIN_MEM_DATA_INFO;


//
// Global context struct. 
//
typedef struct _DMP_CONTEXT
{
    // File based information.
    DEVICE_IO                                           hRawFile;
    LARGE_INTEGER                                       fileOffset;
    LARGE_INTEGER                                       RawDumpFileLength;
    
    // General dump related.
    LARGE_INTEGER                                       ConfigTableAddress;
    RAW_DUMP_HEADER                                     RawDumpHeader;
    PRAW_DUMP_SECTION_HEADER                            RawDumpSectionTable;
    UINT32                                              OfflineMemoryDumpUseCapability;
    PRAW_DUMP_SECTION_HEADER                            DDRSections;
    UINT32                                              DDRMemoryMapCount;
    PDDR_MEMORY_MAP                                     DDRMemoryMap;
    UINT64                                              TotalDDRSizeInBytes;
    UINT64                                              TotalSVSpecificSizeInBytes;
    UINT64                                              TotalCpuContextSizeInBytes;
    UINT32                                              DDRSectionCount;
    UINT32                                              FirstDDRFileIdIndex;
    UINT32                                              SVSectionCount;
    UINT32                                              CPUContextSectionCount;
    UINT32                                              MissingFlagsCount;
    UINT32                                              InvalidSectionCount;
    UINT32                                              InsufficientStorageSectionsCount;
    UINT32                                              InvalidVersionCount;
    UINT32                                              DDRSectionsOverlapCount;
    UINT32                                              DDRSectionFragmentationCount;
    UINT64                                              LargestSVSpecificSectionSize;
    UINT64                                              TotalNonOSDDRSizeInBytes;
    BOOL                                                Is64Bit;

    PVOID                                               IoBuffer;

    //
    // Dump file info
    //
    LPWSTR                                              WindowsDumpFilePath;
    HANDLE                                              WindowsDumpHandle;
    LARGE_INTEGER                                       WindowsDumpFileOffset;
    
    LPWSTR                                              rawdumpInfoFilePath;
    HANDLE                                              rawdumpInfoFileHandle;

    LARGE_INTEGER                                       DDRFileOffset;
    LARGE_INTEGER                                       SecondaryDataOffset;
    LARGE_INTEGER                                       ActualDumpFileUsedInBytes;
    BOOL                                                DumpDDR;

   
    //
    // DDR Reconstruction
    //
    UINT32                                              CompleteMemoryMapCount;
    PDDR_MEMORY_MAP                                     CompleteMemoryMap;

    // CPU Context
    LARGE_INTEGER                                       X86ContextPA;

    //
    // Data from Windows
    //
    LARGE_INTEGER                                       DumpHeaderPA;
    UINT64                                              DumpHeaderOffset;
    LARGE_INTEGER                                       DumpHeaderAddress;
    PDUMP_HEADER32                                      DumpHeader32;
    PDUMP_HEADER64                                      DumpHeader64;
    DUMP_HEADER_STATUS                                  DumpHeaderStatus;
    PPHYSICAL_MEMORY_DESCRIPTOR32                       MemoryDescriptors;
    PPHYSICAL_MEMORY_DESCRIPTOR64                       MemoryDescriptors64;
    UINT64                                              SizeAccordingToMemoryDescriptors;
    UINT32                                              SecondaryDataBlobCount;

    //
    // Data to decode KdDebuggerDataBlock
    //
    LARGE_INTEGER                                       KdDebuggerDataBlockPA;
    PKDDEBUGGER_DATA64                                  KdDebuggerDataBlock;
    BOOL                                                GetKdBlockPAFromImMemData;

    DEVICE_SPECIFIC_INFO_STRUCT_TEMPLATE                DeviceSpecificInfo;


    ULARGE_INTEGER                                      DumpInstance;

    //
    // AP REG info
    //
    LARGE_INTEGER                                       APRegAddress;
    PAP_REG_B_FAMILY_HEADER                             ApReg;
    LARGE_INTEGER                                       ArmContextPA;
    BOOL                                                isAPREG64;

    // x86 data
    LARGE_INTEGER                                       CPUContextAddress;

    UINT32                                              BugCheckCode;
    UINT64                                              BugCheckParam1;
    UINT64                                              BugCheckParam2;
    UINT64                                              BugCheckParam3;
    UINT64                                              BugCheckParam4;


    IN_MEM_DATA_INFO                                    InMemDataInfo;

    //
    // The following flag indicates if the device specific info is present embed 
    // in the rawdump.bin file. If so, we don't need rawdumpinfo.xml file.
    //
    BOOL                                                IsDeviceInfoInRawDump;
} DMP_CONTEXT, *PDMP_CONTEXT;


// 
// Section Type values in the RAW_DUMP_HEADER
//
static PSTR RawDumpSectionTypeStringTable[] = { "RESERVED",
                                                "DDR_RANGE",
                                                "CPU_CONTEXT",
                                                "SV_SPECIFIC"
                                               };

// 
// The table type mapping GUID to name of the GUID.
//
typedef struct _SV_SPECIFIC_NAME_TO_GUID
{
    GUID    Guid;
    UCHAR   Name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
}SV_SPECIFIC_NAME_TO_GUID;


#define PAGE_SIZE                           0x1000
#define DUMP_HEADERSIG_LENGTH               8
#define ARM_VALID_PFN_MASK                  0xFFFFF000
#define ARMV7_TTBCRN_MASK                   0x7
#define INVALID_AP_REG                      0xFFFFFFFF

#define SECUREBOOT_DEBUG_GUID \
{0x0cdad82e, 0xd839, 0x4754, \
{0x89, 0xa1, 0x84, 0x4a, 0xb2, 0x82, 0x31, 0x2b}};


//
// Carved out regions in the rawdump is only present in the DDR physical addresses less
// than 512MB. This macro is to help stop tagging Non OS memories beyond 512 MB 
// because there could be memory holes in the physical address space and OS has 
// absolutely no ways of knowing it.
//
#define NON_OS_MEMORY_LIMIT 0x20000000

//
// Beyond 512MB, a non OS memory range of size more than 256MB will be 
// assumed to be a hole.
//
#define NON_OS_SIZE_LIMIT 0x10000000

//
// Copied from other headers.
//

#define ADDRESS_NOT_PRESENT ((UINT64)-1)

#define MAX_KDDEBUGGER_BLOCK_SIZE 0x10000

//
// The size of the buffer thats appended at the end of the rawdump.
//
#define DEVICE_SPECIFIC_INFO_BUFFER_LENGTH 1024


//
// --------------------------- Function Prototypes ------------------------------------------------------------
//

VOID DumpRawDumpHeader(
    _In_ PRAW_DUMP_HEADER            Header,
    _In_ PRAW_DUMP_SECTION_HEADER    SectionTable
);

HRESULT
ExtractRawDumpFile(
    PDMP_CONTEXT Context,
    LPCWSTR FileName);

NTSTATUS
ReadFromDDRSectionByVirtualAddress32(
    _In_ PDMP_CONTEXT Context,
    _In_ UINT32 VirtualAddress,
    _In_ UINT32 Length,
    _Out_ PVOID Buffer,
    _Out_opt_ PLARGE_INTEGER PhysicalAddress
    );

NTSTATUS
ReadFromDDRSectionByVirtualAddress64(
    _In_ PDMP_CONTEXT Context,
    _In_ UINT64 VirtualAddress,
    _In_ UINT64 Length,
    _Out_ PVOID Buffer,
    _Out_opt_ PLARGE_INTEGER PhysicalAddress
);

NTSTATUS
ReadFromDDRSectionByVirtualAddress(
    _In_ PDMP_CONTEXT Context,
    _In_ UINT64 VirtualAddress,
    _In_ UINT64 Length,
    _Out_ PVOID Buffer,
    _Out_opt_ PLARGE_INTEGER PhysicalAddress
);

NTSTATUS
ReadFromDDRSectionByPhysicalAddress(
    _In_ PDMP_CONTEXT Context,
    _In_ LARGE_INTEGER PhysicalAddress,
    _In_ UINT32 Length,
    _Out_ PVOID Buffer
    );

HRESULT UpdateContextFromXml(_Inout_ DMP_CONTEXT * pContext);
NTSTATUS UpdateContextWithAPRegLegacy(_Inout_ PDMP_CONTEXT Context);

NTSTATUS
VirtualToPhysical(
    _In_ PDMP_CONTEXT Context,
    _In_ UINT32 VirtualAddress,
    _Out_ PLARGE_INTEGER PhysicalAddress
    );

NTSTATUS
VirtualToPhysical64(
    _In_ PDMP_CONTEXT Context,
    _In_ UINT64 VirtualAddress,
    _Out_ PLARGE_INTEGER PhysicalAddress
    );


NTSTATUS
WriteToDumpByPhysicalAddress(
    _Inout_ PDMP_CONTEXT Context,
    _In_ LARGE_INTEGER PhysicalAddress,
    _In_ UINT32 Size,
    _Inout_ PVOID Buffer
    );

NTSTATUS
GetKdDebuggerBlockFromInMemAddresses(
_Inout_ PDMP_CONTEXT Context,
_In_ PKDDEBUGGER_DATA64 dbgDataBlock,
_Inout_ PLARGE_INTEGER pdbgDataHeaderPA
);

NTSTATUS
GetKdDBlockViaMemoryTranslation(
_Inout_ PDMP_CONTEXT        Context,
_In_    ULONG64             dbgDataHeaderVA,
_In_    PKDDEBUGGER_DATA64  pdbgDataBlock,
_Inout_ PLARGE_INTEGER      pdbgDataHeaderPA
);


NTSTATUS ExtractRawDumpToFile(PDMP_CONTEXT Context);
NTSTATUS VerifyRawDumpSectionTable(PDMP_CONTEXT Context);
HRESULT BuildCompleteMemoryMap(_Inout_ PDMP_CONTEXT Context);
NTSTATUS BuildDDRMemoryMap(PDMP_CONTEXT Context);
VOID DumpGUID(_In_ GUID*  Guid);
NTSTATUS ExtractWindowsDumpFile(PDMP_CONTEXT Context);
HRESULT GetDumpHeader(_Inout_ PDMP_CONTEXT Context);
HRESULT ValidateDDRAgainstPhysicalMemoryBlock(_Inout_ PDMP_CONTEXT Context);
HRESULT InitDumpFile(_Inout_ PDMP_CONTEXT Context);
HRESULT VerifyRawDumpHeader(PDMP_CONTEXT Context);
HRESULT WriteDumpHeader(_Inout_ PDMP_CONTEXT Context);
HRESULT WriteDDR(_Inout_ PDMP_CONTEXT Context);
HRESULT WriteInMemDiagBuffer(_Inout_ PDMP_CONTEXT Context);
HRESULT WriteFakeDumpHeader(_Inout_ PDMP_CONTEXT Context);
NTSTATUS GetKdDebuggerDataBlock(_Inout_ PDMP_CONTEXT Context);
NTSTATUS UpdateContextX86(_Inout_ PDMP_CONTEXT Context);
NTSTATUS GetX86CPUContext(_Inout_ PDMP_CONTEXT Context);
HRESULT GetAPRegLegacy(_Inout_ PDMP_CONTEXT Context);
BOOL ValidateKdDebuggerDataBlock(_In_ PDBGKD_DEBUG_DATA_HEADER64 Header);
HRESULT WriteSVSpecific(_Inout_ PDMP_CONTEXT Context);
HRESULT UpdateContextFromEmbedDeviceInfo(_Inout_ PDMP_CONTEXT Context);

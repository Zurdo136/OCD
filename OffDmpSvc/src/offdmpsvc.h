/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name: 
    offdmpsvc.h

Environment:
    User Mode

--*/


#pragma once

//
// This define fools the code into thinking that there's a dump and ignores
// AbnormalReset and whether offline dump is enabled. This is good for 
// repro'ing issues during processing without having to get another SBL dump.
// This is also good for investigating errors in processing on other devices. One can
// just put the raw dump to any device (with gptrawutil.exe).
// One must keep in mind that AP_REG may not be valid since the AP_REG address
// in UEFI variable on the current device may not match what's actually in the raw dump
// if the raw dump was generated from another device.
//
//#define ENABLE_REPLAY_MODE


// ------------------------------------- Includes ----------------------------------------

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>
#include <initguid.h>
#include <rawdump.h>
#include "Device_IO.h"
#include "logging.h"
#include "Device_Specific.h"

// nonstandard extension used : bit field types other than int
#pragma warning(disable: 4214) 
#include "ntiodump.h"
#include "wponefourc.h"
#include "wpcrdmpsettings.h"

#include "logging.h"

//
// 1 MB
//
#define DEFAULT_ALIGNMENT (1024*1024)

//
// 2 GB min
//
#define MIN_PARTITION_SIZE 2*1024*1024*1024LL 
#define DEFAULT_SECTOR_SZ 512

//
// 2 MB min
//
#define DEFAULT_DMP_BUF_SZ (2*1024*1024)


//
// an ntfs limit
//
#define MAX_FILE_SIZE 0xFFFFFFF0000 
#define MAX_ALLOWED_DDR_SECTIONS 10
#define CHKLIST_TRUE    0x1
#define CHKLIST_FALSE   0x0

// test 
//#define ENABLE_REPLAY_MODE
#define DISABLE_RESETTING_UEFI_VARS

#define    UNREFERENCED(x)    x


//
// File based raw dump path is either the root "\\" for single SBL dump 
// implementation or "\\99" for multi-dump implementation. Based on that
// the maximum characters required is 4 plus null. Allocate 10 just to be generous.
//
// Keep in mind that Windows drive letters are not carried over to UEFI environment.
//
#define MAX_RAWDUMP_PATH_CHARS          10
#define MAX_RAWDUMP_PATH_BYTES          (MAX_RAWDUMP_PATH_CHARS * sizeof(WCHAR))

#define IsEqualGUID(rguid1, rguid2) (!memcmp(rguid1, rguid2, sizeof(GUID)))

//
// AP Reg defines.
//
#define AP_REG_STRUCTURE_MAGIC_VALUE    0x44434151
#define AP_REG_STRUCTURE_V2             0x2
#define AP_REG_STRUCTURE_V4             0x4
#define AP_REG_MAX_CPUS                 0x4

//
// Error codes from NT
//
#define E_BAD_DATA            HRESULT_FROM_NT(STATUS_BAD_DATA)
#define E_NOTFOUND            HRESULT_FROM_NT(STATUS_NOT_FOUND)
#define E_ALLOCATION_ERROR    HRESULT_FROM_NT(STATUS_MEMORY_NOT_ALLOCATED)

// cached UEFI varieables
#define DUMP_INSTANCE_CACHED L"DumpInstanceCached"
#define IN_MEM_DATA_INFO_CACHED L"InMemDataInfoCached"

#define Add2Ptr(_ptr,_inc) ((PVOID)((PUCHAR)(_ptr) + (_inc)))

// {0x66C9B323-F7FC-48B6-BF96-6F32E335A428}
static const GUID RAWDUMP_GUID =
{ 0x66C9B323, 0xF7FC, 0x48B6, { 0xBF, 0x96, 0x6F, 0x32, 0xE3, 0x35, 0xA4, 0x28 } };


//
// Dump registry key
//
#define DUMP_REGKEY L"System\\CurrentControlSet\\Control\\CrashControl"

#ifndef BOOLIFY
#define BOOLIFY(e)      (!!(e))
#endif

// nonstandard extension used : nameless struct/union
#pragma warning(disable: 4201) 
// nonstandard extension used : bit field types other than int
#pragma warning(disable: 4214) 
// This mask keeps track of progress in version determination
// to facilitate debugging.
//
typedef struct _VERSION_DETERMINATION_CHKLIST
{
    UINT32      IsConfigTablePresent:1;
    UINT32      IsConfigTableVersionValid : 1;
    UINT32      IsDumpCapabilitySet : 1;
    UINT32      IsLegacySvFlagPresent : 1;
    UINT32      IsMemoryCaptureModePresent : 1;
    UINT32      Reserved : 27;
}VERSION_DETERMINATION_CHKLIST, *PVERSION_DETERMINATION_CHKLIST;

//
// This mask keep track of progress and 
// facilitate debugging.
//
typedef struct _IS_DUMP_EXPECTED_CHKLIST
{
    UINT32      IsAbnormalResetNonZero : 1;
    UINT32      IsUseCapabilityPresent : 1;
    UINT32      IsUseCapabilitySet : 1;
    UINT32      Reserved : 29;
}IS_DUMP_EXPECTED_CHKLIST, *PIS_DUMP_EXPECTED_CHKLIST;

//
// This mask keeps track of DUMP_HEADER validation.
//
typedef struct _DUMP_HEADER_CHKLIST
{
    UINT32      IsSignatureValid : 1;
    UINT32      IsValidDumpGood : 1;
    UINT32      IsBugCheckCodeGood : 1;
    UINT32      IsDumpTypeGood : 1;
    UINT32      IsRequiredDumpSpaceGood : 1;
    UINT32      IsDumpInstanceMatch : 1;
    UINT32      Reserved : 26;
}DUMP_HEADER_CHKLIST, *PDUMP_HEADER_CHKLIST;

//
// Progress list for Version B dump.
//
typedef union _DUMP_PROGRESS
{
    struct
    {
        UINT32      IsDumpEnabled : 1;
        UINT32      IsDumpExpected : 1;
        UINT32      IsRawDumpLocated : 1;
        UINT32      IsRawDumpHeaderValid : 1;
        UINT32      IsRawDumpSectionTableValid : 1;
        UINT32      IsAPRegPresent : 1;
        UINT32      IsDumpHeaderPresent : 1;
        UINT32      IsInMemDataInfoFound : 1;
        UINT32      IsDiagBufferBuilt : 1;
        UINT32      IsPmicPonBinCopied : 1;
        UINT32      IsRstStatBinCopied : 1;
        UINT32      Reserved : 21;
    };

    UINT32 AsUINT32;
}DUMP_PROGRESS, *PDUMP_PROGRESS;

typedef struct _RAW_DUMP_HEADER_VERIFICATION_CHKLIST
{
    UINT32      IsSignatureValid : 1;
    UINT32      IsVersionValid : 1;
    UINT32      IsFlagsValid : 1;
    UINT32      IsDumpComplete : 1;
    UINT32      IsDumpSizeValid : 1;
    UINT32      IsSectionsCountValid : 1;
    UINT32      IsDumpHeaderInContext : 1;
    UINT32      Reserved : 25;
}RAW_DUMP_HEADER_VERIFICATION_CHKLIST, *PRAW_DUMP_HEADER_VERIFICATION_CHKLIST;


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
// Bitmask indicating which diagnostic information to dump when wpdmp exits.
//
typedef struct _DIAG_INFO_TYPE_MASK
{
    UINT32      DumpRawDumpTable : 1;
    UINT32      DumpDDRMemoryMap : 1;
    UINT32      DumpUEFI : 1;
    UINT32      Reserved : 29;
}DIAG_INFO_TYPE_MASK, *PDIAG_INFO_TYPE_MASK;


//defined as OFFLINE_CRASHDUMP_CONFIGURATION_TABLE in arc.h
typedef struct
{
    UINT32      Version;
    UINT32      AbnormalResetOccurred;
    UINT32      OfflineMemoryDumpCapable;
} OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE, *POFFLINE_CRASH_DUMP_CONFIGURATION_TABLE;



//
// Define SBL offline dump media.
//
typedef enum
{
    SBL_DUMP_LOCATION_INVALID = 0,
    SBL_DUMP_LOCATION_EMMC = 1,
    SBL_DUMP_LOCATION_SD = 2
}SBL_DUMP_LOCATION;

//
// Define SBL dump format.
//
typedef enum
{
    SBL_DUMP_FORMAT_INVALID = 0,
    SBL_DUMP_FORMAT_RAW = 1,
    SBL_DUMP_FORMAT_FILE = 2
}SBL_DUMP_FORMAT;


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
}DDR_MEMORY_MAP, *PDDR_MEMORY_MAP;

typedef struct _PAE_ADDRESS {
    union {
        struct {
            ULONG Offset : 12;                  // 0  .. 11
            ULONG Table : 9;                    // 12 .. 20
            ULONG Directory : 9;                // 21 .. 29
            ULONG DirectoryPointer : 2;         // 30 .. 31
        };
        struct {
            ULONG Offset : 21;
            ULONG Directory : 9;
            ULONG DirectoryPointer : 2;
        } LargeAddress;

        ULONG DwordPart;
    };
} PAE_ADDRESS, *PPAE_ADDRESS;


//
//  Raw dump definitions
//

//
// Global context struct. 
//
typedef struct _DMP_CONTEXT
{
    // 
    // Check lists.
    //
    VERSION_DETERMINATION_CHKLIST                       VersionChkList;
    IS_DUMP_EXPECTED_CHKLIST                            IsDumpExpectedChkList;
    DUMP_PROGRESS                                       SBLDumpProgress;
    RAW_DUMP_HEADER_VERIFICATION_CHKLIST                RawDumpHeaderChkList;
    DUMP_HEADER_CHKLIST                                 DumpHeaderChkList;

    //
    // General
    // 
    UINT32                                              OfflineDumpState;
    OFFLINE_DUMP_VERSION                                OfflineDumpVersionSupported;
    
    //
    // SBL dump section
    //
    PVOID                                               IoBuffer;
    
    //
    // SBL dump format and location
    //
    SBL_DUMP_LOCATION                                   SBLDumpLocation;
    SBL_DUMP_FORMAT                                     SBLDumpFormat;

    //
    // Raw dump partition device ID.
    //    
    DEVICE_IO                                           hDisk;
    DEVICE_IO                                           SDCardDeviceHandle;
    DEVICE_IO                                           SDCardRawDumpDotBinFileHandle;
    DEVICE_IO                                           LogFileHandle;
    LARGE_INTEGER                                       diskoffset;
    LARGE_INTEGER                                       RawDumpPartitionLength;
    PRAW_DUMP_HEADER                                    RawDumpHeader;
    UINT32                                              RawDumpTableSize;
    UINT32                                              OfflineMemoryDumpUseCapability;
    PRAW_DUMP_SECTION_HEADER                            DDRSections;
    UINT32                                              DDRMemoryMapCount;
    PDDR_MEMORY_MAP                                     DDRMemoryMap;
    UINT64                                              TotalDDRSizeInBytes;
    UINT64                                              TotalSVSpecificSizeInBytes;
    UINT64                                              TotalCpuContextSizeInBytes;
    LARGE_INTEGER                                       CPUContextAddress;
    UINT32                                              DDRSectionCount;
    UINT32                                              SVSectionCount;
    UINT32                                              CPUContextSectionCount;
    UINT32                                              MissingFlagsCount;
    UINT32                                              InvalidSectionCount;
    UINT32                                              InsufficientStorageSectionsCount;
    UINT32                                              InvalidVersionCount;
    UINT32                                              DDRSectionsOverlapCount;
    UINT32                                              DDRSectionFragmentationCount;
    UINT64                                              LargestSVSpecificSectionSize;
    bool                                                Is64Bit;
    bool                                                IsAPREG64Bit;

    //
    // Data from Windows
    //
    LARGE_INTEGER                                       DumpHeaderPA;
    LARGE_INTEGER                                       DumpHeaderAddress;
    PDUMP_HEADER32                                      DumpHeader;
    PDUMP_HEADER64                                      DumpHeader64;
    PPHYSICAL_MEMORY_DESCRIPTOR32                       MemoryDescriptors;
    UINT64                                              SizeAccordingToMemoryDescriptors;
    UINT32                                              SecondaryDataBlobCount;
    ULARGE_INTEGER                                      DumpInstance;


    //
    // File-based raw dump 
    //    
    LPWSTR                                              RawDumpPath;
    LPWSTR                                              RawDumpOnSDPath;
    LPWSTR                                              LogFilePath;
    LARGE_INTEGER                                       RawDumpDotBinFileId;
    
    //Array of file IDs. Index based on section count.
    UINT32                                              FirstDDRFileId;
    
    // 
    // output file paths
    // 
    LPWSTR                                              RawDumpInfoPath;
    DEVICE_IO                                           hInfoFileHandle;

    //
    // General SBL dump
    //
    OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE                ConfigTable;
}DMP_CONTEXT, *PDMP_CONTEXT;

//
//    The table lists name of various type of sections that the 
// Rawdump header section table can point to. 
//

static PWSTR RawDumpSectionTypeStringTable[] = { L"RESERVED",
   // 
   // DDR sections: containing contents of the RAM
   //
    L"DDR_RANGE",
    //
    // CPU context at the bugcheck time
    //
    L"CPU_CONTEXT",
    //
    // Sections that are outside of OS but belonging to SV.
    // These sections are needed for SVs to process bugs related to 
    // their code.
    //
    L"SV_SPECIFIC" 
};

typedef enum
{
    RAW_DUMP_SECTION_TYPE_Reserved = 0,
    RAW_DUMP_SECTION_TYPE_Maximum = 4
}RAW_DUMP_SECTION_TYPE;

// Flags for UEFI settting functions
typedef enum {
    DISABLE_UEFI = 0,
    ENABLE_UEFI = 1,
} UEFI_SETTING;

//
// The size of the buffer thats appended at the end of the rawdump.
//
#define DEVICE_SPECIFIC_INFO_BUFFER_LENGTH 1024


// This default value is the data pattern filled in by IoFillDumpHeader.
// So in the case the ntkrnel doesn't have the change, we'll
// see the default pattern. 
//
#define DEFAULT_DUMP_INSTANCE_ID 0x4547415045474150

//
// Path to the Log file.
//
#define LOG_FILE_PATH L"offlineCrash.log"

#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)

namespace Raw2Dump
{
    DLLEXPORT NTSTATUS CheckAndSubmitOfflineCrash(VOID);
}


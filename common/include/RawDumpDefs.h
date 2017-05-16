/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   RawDumpDefs.h

Environment:
   User Mode

Author:
   Jose Pagan (jopagan)
--*/

#pragma once

#include <windows.h>

#define ONE_KILOBYTE                        (1024)
#define ONE_MEGABYTE                        (1024 * ONE_KILOBYTE)
#define ONE_GIGABYTE                        (1024 * ONE_MEGABYTE)

// Raw Dump file - Header definitions & types
typedef UINT64                                  RAW_DUMP_HEADER_SIGNATURE_TYPE;
#define RAW_DUMP_HEADER_SIGNATURE               (RAW_DUMP_HEADER_SIGNATURE_TYPE)(0x21706D445F776152)
#define RAW_DUMP_HEADER_SIGNATURE_LENGTH        sizeof RAW_DUMP_HEADER_SIGNATURE_TYPE
#define RAW_DUMP_HEADER_VERSION                 0x00001000
#define RAW_DUMP_HEADER_EXPECTED_BITS           (RAW_DUMP_HEADER_FLAGS_VALID | RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE)

typedef enum _RAW_DUMP_HEADER_FLAGS
{
    RAW_DUMP_HEADER_FLAGS_UNKNOWN               = 0x0,
    RAW_DUMP_HEADER_FLAGS_VALID                 = 0x1,
    RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE  = 0x2
} RAW_DUMP_HEADER_FLAGS;


// Raw Dump file - Sections definitions & types
#define RAW_DUMP_SECTION_HEADER_NAME_LENGTH     0x14
#define RAW_DUMP_SECTION_HEADER_SIZE            sizeof RAW_DUMP_SECTION_HEADER
#define RAW_DUMP_SECTION_HEADER_VERSION         0x00001000
#define RawDumpTableSize(nSections)             (nSections * RAW_DUMP_SECTION_HEADER_SIZE)

typedef enum _RAW_DUMP_SECTION_TYPE
{
    RAW_DUMP_SECTION_TYPE_RESERVED    = 0x0,
    RAW_DUMP_SECTION_TYPE_DDR_RANGE   = 0x1,
    RAW_DUMP_SECTION_TYPE_CPU_CONTEXT = 0x2,
    RAW_DUMP_SECTION_TYPE_SV_SPECIFIC = 0x3,
    RAW_DUMP_SECTION_TYPE_MAX
} RAW_DUMP_SECTION_TYPE;


#pragma pack(1)
/******************************************************************************
** RawDump file - Header information
*******************************************************************************/
typedef struct
{
    UINT64  Signature;
    UINT32  Version;
    UINT32  Flags;              // RAW_DUMP_HEADER_FLAGS
    UINT64  OsData;
    UINT64  CpuContext;
    UINT32  ResetTrigger;
    UINT64  DumpSize;
    UINT64  TotalDumpSizeRequired;
    UINT32  SectionsCount;
} RAW_DUMP_HEADER, *PRAW_DUMP_HEADER;


/******************************************************************************
** RawDump file - Sections specific types: DDR, SV, CPU_Context
*******************************************************************************/
//  DDR Specific section
typedef struct
{
    UINT64                 Base;
} DDR_INFORMATION, *PDDR_INFORMATION;

//  CPU Context Specific section
typedef struct
{
    UINT16 Architecture; 
    UINT32 CoreCount;
} CPU_CONTEXT_INFORMATION, *PCPU_CONTEXT_INFORMATION;

//  SV Specific section
#define RAW_DUMP_SV_SPECIFIC_DATA_LENGTH    0x10

typedef struct
{ // SV specific
    UCHAR   SVSpecificData[RAW_DUMP_SV_SPECIFIC_DATA_LENGTH];
} SV_SPECIFIC_INFORMATION, *PSV_SPECIFIC_INFORMATION;

// Raw Dump file - Section entry
typedef struct
{
    UINT32  Flags;
    UINT32  Version;
    UINT32  Type;   // RAW_DUMP_SECTION_TYPE
    UINT64  Offset;
    UINT64  Size;

    union
    {
        DDR_INFORMATION             DDRInformation;
        CPU_CONTEXT_INFORMATION     CPUContextInformation;
        SV_SPECIFIC_INFORMATION     SVSpecificInformation;
    } u;

    UCHAR                           Name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
} RAW_DUMP_SECTION_HEADER, *PRAW_DUMP_SECTION_HEADER;
#pragma pack()
// // //

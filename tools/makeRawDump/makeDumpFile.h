/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    makeDumpFile.h

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/
#pragma once

#include <windows.h>
#include <vector>
#include <string>

#include "DEVICE_IO.h"
#include "SV_Specific.h"
#pragma pack(1)
#include "Dump_Header.h"
#include "RawDumpDefs.h"
#pragma pack()

#define INVALID_UINT32                      MAX_VALUE(UINT32)
#define INVALID_ULARGE_INTEGER              MAX_VALUE(ULONGLONG)
#define MAX_UINT32                          MAX_VALUE(UINT32)
#define MAX_ULONGLONG                       MAX_VALUE(ULONGLONG)
#define MAX_DDR_SECTIONS                    250

// DDR defaults
#define DEFAULT_DDR_SECTIONS_COUNT          2
#define DEFAULT_DDR_SECTIONS_SIZE           0x80000000
#define DEFAULT_DDR_SECTIONS_BASE           0x20000000

#define DEFAULT_CORE_COUNT                  6

#define DEFAULT_BLOCK_SIZE                  0x1000

#define MAX_DISK_SLOTS                      100     // High bound for dist attached to this device.
#define TEST_PATTERN_BEGIN                  32        // <space>
#define TEST_PATTERN_END                    126     // Last Ascii Char
#define TEST_PATTERN_SIZE                   (TEST_PATTERN_END - TEST_PATTERN_BEGIN + 1)
#define OFFSET2VALUE(offset)                (((offset) % TEST_PATTERN_SIZE) + TEST_PATTERN_BEGIN )
#define PADDING_BUFFER_SIZE                 (ONE_MEGABYTE)

#define MIN_TEST_PATTERN_SIZE               (TEST_PATTERN_SIZE * DEFAULT_BLOCK_SIZE)
#define DEVICE_SPECIFIC_INFO_BUFFER_LENGTH  1024

#define DEFAULT_DUMP_FILE_NAME              "RawDump.bin"
#define DEFAULT_DUMP_SIZE                   DEFAULT_SV_SECTION_RAWDUMP_SIZE         //  (512 * ONE_MEGABYTE)

#define MAX_VALUE(type)                     ((type)(-1))

#define SECTION_TABLE_SIZE(count)           (sizeof RAW_DUMP_SECTION_HEADER * count)
#define ArrayCount(array)                   (sizeof array/sizeof array[0])
#define CHAR_OUT_OF_STRING(str, idx)        (((PCHAR)(str))[idx])
#define INVALIDATE_VALUE(val)               (~val)

extern std::string  outFileName;

typedef enum
{ // Describes DDR section proximity, in section table
    DDR_PROXIMITY_UNSET = 0,
    DDR_PROXIMITY_ADJACENT,
    DDR_PROXIMITY_SCATTER
} DDR_PROXIMITY;


typedef enum
{ // Describes DDR ordsering rules, in section table
    DDR_ORDER_UNSET = 0,
    DDR_ORDER_ASLISTED,
    DDR_ORDER_ASCENDING,
    DDR_ORDER_DESCENDING,
    DDR_ORDER_RANDOM
} DDR_ORDER;


typedef enum
{ // Determines if output goes to file or partition
    OUTPUT_FILE = 0,
    OUTPUT_PARTITION
} OUTPUT_CONTROL_FLAG;


typedef struct _DDR_SECTION_DATA
{ // struct to describe a DDR entry
    UINT32            sectionID;
    ULARGE_INTEGER    base;
    ULARGE_INTEGER    size;
    ULARGE_INTEGER    offset;

}DDR_SECTION_DATA, *PDDR_SECTION_DATA;


// configuration record to direct layout of output file
typedef struct _DUMP_CONFIG
{
    // Test Possibilities - flags and other failures
    BOOL                                    badHeaderSignature;         // Signature incorrect
    BOOL                                    badHeaderVersion;           // version incorrect
    BOOL                                    badHeaderFlags;             // header flags incorrect
    BOOL                                    badHeaderNumberOfSections;  // Missing sections
    BOOL                                    DDROverlap;                 // overlapping DDR sections
    BOOL                                    excludeTZ;
    BOOL                                    excludeApReg;

    // Raw dump file header
    RAW_DUMP_HEADER                         dumpFileHeader;
    std::vector<PRAW_DUMP_SECTION_HEADER>   dumpFileSections;

    // Output control values and flags
    BOOL                                    writePayload;               // flag - when false, no payload and padding data, headers only
    BOOL                                    outputToPartition;
    ULARGE_INTEGER                          requestedRawDumpFileSize;   // Requested size of the output file, from /FileSzie argument
    ULARGE_INTEGER                          actualRawDumpFileSize;      // Computed size of the output file, determined from table data
    ULARGE_INTEGER                          sectionTableOffset;
    ULARGE_INTEGER                          payloadOffset;

    // DDR Section parameters
    UINT32                                  DDR_SectionCount;           // Number of sections, can be set by /DDRCount
    DDR_PROXIMITY                           DDR_Proximity;              // Proximity of DDR sections, can be set by /DDRProximity
    DDR_ORDER                               DDR_Order;                  // Ordering of DDR sections, can be set by /DDROrder
    ULARGE_INTEGER                          DDR_Size;                   // Default DDR section size, can be set by /DDRSize
    ULARGE_INTEGER                          DDR_PayloadSize;            // Total Size of the data for all segments referred to here
    std::vector<PRAW_DUMP_SECTION_HEADER>   sectionDDR;                 // list of DDR sections

    // ApReg Section parameters
    UINT32                                  CoreCount;
    ULARGE_INTEGER                          CPU_PayloadSize;            // Total Size of the data for all segments referred to here
    std::vector<PRAW_DUMP_SECTION_HEADER>   sectionCPU;                 // lsit of CPU/APReg sections

    // SV Section parameters
    ULARGE_INTEGER                          SV_PayloadSize;             // Total Size of the data for all segments referred to here
    std::vector<PRAW_DUMP_SECTION_HEADER>   sectionSV;                  // list of SV specific sections

} DUMP_CONFIG, *PDUMP_CONFIG;

HRESULT OpenOutput(_In_ PDUMP_CONFIG cfg, _In_ std::string *fName, _Out_ DEVICE_IO *oHandle);
HRESULT WriteSections(_Inout_ DEVICE_IO *oFile, _In_ PDUMP_CONFIG cfg);
HRESULT WritePayload(_Inout_ DEVICE_IO *oFile, _In_ PDUMP_CONFIG cfg);
HRESULT WritePattern(_Inout_ DEVICE_IO *oFile, _In_ PCHAR pattern, _In_ size_t patternSize, _In_ ULARGE_INTEGER writeSize, _Out_ ULARGE_INTEGER *bytesWritten);
HRESULT CreateFullSectionsTable(_Inout_ PDUMP_CONFIG cfg);
HRESULT setTableOffsets(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vDst);
HRESULT copySectionTable(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vDst, _In_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc);
HRESULT sortDDRSectionTable(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc, _In_ DDR_ORDER order);
HRESULT sortVector(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *vSrc, _In_ DDR_ORDER order);
HRESULT randomizeVector(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> *vSrc);
void    DumpVectorTable(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc);
HRESULT DumpConfigInfo(_Inout_ PDUMP_CONFIG cfg, _In_ std::wstring fName);
HRESULT DumpParserWarnings(_In_ std::string *str);
/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   Dump_Header.h

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/
#pragma once

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#include "RawDumpDefs.h"

/**************************************************************************************************
** macros, constants and enums
**************************************************************************************************/
#define getHeaderErrorWString(err)  &(HeaderErrorWString[err].description)
#define HEADER_ERROR                lastError
#define HEADER_ERROR_WSTRING        getHeaderErrorWString(HEADER_ERROR)

#define ERROR_STRING_MAX_LENGTH         255

typedef enum
{ // Error string indexes
    RAW_DUMP_HEADER_OK = 0,
    RAW_DUMP_SECTIONS_OK = RAW_DUMP_HEADER_OK,
    RAW_DUMP_ARGS,
    RAW_DUMP_HEADER_SIGNATURE_INVALID,
    RAW_DUMP_HEADER_VERSION_INVALID,
    RAW_DUMP_HEADER_EXPECTED_BITS_INVALID,
    RAW_DUMP_HEADER_SIZE_INVALID,
    RAW_DUMP_HEADER_SECTION_COUNT_INVALID,
    RAW_DUMP_SECTION_VERSION_INVALID,
    RAW_DUMP_SECTION_FLAGS_INVALID,
    RAW_DUMP_SECTION_TYPE_INVALID,
    RAW_DUMP_SECTION_INSUFFICENT_STORAGE_INVALID,
    MAX_RAW_DUMP_ERROR
} RAW_DUMP_ERROR;

typedef struct _SECTION_TABLE_STATS
{
    // DDR section(s) data
    PRAW_DUMP_SECTION_HEADER    FirstDDRSection;
    UINT32                      DDRSectionCount;
    UINT64                      TotalDDRSizeInBytes;

    // CPU Context section(s) data
    UINT32                      CPUContextSectionCount;
    UINT64                      TotalCpuContextSizeInBytes;
    UINT32                      CpuArchitecture;

    // SV section(s) data
    UINT32                      SVSectionCount;
    UINT64                      TotalSVSpecificSizeInBytes;
    UINT64                      LargestSVSpecificSectionSize;

    // Count of invalids
    UINT32                      InvalidVersionCount;
    UINT32                      InvalidFlagsCount;
    UINT32                      InvalidSectionCount;

    // Data from DDR map creation
    UINT32                      DDRSectionFragmentationCount;
    UINT32                      DDRSectionsOverlapCount;
    UINT32                      InsufficientStorageSectionsCount;
    
} SECTION_TABLE_STATS, *PSECTION_TABLE_STATS;


typedef struct _HDR_ERR_TABLE_ENTRY
{
    WCHAR    description[ERROR_STRING_MAX_LENGTH];
} HDR_ERR_TABLE_ENTRY;


/**************************************************************************************************
** Globals
**************************************************************************************************/
extern HDR_ERR_TABLE_ENTRY HeaderErrorWString[];
extern RAW_DUMP_ERROR lastError;


/**************************************************************************************************
** Function Prototypes
**************************************************************************************************/
BOOL        IsValidRawDumpHeader(PRAW_DUMP_HEADER pRawDumpHeader);
HRESULT     ValidateRawDumpSectionTable(PRAW_DUMP_SECTION_HEADER  SectionTable, UINT32 TableSize, PSECTION_TABLE_STATS SectionStats);

/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    processArgs.h

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/

#pragma once

#include "MakeDumpFile.h"

#define MAX_PARAM_STRING_SIZE               32
#define DEFAULT_PARTITION_SIZE              (ONE_GIGABYTE)
#define MAX_PARTITION_SIZE                  ((ULARGE_INTEGER)(4 * ONE_GIGABYTE))

// Define the APREG section
#define DEFAULT_APREG_CORE_COUNT            0

// Define the DDR sections
#define DEFAULT_DDR_SECTION_COUNT           2
#define DEFAULT_DDR_SECTION_LENGTH          0x80000000

typedef struct _PARAM_ROW
{
    CHAR    paramStr[MAX_PARAM_STRING_SIZE];
    UINT32  numArgs;
    HRESULT (*proccessArgsFunc)(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
} PARAM_ROW, *PPARAM_ROW;


/****************************************************************************************************
** Functions exposed extrnal to this object file
*****************************************************************************************************/
HRESULT ProcessCommandArguments(_In_ int argc, _In_ char** argv, _Inout_ PDUMP_CONFIG config, _Out_ std::string *failString);


/****************************************************************************************************
** Functions intrnal to this object file
*****************************************************************************************************/
UINT32  FindArgumentInParameterArray(_In_ PCHAR arg);
HRESULT ResizeSectionsList(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, _In_ UINT32 newSize);


/****************************************************************************************************
** Prototypes for the extraction of parameters
*****************************************************************************************************/
HRESULT ProcessFileSize(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessFileName(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessPart(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessDDRParameters(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessDDRSize(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessDDRCount(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessDDRProximity(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessDDROrder(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessNumCores(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessNoPayload(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
HRESULT ProcessNoAPReg(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList);
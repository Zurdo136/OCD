/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    processSV.h

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/

#pragma once

#include "makeDumpFile.h"

// The table incldues all SV sections plus to additional entries.
// The Last two rows include RawDump.bin and a null entry, which 
// is indicated as a needed row. Both can be ignored in this
// application. The tird exclusiong is the APReg Section which is
// included by default, but can be supressed with the /NpApReg 
// argument, on the command line
#define SV_ROWS_TO_USE          (SV_SPECIFIC_TABLE_SIZE - 3)

/****************************************************************************************************
** Functions intrnal to this object file
*****************************************************************************************************/
HRESULT MakeSVNameString(_Out_ PCHAR name, _In_ size_t nameLen, _In_ PCHAR sectionName);
HRESULT UpdateSVWithDefault(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, BOOL excludeApReg);

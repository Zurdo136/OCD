/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    processDDR.h

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/

#pragma once

#include "makeDumpFile.h"

#define DDR_NAME_FORMAT_STRING          "%%s%%0%dd.%%s"
#define DEFAULT_DDR_NAME_STRING         "DDRCS"
#define DEFAULT_DDR_EXT_STRING          "BIN"
#define DDR_NAME_STRING_ID_LEN          3

/****************************************************************************************************
** Functions intrnal to this object file
*****************************************************************************************************/
HRESULT MakeDDRNameString(_Out_ PCHAR ddrName, _In_ size_t ddrNameLen, _In_ UINT32 id);
HRESULT UpdateDDRWithDefault(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, _In_ ULARGE_INTEGER defaultSize);


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

#include "MakeDumpFile.h"

HRESULT GetSectionSize(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, _Out_ PULARGE_INTEGER sizeTotal);
HRESULT PopulateRawDumpFileHeader(_Inout_ PDUMP_CONFIG config);

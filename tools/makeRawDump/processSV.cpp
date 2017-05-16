/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    processSV.cpp

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/

#pragma once

#include "SV_Specific.h"
#include "processArgs.h"
#include "processSV.h"

extern SV_SPECIFIC_GUID_TO_NAME SvSectionTable[];

//Expand the sectionsDDR verctor 
// Sort the vector by base
// check for overlap ?

#define SV_FILE_PREFIX "SV_"

HRESULT MakeSVNameString(_Out_ PCHAR name, _In_ size_t nameLen, _In_ UINT32 idx)
{
    HRESULT ret = S_OK;

    if ((nullptr == name) ||
        (RAW_DUMP_SECTION_HEADER_NAME_LENGTH < nameLen) ||
        ((strlen((PCHAR)&SvSectionTable[idx].Name) + sizeof SV_FILE_PREFIX) > RAW_DUMP_SECTION_HEADER_NAME_LENGTH)
       )
    {
        ret = E_INVALIDARG;
    }
    else
    {
        sprintf(name, "%s%s", SV_FILE_PREFIX, SvSectionTable[idx].Name);
    }

    return ret;
}



/****************************************************************************************************
** HRESULT   FillEmptyDDRWithDefault(
**              _In_out_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList,
**              _In_ ULARGE_INTEGER defaultSize)
**
** Description:
**  Fill in the DDR (vector) table with default values.  The DDR table contains a default number of 
**  entries which can be modified by the /DDRCount command line argument.  The DDR table can also 
**  have specific entries set to non-default values ising the /DDR command line argument.  Users can
**  When create a table with more entries than are defined (by /DDR).  These unspecified entries
**  will be updated by this function.
**
**  Unspecified entries are defined to continue the DDR table.  Thus any DDR entries added here 
**  will appended DDR segments.
**
** Arguments :
**  sectionList - pointer to the DDR sections vector
**  defaultSize - the size of all default sections
**
** Return :
**  S_OK
**  E_OUTOFMEMRY - failure to allocate a DDR entry
**
*****************************************************************************************************/
HRESULT UpdateSVWithDefault(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, BOOL excludeApReg)
{
    HRESULT ret = S_OK;

    if (nullptr == sectionList)
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else
    {
        UINT32 nEntries = SV_ROWS_TO_USE + (excludeApReg ? 0 : 1);
        UINT32 tableRow = 0;

        ret = ResizeSectionsList(sectionList, nEntries);

        for (UINT32 i = 0; i < nEntries; i++, tableRow++)
        { // Fill in the empty table entried starting from endDDR
            BOOL isAPRegSection = (SV_SECTION_AP_REG_GUID == SvSectionTable[tableRow].Guid) ? TRUE : FALSE;
            
            if (isAPRegSection && excludeApReg)
            {
                tableRow++;
            }

            PRAW_DUMP_SECTION_HEADER tmpRow = new RAW_DUMP_SECTION_HEADER;
            if (nullptr == tmpRow)
            {
                ret = E_OUTOFMEMORY;
                break;
            }

            ZeroMemory(tmpRow, sizeof RAW_DUMP_SECTION_HEADER);
            if (SUCCEEDED(MakeSVNameString((PCHAR)(tmpRow->Name), sizeof tmpRow->Name, tableRow)))
            {
                tmpRow->Flags = RAW_DUMP_HEADER_FLAGS_VALID;
                tmpRow->Version = RAW_DUMP_SECTION_HEADER_VERSION;
                tmpRow->Type = (isAPRegSection) ? RAW_DUMP_SECTION_TYPE_CPU_CONTEXT : RAW_DUMP_SECTION_TYPE_SV_SPECIFIC;
                tmpRow->Offset = INVALID_ULARGE_INTEGER;
                tmpRow->Size = SvSectionTable[tableRow].Size.QuadPart;

                sectionList->at(i) = tmpRow;
            }

        }

    }

    return ret;
}


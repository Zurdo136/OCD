/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    processDDR.cpp

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/

#pragma once

#include "processDDR.h"

/****************************************************************************************************
** HRESULT  MakeDDRNameString(
**              _Out_ PCHAR name, _In_ size_t nameLen,
**               _In_ UINT32 id)
**
** Description:
**  Fill in the DDR file name with the appropriate format.  Name should be DDRCSXXX.BIN where 
**  the value of XXX is the DDR section index (by convention).
**
** Arguments :
**  name - char array for the constructed name
**  id - the ddr index to be used in the name
**
** Return :
**  S_OK
**  E_INVALIDARG - one of thei nput parameters i incorrect
**
*****************************************************************************************************/
HRESULT MakeDDRNameString(_Out_ PCHAR name, _In_ size_t nameLen, _In_ UINT32 id)
{
    HRESULT ret = S_OK;

    if ((nullptr == name) || (RAW_DUMP_SECTION_HEADER_NAME_LENGTH < nameLen))
    {
        ret = E_INVALIDARG;
    }
    else
    {
        CHAR ddrFormat[sizeof DDR_NAME_FORMAT_STRING] = { 0 };
        
        sprintf(ddrFormat, DDR_NAME_FORMAT_STRING, DDR_NAME_STRING_ID_LEN);
        sprintf(name, ddrFormat, DEFAULT_DDR_NAME_STRING, id, DEFAULT_DDR_EXT_STRING);
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
**  have specific entries set to non-default values using the /DDR command line argument.  Users can
**  create a table with more entries than are defined (or defaulted) using the /DDR command line 
**  argument. Any undefined entries will be updated with default values, by this function.
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
HRESULT UpdateDDRWithDefault(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, _In_ ULARGE_INTEGER defaultSize)
{
    HRESULT ret = S_OK;

    if (nullptr == sectionList)
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else
    {
        ULARGE_INTEGER              maxBase = { 0 };
        ULARGE_INTEGER              endDDR = { 0 };
        size_t                      nEntries = sectionList->size();
        PRAW_DUMP_SECTION_HEADER    tmpRow = nullptr;

        // Find max base and compute DDR end.
        maxBase.QuadPart = DEFAULT_DDR_SECTIONS_BASE;
        for (UINT32 i = 0; i < nEntries; i++)
        {

            // consider only populated items in the vector
            if ( (nullptr != sectionList->at(i)) &&
                 ((sectionList->at(i))->u.DDRInformation.Base > maxBase.QuadPart)
               )
            { // update the base, from the populated item, if appropriate
                maxBase.QuadPart = (sectionList->at(i))->u.DDRInformation.Base;
                endDDR.QuadPart = maxBase.QuadPart + (sectionList->at(i))->Size;
            }

        }

        // Fill in empty table entries beginning with endDDR address
        for (UINT32 i = 0; i < nEntries; i++)
        {
            if (nullptr == sectionList->at(i))
            { // Fill in un-populated vector entry
                tmpRow = new RAW_DUMP_SECTION_HEADER;
                if (nullptr == tmpRow)
                { // Fail if we run out of memory
                    ret = E_OUTOFMEMORY;
                    break;
                }

                ZeroMemory(tmpRow, sizeof RAW_DUMP_SECTION_HEADER);
                if (SUCCEEDED(MakeDDRNameString((PCHAR)(tmpRow->Name), sizeof tmpRow->Name, i)))
                { // Create and populate the new vector item
                    tmpRow->Flags = RAW_DUMP_HEADER_FLAGS_VALID;
                    tmpRow->Version = RAW_DUMP_SECTION_HEADER_VERSION;
                    tmpRow->Type = RAW_DUMP_SECTION_TYPE_DDR_RANGE;
                    tmpRow->Size = defaultSize.QuadPart;
                    tmpRow->u.DDRInformation.Base = endDDR.QuadPart;
                    tmpRow->Offset = INVALID_ULARGE_INTEGER;

                    sectionList->at(i) = tmpRow;
                    endDDR.QuadPart += tmpRow->Size;
                }

            }

        }

    }

    return ret;
}

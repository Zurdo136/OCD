/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   Dump_Header.cpp

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/

#include "Dump_Header.h"

RAW_DUMP_ERROR  lastError = RAW_DUMP_HEADER_OK;

HDR_ERR_TABLE_ENTRY HeaderErrorWString[] =
{
    { L"valid"},                                                    // RAW_DUMP_HEADER_OK = RAW_DUMP_SECTIONS_OK = RAW_DUMP_HEADER_OK = 0
    { L"Unable to process data" },                                  // RAW_DUMP_ARGS
    { L"Invalid Signature"},                                        // RAW_DUMP_HEADER_SIGNATURE_INVALID
    { L"Invalid Version" },                                         // RAW_DUMP_HEADER_VERSION_INVALID
    { L"Invalid Flags" },                                           // RAW_DUMP_HEADER_EXPECTED_BITS_INVALID
    { L"Invalid Size, should not be zero" },                        // RAW_DUMP_HEADER_SIZE_INVALID
    { L"Invalid number of Sections, should not be zero" },          // RAW_DUMP_HEADER_SECTION_COUNT_INVALID
    { L"Invlaid version in section table"},                         // RAW_DUMP_SECTION_VERSION_INVALID
    { L"Invlaid flags in section table" },                          // RAW_DUMP_SECTION_FLAGS_INVALID
    { L"Invlaid type in section table" },                           // RAW_DUMP_SECTION_TYPE_INVALID
    { L"Insufficient storage flag set before the last section" }    // RAW_DUMP_SECTION_INSUFFICENT_STORAGE_INVALID
};


/**************************************************************************************************
** BOOL IsValidRawDumpHeader(PRAW_DUMP_HEADER pRawDumpHeader)
**
** Description:
**   This function validates the RAW_DUMP_HEADER, provided as parameter.
**   The following are checked:
**     1. RAW_DUMP_HEADER.Signature
**     2. RAW_DUMP_HEADER.Version
**     3. RAW_DUMP_HEADER.Flags
**     4. RAW_DUMP_HEADER.DumpSize
**     5. RAW_DUMP_HEADER.SectionCount
**
**   If things check out, it will allocate and read the complete dump table
**   to memory.
**
** Arguments:
**   PRAW_DUMP_HEADER
**
** Return Value:
**   FALSE - header is invalid
**   TRUE - header is valid
**************************************************************************************************/
BOOL
IsValidRawDumpHeader(PRAW_DUMP_HEADER pRawDumpHeader)
{
    BOOL ret = FALSE;

    if (nullptr == pRawDumpHeader)
    {
        lastError = RAW_DUMP_ARGS;
    }
    else
    {
        if (RAW_DUMP_HEADER_SIGNATURE != pRawDumpHeader->Signature)
        { // Check the signature.
            lastError = RAW_DUMP_HEADER_SIGNATURE_INVALID;
        }
        else if (RAW_DUMP_HEADER_VERSION != pRawDumpHeader->Version)
        { // Check the version.
            lastError = RAW_DUMP_HEADER_VERSION_INVALID;
        }
        else if (0 == (pRawDumpHeader->Flags & RAW_DUMP_HEADER_EXPECTED_BITS))
        { // Check for expected flags.
            lastError = RAW_DUMP_HEADER_EXPECTED_BITS_INVALID;
        }
        else if (0 == pRawDumpHeader->DumpSize)
        { // Check dump size, must be a non-zero value.
            lastError = RAW_DUMP_HEADER_SIZE_INVALID;
        }
        else if (0 == pRawDumpHeader->SectionsCount)
        { // There must be at least one section.
            lastError = RAW_DUMP_HEADER_SECTION_COUNT_INVALID;
        }
        else
        {
            lastError = RAW_DUMP_HEADER_OK;
            ret = TRUE;
        }

    }

    return ret;
}


/**************************************************************************************************
** HRESULT  ValidateRawDumpSectionTable(
**                PRAW_DUMP_SECTION_HEADER  SectionTable, 
**                UINT32                    TableEntryCount,
**                PSECTION_TABLE_STATS      SectionStats
**          )
**
** Description:
**   This function validates the section table, provided as parameter.
**   The following are checked:
**       1. At least one DDR section.
**       2. No sections with unrecognized section type.
**       3. SectionsCount matches number of valid sections.
**       4. Only the last section should have the RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE flag.
**
** Arguments:
**     PRAW_DUMP_SECTION_HEADER  SectionTable - ptr to section table, of RAW_DUMP_SECTION_HEADER elemnts
**     UINT32                    TableEntryCount - number of entries in the table
**     PSECTION_TABLE_STATS      SectionStats - ptr to statistical data table, collected here.
**       NOTE: this table is populated and returned (argument 3)
**
** Return Values:
**   S_OK - table is valid, pSectionStats table is populated and returned.
**   E_FAIL - table is invalid, error code can be used for fatal error encountered
**   E_INVALIDARG - null pointer(s) passed as arguments
**************************************************************************************************/
HRESULT
ValidateRawDumpSectionTable( PRAW_DUMP_SECTION_HEADER  SectionTable, UINT32 TableEntryCount, PSECTION_TABLE_STATS SectionStats )
{
    HRESULT hr = S_OK;

    if ((nullptr == SectionTable) ||
        (nullptr == SectionStats) ||
        (0 == TableEntryCount)
        )
    {
        hr = E_INVALIDARG;
        lastError = RAW_DUMP_ARGS;
    }
    else
    {
        lastError = RAW_DUMP_SECTIONS_OK;
        ZeroMemory(SectionStats, sizeof SECTION_TABLE_STATS);       // Clear stats table
        SectionStats->CpuArchitecture = PROCESSOR_ARCHITECTURE_UNKNOWN;

        for (UINT32 index = 0; index < TableEntryCount; index++)
        { // Check for valid values in each table entry

            if (RAW_DUMP_SECTION_HEADER_VERSION != SectionTable[index].Version)
            {
                SectionStats->InvalidVersionCount++;
                lastError = RAW_DUMP_SECTION_VERSION_INVALID;
            }

            if (0 == (SectionTable[index].Flags & RAW_DUMP_HEADER_EXPECTED_BITS))
            {
                SectionStats->InvalidFlagsCount++;
                lastError = RAW_DUMP_SECTION_FLAGS_INVALID;
            }

            // Check if RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE flag is  set. If so, it should only be set for the last section.
            if (((SectionTable[index].Flags & RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE) == RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE) &&
                (index != (TableEntryCount - 1))
                )
            {
                SectionStats->InsufficientStorageSectionsCount++;
                lastError = RAW_DUMP_SECTION_INSUFFICENT_STORAGE_INVALID;
            }

            switch (SectionTable[index].Type)
            { // Section type-specific checks.
                case RAW_DUMP_SECTION_TYPE_DDR_RANGE:
                    if (SectionStats->DDRSectionCount == 0)
                    { // Save the pointer to the first DDR section
                        SectionStats->FirstDDRSection = &SectionTable[index];
                    }

                    SectionStats->DDRSectionCount++;
                    SectionStats->TotalDDRSizeInBytes += SectionTable[index].Size;
                    break;

                case RAW_DUMP_SECTION_TYPE_CPU_CONTEXT:
                    SectionStats->CPUContextSectionCount++;
                    SectionStats->TotalCpuContextSizeInBytes += SectionTable[index].Size;
                    SectionStats->CpuArchitecture = SectionTable[index].u.CPUContextInformation.Architecture;
                    break;

                case RAW_DUMP_SECTION_TYPE_SV_SPECIFIC:
                    if (SectionStats->LargestSVSpecificSectionSize < SectionTable[index].Size)
                    {
                        SectionStats->LargestSVSpecificSectionSize = SectionTable[index].Size;
                    }

                    SectionStats->SVSectionCount++;
                    SectionStats->TotalSVSpecificSizeInBytes += SectionTable[index].Size;
                    break;

                default:
                    SectionStats->InvalidSectionCount++;
                    lastError = RAW_DUMP_SECTION_TYPE_INVALID;
                    break;
            } //switch (SectionTable[index].Type)

        } // for (UINT32 index = 0; index < dumpHeader->SectionsCount; index++)

        if (RAW_DUMP_SECTIONS_OK != lastError)
        {
            hr = E_FAIL;
        }

    }

    return hr;
}

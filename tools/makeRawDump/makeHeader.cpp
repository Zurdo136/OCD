/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    makeHeader.cpp

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/

#pragma once

#include "makeHeader.h"

/****************************************************************************************************
** HRESULT  PopulateRawDumpFileHeader(
**              _Inout_ PDUMP_CONFIG config)
**
** Description:
**  This function creates the header portion of the rawdump.bin file. Flags in the config affect
**  the data wirtten, signature, version, flags.
**
** Arguments:
**  config - the configuration to be used for file creation
**
** Return:
**  S_OK
**  E_INVALIDARG - bad pointer to congif table
**  E_FAIL - failed to get an appropriate size for any of the DDR sections
**
*****************************************************************************************************/
HRESULT PopulateRawDumpFileHeader(_Inout_ PDUMP_CONFIG config)
{
    HRESULT ret = S_OK;

    if (nullptr == config)
    {
        ret = E_INVALIDARG;
    }
    else if (FAILED(ret = GetSectionSize(&(config->sectionDDR), &config->DDR_PayloadSize)) ||
             FAILED(ret = GetSectionSize(&(config->sectionCPU), &config->CPU_PayloadSize)) ||
             FAILED(ret = GetSectionSize(&(config->sectionSV),  &config->SV_PayloadSize))
            )
    {
        ret = E_FAIL;
    }
    else
    { // Populate the File header section
        config->dumpFileHeader.Signature    = (config->badHeaderSignature ? INVALIDATE_VALUE(RAW_DUMP_HEADER_SIGNATURE) : RAW_DUMP_HEADER_SIGNATURE);
        config->dumpFileHeader.Version      = (config->badHeaderVersion ? INVALIDATE_VALUE(RAW_DUMP_HEADER_VERSION) : RAW_DUMP_HEADER_VERSION);
        config->dumpFileHeader.Flags        = (config->badHeaderFlags ? INVALIDATE_VALUE(RAW_DUMP_HEADER_FLAGS_VALID) : RAW_DUMP_HEADER_FLAGS_VALID);
        config->dumpFileHeader.OsData       = 0;
        config->dumpFileHeader.CpuContext   = 0;
        config->dumpFileHeader.ResetTrigger = 0;
        
        // Compute the size of the raw dump by totaling all the sections and payloads they require
        config->dumpFileHeader.DumpSize = sizeof(config->dumpFileHeader) +                                              /* Header size component*/
            SECTION_TABLE_SIZE(config->dumpFileHeader.SectionsCount) +                                                  /* Sections table size component */
            (config->DDR_PayloadSize.QuadPart + config->CPU_PayloadSize.QuadPart + config->SV_PayloadSize.QuadPart);    /* Payload size component*/

        config->dumpFileHeader.TotalDumpSizeRequired = config->dumpFileHeader.DumpSize;
    }

    return ret;
}


/****************************************************************************************************
** HRESULT  GetSectionSize(
**              _In_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList,
**              _Out_ PULARGE_INTEGER sizeTotal)
**
** Description:
**  Parse the sectionList (vector list) and tally up the size value for each entry. Each entry is
**  a pointer to a RAW_DUMP_SECTION_HEADER structure which contains a size value. Each vector can
**  have zeor or more entrues.  The totla size of this table is the amount of payload space that
**  we be needed.
**
** Arguments:
**  sectionList - table of RAW_DUMP_SECTION_HEADER elements
**  sizeTotal - pointer to a ULARGE_INTEGER, return size of payload data
**
** Return:
**  S_OK
**  E_INVALIDARG - one of the arguments is nullptr
**  E_FAIL - found a bad entry in the table, nullptr indicates table has holes 
**
*****************************************************************************************************/
HRESULT GetSectionSize(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, _Out_ PULARGE_INTEGER sizeTotal)
{
    HRESULT ret = S_OK;

    if ((nullptr == sectionList) || (nullptr == sizeTotal))
    { // fail if pointers are null
        ret = E_INVALIDARG;
    }
    else
    {
        size_t nEntries = sectionList->size();

        (*sizeTotal).QuadPart = 0;

        for (size_t i = 0; i < nEntries; i++)
        {
            if (nullptr == sectionList->at(i))
            { // Fail if one of the entries is null
                ret = E_FAIL;
                (*sizeTotal).QuadPart = 0;
                break;
            }
            else
            {
                (*sizeTotal).QuadPart += (sectionList->at(i))->Size;
            }

        }

    }

    return ret;
}
/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    makeDumpFile.cpp

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/

#pragma once
#pragma optimize( "", off )

/*******************************************************************************
** NOTES for Testing:
** Write to Partition - dump too large for Talkman
**      /part  /DDRSize:201  /DDRCount:0x0007  /DDR:0:0x20000000:0x80000000 /DDR:1:0xa0000000:0x40000000 /DDR:2:0x100000000:0x30000000 /DDR:8:0x150000000:0x40000000
** Write to Partition - defaults
**      /Partition
** Write to 1GB Partition - with padding
**      /Partition  /DDRSize:4000
********************************************************************************/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "makeDumpFile.h"
#include "MakeHeader.h"
#include "processArgs.h"
#include "processDDR.h"
#include "processSV.h"

DUMP_CONFIG     config = { 0 };
std::string     outFileName = DEFAULT_DUMP_FILE_NAME;

char            testPattern[MIN_TEST_PATTERN_SIZE] = { 0 };
char            paddPattern[PADDING_BUFFER_SIZE] = { 0 };

/****************************************************************************************************
**
** Description:
**
** Arguments:
**
** Return:
**
*****************************************************************************************************/
int main(int argc, char **argv)
{
    HRESULT         hr = E_FAIL;
    DEVICE_IO       dumpFile;
    std::string     failString;

    // Sets the default output file name
    outFileName.assign(DEFAULT_DUMP_FILE_NAME);

    srand((unsigned)time(NULL));    // seed the random number generator
    config.DDR_SectionCount = DEFAULT_DDR_SECTIONS_COUNT;
    config.DDR_Size.QuadPart = DEFAULT_DDR_SECTIONS_SIZE;
    config.sectionTableOffset.QuadPart = sizeof(config.dumpFileHeader);

    if (FAILED(hr = ResizeSectionsList(&config.sectionDDR, config.DDR_SectionCount)))
    {
        printf("ERROR: failed to create the default DDR sectsions list, HRESULT: %#x\r\n", hr);
    }
    else if ( (argc > 1 ) && FAILED(hr = ProcessCommandArguments(argc, argv, &config, &failString)))
    { // Failed to process the command line arguments
        printf("ERROR: failed to parse arguemnt list, HRESULT: %#x\r\n", hr);
        if (!failString.empty())
        {
            printf("%s", failString.c_str());
        }

    }
    else if (FAILED(DumpParserWarnings(&failString)))
    { // Dump any warning messages from the command processor
        printf("ERROR: command parser warnings could not be dumped.\r\n");
        printf("%s", failString.c_str());
    }
    else if (FAILED(hr = OpenOutput(&config, &outFileName, &dumpFile)))
    { // Failed to open output (file or partition)
        if (config.outputToPartition)
        {
            printf("ERROR: could not find SVRawDump Partition, (%#lx)\r\n", hr);
        }
        else
        {
            printf("ERROR: opening file \"%s\", (%#lx)\r\n", outFileName.c_str(), hr);
        }
    }
    else if (FAILED(hr = UpdateDDRWithDefault(&config.sectionDDR, config.DDR_Size)))
    {
        printf("ERROR: UpdateDDRWithDefault(), (%#lx)\r\n", hr);
    }
    else if (FAILED(hr = UpdateSVWithDefault(&config.sectionSV, config.excludeApReg)))
    {
        printf("ERROR: UpdateSVWithDefault(), (%#lx)\r\n", hr);
    }
    else if (FAILED(hr = CreateFullSectionsTable(&config)))
    {
        printf("ERROR: Cannot create a full sections table, CreateFullSectionsTable(), (%#lx)\r\n", hr);
    }
    else if (FAILED(hr = PopulateRawDumpFileHeader(&config)))
    {
        printf("ERROR: Cannot update file header, PopulateRawDumpFileHeader(), (%#lx)\r\n", hr);
    }
    else if (FAILED(hr = DumpConfigInfo(&config, dumpFile.GetDeviceName())))
    {
        printf("ERROR: failed to dump configuration information, HRESULT: %#x\r\n", hr);
    }
    else if (FAILED(hr = dumpFile.Write((PCHAR)&config.dumpFileHeader, sizeof(config.dumpFileHeader), NULL)))
    { // Write the file header section
        printf("ERROR: failed to write file header, %#lx\r\n", hr);
    }
    else if (FAILED(hr = WriteSections(&dumpFile, &config)))
    { // Write the sections table 
        printf("ERROR: failed to write sections table, %#lx\r\n", hr);
    }
    else if ( config.writePayload && (FAILED(hr = WritePayload(&dumpFile, &config))) )
    {
        printf("ERROR: failed to write payload, (%#lx)\r\n", hr);
    }
    else
    {
        hr = S_OK;
    }

    dumpFile.Close();

    if (!config.writePayload)
    {
        printf("INFO: Payload and Padding Data not written.\r\n", hr);
    }

    return hr;
}


/****************************************************************************************************
** HRESULT OpenOutput(
**          _In_ PDUMP_CONFIG cfg,
**          _In_ std::string *fName,
**          _Out_ DEVICE_IO *oHandle)
**
** Description:
**  Open the output file or partition.  If partition, open the device and select the SVRawDump
**  partition.
**
** Arguments:
**  cfg - dump file configuration data
**  fName - dump file configuration data
**  oHandle - dump file configuration data
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT OpenOutput(_In_ PDUMP_CONFIG cfg, _In_ std::string *fName, _Out_ DEVICE_IO *oHandle)
{
    HRESULT     hr = E_FAIL;

    if (cfg->outputToPartition)
    { // Open the first device containing an SVRawDump Partition
        SYSTEM_DEVICE_INFORMATION       DeviceInfo = { 0 };

        hr = HRESULT_FROM_NT(NtQuerySystemInformation(SystemDeviceInformation, &DeviceInfo, sizeof(SYSTEM_DEVICE_INFORMATION), 0));
        if (FAILED(hr))
        {
            printf("ERROR : failed NtQuerySystemInformation(), %#x", hr);
        }
        else if (0 == DeviceInfo.NumberOfDisks)
        { // No disks detected
            printf("ERROR : NtQuerySystemInformation() found no disks");
        }
        else
        { // there are disks mounted
            UINT DiskSlot = 0;

            for (UINT DiskIndex = 0; DiskIndex < DeviceInfo.NumberOfDisks; DiskIndex++, DiskSlot++)
            { // Loop through the disks looking for partitions
                oHandle->Close();
                if (DiskSlot >= MAX_DISK_SLOTS)
                { // FAIL yf beyond a reasonable number of attached disks
                    printf("ERROR: cannot find SVRawDump partition within the first %d disk slots\r\n", DiskSlot);
                    printf("       this might occur if you are not running this utility in an admin command window.\r\n", DiskSlot);
                    hr = E_FAIL;
                    break;
                }
                else if (FAILED(hr = oHandle->Open(DiskSlot)))
                { // Open failed, determine the error
                    if (E_ACCESSDENIED == hr)
                    { // If open resulted in ACCESS DENIED, not running in ADMIN mode
                        printf("INFO: skipping disk in slot %d - ACCESS DENIED\r\n", DiskSlot);
                        continue;
                    }
                    else if (oHandle->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE)
                    { // If open resulted in invalid handle, disk is not attached, skip this slot
                        DiskIndex--;
                        printf("INFO: skipping unattached disk in slot %d\r\n", DiskSlot);
                        continue;
                    }
                    else if (oHandle->GetError() == DEVICE_IO::IO_ERROR_UNSUPPORTED_PARTITION_MBR)
                    { // If open resulted in MBR partition scheme, try next disk
                        printf("INFO: skipping MBR disk in slot %d\r\n", DiskSlot);
                        continue;
                    }
                    else
                    { // Any other error is a serious failure, cannot continue
                        hr = HRESULT_FROM_WIN32(GetLastError());
                        break;
                    }
                }
                else if (SUCCEEDED(hr = oHandle->SetPartition(DEVICE_IO::SVRAWDUMP)))
                { // Check next device if no SVRawDump partition here
                    printf("INFO: Found SVRawDump partition on disk in slot %d, %s\r\n",
                           DiskSlot,
                           ( (DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE == oHandle->GetDeviceType()) ? "Removable" :
                             (DEVICE_IO::RAW_DEVICE_TYPE == oHandle->GetDeviceType()) ? "Fixed" : "Unknown"
                           )
                    );
                    break;
                }
                else
                { // No SVRawDump Partition on this disk, keep searching
                    printf("INFO: skipping GPT disk in slot %d, no SVRawDump partition found\r\n", DiskSlot);
                }

            }

        }

    }
    else
    { // Open a file
        if (FAILED(hr = oHandle->Open(std::wstring(fName->begin(), fName->end()))))
        { // Failed to open file
            printf("ERROR: opening \"%s\", (%#lx)\r\n", fName->c_str(), hr);
        }
    }

    return hr;
}


/****************************************************************************************************
** HRESULT WriteSections(
**          _Inout_ DEVICE_IO *oFile, 
**          _In_ PDUMP_CONFIG cfg)
**
** Description:
**
** Arguments:
**  oFile - output file handle (DEVICE_IO)
**  cfg - dump file configuration data
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT WriteSections(_Inout_ DEVICE_IO *oFile, _In_ PDUMP_CONFIG cfg)
{
    HRESULT hr = S_OK;

    if ((nullptr == oFile) || (nullptr == cfg))
    {
        hr = E_INVALIDARG;
    }
    else if ( (0 == cfg->dumpFileSections.size()) &&
              (FAILED(hr = CreateFullSectionsTable(cfg)))
            )
    {
        printf("ERROR: dump file sections table is invalid and uncorrectable, (%#lx)\r\n", hr);
    }
    else
    { // Write the Sections table
        for (UINT i = 0; i < cfg->dumpFileSections.size(); i++)
        { // Write each sectsion
            PRAW_DUMP_SECTION_HEADER pSection = cfg->dumpFileSections.at(i);

            if (nullptr == pSection)
            { // Null pointer is bad!
                hr = E_POINTER;
                printf("ERROR: dump file sections %d is null, invlaid table reference, (%#lx)\r\n", hr);
            }
            else if (FAILED(hr = oFile->Write((PCHAR)pSection, sizeof(RAW_DUMP_SECTION_HEADER), NULL)))
            { // failed to Write the section
                printf("ERROR: dump file sections table is invalid and uncorrectable, (%#lx)\r\n", hr);
            }

        }

    }

    return hr;
}

#pragma optimize( "", off )
/****************************************************************************************************
** HRESULT WritePayload(
**          _Inout_ DEVICE_IO *oFile,
**          _In_ PDUMP_CONFIG cfg)
**
** Description:
**  Write payload data and padding.  Paylod data is a repeating pattern where the byte offset
**  can be translated into a an ascii character, predicatble.  Padding is all zeros.
**
** Arguments:
**  oFile - output file handle (DEVICE_IO)
**  cfg - dump file configuration data
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT WritePayload(_Inout_ DEVICE_IO *oFile, _In_ PDUMP_CONFIG cfg)
{
    HRESULT     hr = S_OK;

    if ( (nullptr == oFile) ||
         (nullptr == cfg)
       )
    {
        printf("ERROR: Invalid arguments passed to CreateDump()\r\n");
        hr = E_INVALIDARG;
    }
    else
    { // Write the payload
        ULARGE_INTEGER  bytesForPayload = { 0 };
        ULARGE_INTEGER  bytesForPadding = { 0 };
        ULARGE_INTEGER  totalWritten = { 0 };
        size_t          patternBegin = cfg->payloadOffset.QuadPart % sizeof testPattern;

        // DumpSize is computed from the paylod for each of section lists and the headers
        // this value represetns the total size of the "useful" dump data.
        totalWritten.QuadPart = cfg->payloadOffset.QuadPart;
        bytesForPayload.QuadPart = (cfg->dumpFileHeader.DumpSize - totalWritten.QuadPart);

        // Create a Test pattern
        for (size_t i = 0; i < sizeof testPattern; i++)
        {
            testPattern[i] = (i % TEST_PATTERN_SIZE) + TEST_PATTERN_BEGIN;
        }

        // If writing to a partition, the payload cannot exceeded the partition size.
        if (cfg->outputToPartition)
        {
            if (cfg->dumpFileHeader.DumpSize > oFile->GetCurrentPartitionSize())
            { // If the intended payload is bigger than the partition, this is an error.
                printf("ERROR: payload size is larger than the target partition.\r\n");
                hr = E_FAIL;
            }
            else
            { // If the intended payload is smaller than the partition, add padding.
                bytesForPadding.QuadPart = oFile->GetCurrentPartitionSize() - cfg->dumpFileHeader.DumpSize;
            }
            printf("INFO: Writing Payload data to partition\r\n");
        }
        else
        {
            printf("INFO: Writing Payload data to file\r\n");
        }

        if (SUCCEEDED(hr) && (0 != patternBegin))
        { // This write happens if there is a partial block to write as the first block
            size_t        bWrite;

            if (FAILED(hr = oFile->Write((PCHAR)&testPattern[patternBegin], (size_t)((sizeof testPattern) - patternBegin), &bWrite)))
            { // failed to write the a partial first block
                printf("ERROR: failed to write first block of test pattern, (%#lx)\r\n", hr);
            }
            else
            { // successful write, adjust the payload bytes to be written
                bytesForPayload.QuadPart -= bWrite;
                totalWritten.QuadPart += bWrite;
            }

        }

        if (SUCCEEDED(hr))
        { // write the remaining payload data
            ULARGE_INTEGER  bytesWritten;

            if (FAILED(hr = WritePattern(oFile, (PCHAR)testPattern, sizeof testPattern, bytesForPayload, &bytesWritten)))
            { // Failed to write payload data
                printf("ERROR: failed to write payload data\r\n");
            }
            else
            { // Successful write
                if (bytesForPayload.QuadPart != bytesWritten.QuadPart)
                { // fail for incomplete write
                    printf("ERROR: incomplete payload write\r\n");
                    hr = E_FAIL;
                }
                else
                {
                    totalWritten.QuadPart += bytesWritten.QuadPart;
                }

            }

        }


        if (SUCCEEDED(hr))
        { // write the padding data
            ULARGE_INTEGER  bytesWritten;

            printf("INFO: Writing padding data to %s\r\n", (cfg->outputToPartition) ? "partition" : " file" );
            if (FAILED(hr = WritePattern(oFile, (PCHAR)paddPattern, sizeof paddPattern, bytesForPadding, &bytesWritten)))
            { // Failed to write payload data
                printf("ERROR: failed to write padding data\r\n");
            }
            else
            { // Successful write
                totalWritten.QuadPart += bytesWritten.QuadPart;
                if (bytesForPadding.QuadPart != bytesWritten.QuadPart)
                { // fail for incomplete write
                    printf("ERROR: incomplete padding write\r\n");
                    hr = E_FAIL;
                }

            }

        }

    }

    return hr;
}


/****************************************************************************************************
** HRESULT WritePattern(
**          _Inout_ DEVICE_IO *oFile,
**          _In_ PCHAR pattern,
**          _In_ size_t patternSize,
**          _In_ ULARGE_INTEGER writeSize,
**          _Out_ ULARGE_INTEGER *bytesWritten)
**
** Description:
**  Writes the input pattern to the output file handle (oFile) untill the number of bytes (writeSize)
**  is satisfied.
**
** Arguments:
**  oFile - output file handle (DEVICE_IO)
**  pattern - pattern buffer to use for writing payload data, repeating pattern
**  patternSize - size of the pattern buffer
**  writeSize - total size to write
**  bytesWritten - number of bytes written, returned to caller
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT WritePattern(_Inout_ DEVICE_IO *oFile, _In_ PCHAR pattern, _In_ size_t patternSize, _In_ ULARGE_INTEGER writeSize, _Out_ ULARGE_INTEGER *bytesWritten)
{
    HRESULT hr = S_OK;

    if ((nullptr == oFile) ||
        (nullptr == pattern)
        )
    { // Bad pointers
        printf("ERROR: invalid pointers passed to WritePattern()");
        hr = E_INVALIDARG;
    }
    else if ((0 == patternSize) ||
             (0 == writeSize.QuadPart)
             )
    { // Invalid arguments
        printf("ERROR: invalid size values passed to WritePattern()");
        hr = E_INVALIDARG;
    }
    else
    {
        size_t          bWrite = 0;
        ULARGE_INTEGER  bytesToWrite;
        ULONGLONG       meg = 0;

        bytesToWrite.QuadPart = writeSize.QuadPart;
        if (nullptr != bytesWritten)
        {
            (*bytesWritten).QuadPart = 0;
        }

        while (bytesToWrite.QuadPart > 0)
        {
            if (0x00000000015578c8 == bytesToWrite.QuadPart)
            {
                bytesToWrite.QuadPart = 0x00000000015578c8;
            }

            if (FAILED(hr = oFile->Write((PCHAR)pattern, patternSize, &bWrite)))
            {
                printf("\r\nERROR: failed to write at offset %lld, (%#x)\r\n", (ULONGLONG)(writeSize.QuadPart - bytesToWrite.QuadPart), hr);
                break;
            }

            char c = '\\';
            meg = (ULONGLONG)(writeSize.QuadPart - bytesToWrite.QuadPart) / ONE_MEGABYTE;

            switch (meg % 4)
            {
                case 1:
                    c = '|';
                    break;
                case 2:
                    c = '/';
                    break;
                case 3:
                    c = '-';
                    break;
                default:
                case 0:
                    break;
            }

            printf("\r %c  %ld Mbytes written             ", c, meg);
            bytesToWrite.QuadPart -= bWrite;
            if (nullptr != bytesWritten)
            {
                (*bytesWritten).QuadPart += bWrite;
            }

            if (bytesToWrite.QuadPart < patternSize)
            {
                patternSize = bytesToWrite.QuadPart;
            }

        }

        if (SUCCEEDED(hr))
        {
            printf("\r *  %ld Mbytes written successfully\r\n", meg);
        }

    }


    return hr;
}


/****************************************************************************************************
** HRESULT CreateFullSectionsTable(_Inout_ PDUMP_CONFIG cfg)
**
** Description:
**  Create a full section table from the various tables in cgf.
**
** Arguments:
**  cfg - dump file configuration data
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT CreateFullSectionsTable(_Inout_ PDUMP_CONFIG cfg)
{
    HRESULT hr = S_OK;
    cfg->dumpFileHeader.SectionsCount = (UINT)(cfg->sectionDDR.size() + cfg->sectionSV.size() + cfg->sectionCPU.size());
    std::vector<PRAW_DUMP_SECTION_HEADER> dumpFileSections(cfg->dumpFileHeader.SectionsCount);      // Size the final table appropriately

    cfg->payloadOffset.QuadPart = cfg->sectionTableOffset.QuadPart + SECTION_TABLE_SIZE(cfg->dumpFileHeader.SectionsCount);

    if (FAILED(hr = sortDDRSectionTable(cfg->sectionDDR, cfg->DDR_Order)))
    {
        printf( "ERROR: failed to sort DDR section table (%s), (%#lx)\r\n"
                ,((DDR_ORDER_ASLISTED == cfg->DDR_Order)    ? "AS LISTED" :
                   ((DDR_ORDER_ASCENDING == cfg->DDR_Order)  ? "ASCENDING" :
                     ((DDR_ORDER_DESCENDING == cfg->DDR_Order) ? "DESCENDING" :
                       ((DDR_ORDER_RANDOM == cfg->DDR_Order)     ? "RANDOM" :
                         "BAD ORDER FLAG"))))
                ,hr
              );

    }
    else if (DDR_PROXIMITY_ADJACENT == cfg->DDR_Proximity)
    { // Copy the sections to destination vector, as listed

        if (FAILED(hr = copySectionTable(dumpFileSections, cfg->sectionSV)))
        { //Failed to copy section
            printf("ERROR: failed to copy SV section (ADJACENT), (%#lx)\r\n", hr);
        }
        else if (FAILED(hr = copySectionTable(dumpFileSections, cfg->sectionDDR)))
        { //Failed to copy section
            printf("ERROR: failed to copy DDR section (ADJACENT), (%#lx)\r\n", hr);
        }
        else if (FAILED(hr = copySectionTable(dumpFileSections, cfg->sectionCPU)))
        { //Failed to copy section
            printf("ERROR: failed to copyCPU section (ADJACENT), (%#lx)\r\n", hr);
        }

    }
    else if (DDR_PROXIMITY_SCATTER == cfg->DDR_Proximity)
    { // Copy the sections to destination vector, as disjoint as possilbe
        UINT idxSV  = 0;
        UINT idxDDR = 0;
        UINT idxCPU = 0;
        
        for (UINT idxDst = 0; idxDst < dumpFileSections.size(); idxDst++)
        {
            UINT  sectionSelected = rand() % RAW_DUMP_SECTION_TYPE_MAX;

            switch (sectionSelected)
            {
                case RAW_DUMP_SECTION_TYPE_DDR_RANGE:   // = 0x1,
                    if ( (0 != cfg->sectionDDR.size()) &&
                         (idxDDR < cfg->sectionDDR.size())
                       )
                    {
                        dumpFileSections[idxDst] = cfg->sectionDDR[idxDDR++];
                    }
                    else
                    {
                        idxDst--;
                    }
                    break;

                case RAW_DUMP_SECTION_TYPE_CPU_CONTEXT: // = 0x2,
                    if ( (0 != cfg->sectionCPU.size()) &&
                         (idxCPU < cfg->sectionCPU.size())
                       )
                    {
                        dumpFileSections[idxDst] = cfg->sectionCPU[idxDDR++];
                    }
                    else
                    {
                        idxDst--;
                    }
                    break;

                case RAW_DUMP_SECTION_TYPE_SV_SPECIFIC: // = 0x3,
                    if ( (0 != cfg->sectionSV.size()) &&
                         (idxSV < cfg->sectionSV.size())
                       )
                    {
                        dumpFileSections[idxDst] = cfg->sectionSV[idxSV++];
                    }
                    else
                    {
                        idxDst--;
                    }
                    break;

                case RAW_DUMP_SECTION_TYPE_RESERVED:    // = 0x0,
                case RAW_DUMP_SECTION_TYPE_MAX:
                default:
                    idxDst--;
                    break;
            }

        }

    }

    if (SUCCEEDED(hr))
    {
        dumpFileSections[0]->Offset = cfg->payloadOffset.QuadPart;

        if (SUCCEEDED(hr = setTableOffsets(dumpFileSections)))
        {
            cfg->dumpFileSections = dumpFileSections;
#ifdef TOOL_DEBUG
            DumpVectorTable(cfg->dumpFileSections);
#endif
        }

    }

    return hr;
}


/****************************************************************************************************
** HRESULT setTableOffsets(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vDst)
**
** Description:
**  Update the offsets in the provided vDist vector (table).
**
** Arguments:
**  vDst - vector table to modify
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT setTableOffsets(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vDst)
{
    HRESULT hr = S_OK;
    size_t s = vDst.size();
    

    for (size_t idx = 1; idx < s; idx++)
    {
        vDst[idx]->Offset = vDst[idx - 1]->Offset + vDst[idx - 1]->Size;
    }


    return hr;
}


/****************************************************************************************************
** HRESULT copySectionTable(
**          _Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vDst,
**          _In_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc)
**
** Description:
**  Update the destination vector table (vDst) by filling in all holes from source (vSrc).  Algorithm
**  is to begin at the beginning of vDst, fill holes from vSrc and append vDst is items from vSrc
**  overflow the current vDst.
**
** Arguments:
**  vDst - vector table to modify
**  vSrc - source vector table
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT copySectionTable(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vDst, _In_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc)
{
    UNREFERENCED_PARAMETER(vDst);
    UNREFERENCED_PARAMETER(vSrc);
    HRESULT hr = S_OK;

    if ( (nullptr == &vDst) || (nullptr == &vSrc) )
    { //Fail for bad reference
        hr = E_INVALIDARG;
    }
    else if ( !vSrc.empty() )
    { // Copy the table


        UINT nEmpty = 0;
        for (size_t idxDst = 0; idxDst < vDst.size(); idxDst++)
        { // Count empty rows - to determine if table needs expansion
            if (nullptr == vDst[idxDst])
            {
                nEmpty++;
            }

        }

        if (nEmpty < vSrc.size())
        { // Resize verctor if we need more space
            UINT newSize = (UINT)(vDst.size() + (vSrc.size() - nEmpty));

            hr = ResizeSectionsList(&vDst, newSize );
        }

        if (SUCCEEDED(hr))
        {
            size_t idxSrc = 0;
            for (size_t idxDst = 0; idxDst < vDst.size(); idxDst++)
            {
                if (nullptr != vDst[idxDst])
                { // skip non-null entries in destination vector
                    continue;
                }
                else if (idxSrc < vSrc.size())
                { //
                    vDst[idxDst] = vSrc[idxSrc++];
                }
                else
                {
                    break;
                }

            }

            if (idxSrc != vSrc.size())
            { //
                hr = E_FAIL;
            }

        }

    }

    return hr;
}


/****************************************************************************************************
** HRESULT sortDDRSectionTable(
**          _Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc,
**          _In_ DDR_ORDER order)
**
** Description:
**  Sort or randomize the table provided (vSrc) using the designated order.  This function is 
**  specific to the DDR table.
**
** Arguments:
**  vSrc - source vector table
**  order - sort order (ascending | descending | random)
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT sortDDRSectionTable(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc, _In_ DDR_ORDER order)
{
    HRESULT hr = S_OK;

#ifdef TOOL_DEBUG
    printf("== == == ==  Original DDR Vector  == == == ==\r\n");
    DumpVectorTable(vSrc);
    printf("== == == == == == == == == == == == == == ==\r\n");
#endif

    switch (order)
    {
        case DDR_ORDER_ASCENDING:
        case DDR_ORDER_DESCENDING:
            hr = sortVector(&vSrc, order);
            break;

        case DDR_ORDER_RANDOM:
            hr = randomizeVector(&vSrc);
            break;

        case DDR_ORDER_ASLISTED:
        default:
            break;
    }

#ifdef TOOL_DEBUG
    printf("\r\n== == == ==  Final DDR Vector  == == == ==\r\n");
    DumpVectorTable(vSrc);
    printf("== == == == == == == == == == == == == == ==\r\n");
#endif
    return hr;
}


/****************************************************************************************************
** HRESULT sortVector(
**          _Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *vSrc,
**          _In_ DDR_ORDER order)
**
** Description:
**  Sort the table provided (vSrc) using the designated order.  Typically, DDR sections are
**  sorted, this function sorts any vector table.
**
** Arguments:
**  vSrc - source vector table
**  order - sort order (ascending | descending)
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT sortVector(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *vSrc, _In_ DDR_ORDER order)
{
    HRESULT hr = S_OK;


    if ( (nullptr == vSrc) ||
         ((DDR_ORDER_ASCENDING != order) && (DDR_ORDER_DESCENDING != order))
       )
    {
        hr = E_INVALIDARG;
    }
    else
    {
        size_t n = vSrc->size();

        for (size_t c = 0; c < (n-1); c++)
        {
            for (size_t d = 0; d < (n - c - 1); d++)
            {
                if ( ((DDR_ORDER_ASCENDING == order)  && (vSrc->at(d)->u.DDRInformation.Base > vSrc->at(d + 1)->u.DDRInformation.Base)) ||
                     ((DDR_ORDER_DESCENDING == order) && (vSrc->at(d)->u.DDRInformation.Base < vSrc->at(d + 1)->u.DDRInformation.Base))
                   )
                { //swap
                    PRAW_DUMP_SECTION_HEADER tmp = vSrc->at(d);
                    vSrc->at(d) = vSrc->at(d + 1);
                    vSrc->at(d + 1) = tmp;
                }

            }

        }

    }

    return hr;
}


/****************************************************************************************************
** HRESULT randomizeVector(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> *vSrc)
**
** Description:
**  Randomizes the table provided (vSrc)
**
** Arguments:
**  vSrc - vector table to randomize
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT randomizeVector(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> *vSrc)
{
    HRESULT hr = S_OK;

    if (nullptr == vSrc)
    {
        hr = E_INVALIDARG;
    }
    else
    {
        UINT  s = (UINT)vSrc->size();      // Destination index

        // Temporary vector - all null
        std::vector<PRAW_DUMP_SECTION_HEADER> ddrTmp(vSrc->size());

        for (size_t i = 0; i < s; i++)
        { // Index through the source
            UINT    idx = rand() % s;

            // Find the nearest open slot
            // Ideally, IDX will be the index of an open slot in the detination vector.
            // This case will be caught in the first "if".  WHen IDX points to an
            // occupued slot, the search for an open slot looks at the adjacent slots
            // from 1 to "s" until an open slot is found.
            for (UINT j = 0; j < s; j++)
            {
                if ( (idx >= j) && ((idx - j) >= 0) && (nullptr == ddrTmp[idx - j]) )
                { // slot before is open
                    idx -= j;
                }
                else if ( ((idx + j) < s) && (nullptr == ddrTmp[idx + j]) )
                { // slot after is open
                    idx += j;
                }
                else
                { // no match, increase the distance and check again
                    continue;
                }

                // Fall through to here when we have an open slot
                ddrTmp[idx] = vSrc->at(i);
                break;
            }

        }
        
        *vSrc = ddrTmp;
    }

    return hr;
}


#define STRLEN_ADJUST 10
/****************************************************************************************************
** void DumpVectorTable(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc)
**
** Description:
**  Prints Vector table, typically receives dumpFileSections from cfg
**
** Arguments:
**  vSrc - vector table to dump
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
void DumpVectorTable(_In_ std::vector<PRAW_DUMP_SECTION_HEADER> &vSrc)
{
    for (size_t i=0; i< vSrc.size(); i++)
    { // Iterate the vector
        printf("Name:%s%s \tType: %s \tOffset: %#16I64x \tSize: %#16I64x"
               , vSrc[i]->Name
               , (strlen((char *)vSrc[i]->Name) < STRLEN_ADJUST) ? "\t" : ""
               , (((RAW_DUMP_SECTION_TYPE_RESERVED == vSrc[i]->Type) ? "RESERVED" :
               ((RAW_DUMP_SECTION_TYPE_DDR_RANGE == vSrc[i]->Type) ? "DDR     " :
                   ((RAW_DUMP_SECTION_TYPE_CPU_CONTEXT == vSrc[i]->Type) ? "CPU     " :
                ((RAW_DUMP_SECTION_TYPE_SV_SPECIFIC == vSrc[i]->Type) ? "SV      " :
                 "UNKNOWN ")))))
               , vSrc[i]->Offset
               , vSrc[i]->Size);

        if (RAW_DUMP_SECTION_TYPE_DDR_RANGE == vSrc[i]->Type)
        {
            printf(" \tBase: %#16I64x", vSrc[i]->u.DDRInformation.Base);
        }

        printf("\r\n", vSrc[i]->u.DDRInformation.Base);
    }
    return;
}


/****************************************************************************************************
** HRESULT DumpConfigInfo(
**          _Inout_ PDUMP_CONFIG cfg,
**          _In_ std::string fName)
**
** Description:
**  Prints config table information from cfg.
**
** Arguments:
**  cfg - configuration table
**  fName - string containing the output file
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT DumpConfigInfo(_Inout_ PDUMP_CONFIG cfg, _In_ std::wstring fName)
{
    HRESULT hr = S_OK;

    if (nullptr == cfg)
    {
        hr = E_POINTER;
    }
    else
    {
        printf("= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =\r\n");
        if (cfg->outputToPartition)
        {
            printf("\t  Creating Raw Dump on disk : %ls \r\n", fName.c_str());
        }
        else
        {
            printf("\t     Creating Raw Dump File : %ls \r\n", fName.c_str());
        }

        printf("\t            Number of cores : %d\r\n", cfg->CoreCount);

        printf("\t                  DDR Order : %s\r\n",
            ((cfg->DDR_Order == DDR_ORDER_ASCENDING) ? "Ascending" :
             (cfg->DDR_Order == DDR_ORDER_DESCENDING) ? "Descending" :
             (cfg->DDR_Order == DDR_ORDER_ASLISTED) ? "As Listed" :
             (cfg->DDR_Order == DDR_ORDER_RANDOM) ? "Radom" : "ERROR"
            )
        );
        printf("\t              DDR Proximity : %s\r\n",
            ((cfg->DDR_Proximity == DDR_PROXIMITY_ADJACENT) ? "Adjacent" :
             (cfg->DDR_Proximity == DDR_PROXIMITY_SCATTER) ? "Scattered" : "ERROR"
            )
        );

        printf("\t               DDR Sections : %d\r\n", cfg->sectionDDR.size());
        printf("\t                SV Sections : %d\r\n", cfg->sectionSV.size());
        printf("\t               CPU Sections : %d\r\n", cfg->sectionCPU.size());
        printf("\t             Total Sections : %d\r\n", cfg->sectionSV.size() + cfg->sectionDDR.size() + cfg->sectionCPU.size());
        if (0 != cfg->requestedRawDumpFileSize.QuadPart)
        {
            printf("\t        File Size Requested : %lld (%#016I64x)\r\n", cfg->requestedRawDumpFileSize, cfg->requestedRawDumpFileSize);
        }

        // Section to display file failures
        if (cfg->badHeaderVersion ||            // version incorrect
            cfg->badHeaderFlags ||              // header flags incorrect
            cfg->badHeaderNumberOfSections ||   // Missing sections
            cfg->DDROverlap ||                  // overlapping DDR sections
            cfg->excludeTZ ||
            cfg->excludeApReg)
        {
            printf("\r\n\t  - - - - - - - File exhibits the following error(s) - - - - - - -\r\n");
            if (cfg->badHeaderVersion)
            { // version incorrect
                printf("\t                            : Header Version incorrect\r\n");
            }

            if (cfg->badHeaderFlags)
            { // header flags incorrect
                printf("\t                            : Header Flags invalid\r\n");
            }

            if (cfg->badHeaderNumberOfSections)
            { // Missing sections
                printf("\t                            : Missing sections (section table incomplete)\r\n");
            }

            if (cfg->DDROverlap)
            { // overlapping DDR sections
                printf("\t                            : one or more DDR sections overlap\r\n");
            }

            if (cfg->excludeTZ)
            { // no TZ data
                printf("\t                            : missing Trust Zone data\r\n");
            }

            if (cfg->excludeApReg)
            { // no TZ data
                printf("\t                            : missing AP Reg data\r\n");
            }

        }
        else
        {
            printf("\r\n\t  - - - - - - - - - File exhibits no error(s) - - - - - - - - - -\r\n");
        }

        /*
        // Output control values and flags
        ULARGE_INTEGER                            actualRawDumpFileSize;        // Computed size of the output file, determined from table data
        ULARGE_INTEGER                          sectionTableOffset;
        ULARGE_INTEGER                          payloadOffset;

        // DDR Section parameters
        UINT32                                  DDR_SectionCount;            // Number of sections, can be set by /DDRCount
        ULARGE_INTEGER                            DDR_Size;                    // Default DDR section size, can be set by /DDRSize
        ULARGE_INTEGER                            DDR_PayloadSize;            // Total Size of the data for all segments referred to here

        // ApReg Section parameters
        ULARGE_INTEGER                            CPU_PayloadSize;            // Total Size of the data for all segments referred to here

        // SV Section parameters
        ULARGE_INTEGER                            SV_PayloadSize;                // Total Size of the data for all segments referred to here

        } DUMP_CONFIG, *PDUMP_CONFIG;

        */
        printf("\r\n\t  - - - - - - - - - - - - - File Header - - - - - - - - - - - - - -\r\n");
        printf("\t                  Signature : %d %s \r\n", cfg->dumpFileHeader.Signature, (RAW_DUMP_HEADER_SIGNATURE != cfg->dumpFileHeader.Signature) ? "(invalid)" : "");
        printf("\t                    Version : %d %s\r\n", cfg->dumpFileHeader.Version, (RAW_DUMP_HEADER_VERSION != cfg->dumpFileHeader.Version) ? "(invalid)" : "");
        printf("\t                      Flags : %d %s\r\n", cfg->dumpFileHeader.Flags, (RAW_DUMP_HEADER_EXPECTED_BITS != cfg->dumpFileHeader.Flags) ? "(invalid)" : "");
        printf("\t                     OsData : %#16I64x\r\n", cfg->dumpFileHeader.OsData);
        printf("\t                CPU Context : %#16I64x\r\n", cfg->dumpFileHeader.CpuContext);
        printf("\t              Reset Trigger : %#16I64x\r\n", cfg->dumpFileHeader.ResetTrigger);
        printf("\t                  Dump Size : %#16I64x\r\n", cfg->dumpFileHeader.DumpSize);
        printf("\t   Total Dump Size Required : %#16I64x\r\n", cfg->dumpFileHeader.TotalDumpSizeRequired);
        printf("\t             Sections Count : %d\r\n", cfg->dumpFileHeader.SectionsCount);

        printf("\r\n\t  - - - - - - - - - - - - - Section Table - - - - - - - - - - - - -\r\n");
        DumpVectorTable(cfg->dumpFileSections);
        printf("\r\n\t  - - - - - - - - - - - - - Payload Data - - - - - - - - - - - - - -\r\n");
    }

    return hr;
}


/****************************************************************************************************
** HRESULT DumpParserWarnings(_In_ std::string *str)
**
** Description:
**  Prints warning messages, from a string (str).
**
** Arguments:
**  str - string to print
**
** Return:
**  HRESULT
**
*****************************************************************************************************/
HRESULT DumpParserWarnings(_In_ std::string *str)
{
    HRESULT hr = S_OK;

    if (nullptr == str)
    {
        hr = E_POINTER;
    }
    else if (!str->empty())
    { // Dump any warning messages from the command processor
        printf("WARNING: command parser found non-critical errors\r\n");
        printf("%s", str->c_str());
    }

    return hr;
}


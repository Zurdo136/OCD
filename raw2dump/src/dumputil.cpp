/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:
    dumputil.cpp

Environment:
    User Mode

--*/
#include "dumputil.h"
#include "DumpExtract64.h"
#include "apreg64.h"
#include <bugcodes.h>



//
// ----------------------------- Global Tables ----------------------------------------------------------------
//

// Signature to help locate the in-memory dump header
UCHAR InMemoryDumpHeaderMagicString[] = {
    0x3B, 0x49, 0x53, 0x53, 0x94, 0x45, 0x2E, 0x30,
    0xD4, 0xCB, 0xDA, 0x97, 0xF1, 0x11, 0x02, 0xB5,
    0xE8, 0x36, 0x08, 0x61, 0x88, 0x70, 0x9B, 0x19 };


// Signature to help locate the in-memory dump header
UCHAR InMemoryDumpHeaderMagicStringPlus[] = {
    0x3B, 0x49, 0x53, 0x53, 0x94, 0x45, 0x2E, 0x30,
    0xD4, 0xCB, 0xDA, 0x97, 0xF1, 0x11, 0x02, 0xB5,
    0xE8, 0x36, 0x08, 0x61, 0x88, 0x70, 0x9B, 0x19,
    0x50, 0x41, 0x47, 0x45, 0x44, 0x55, 0x4d, 0x50 };

//
// -------------------------------------------------------------------------------------------------------------
//

//
// This is used just for calculating the correct
// offset of the DUMP_HEADER
//
UCHAR DumpHeaderSig[] = { 0x50, 0x41, 0x47, 0x45, 0x44, 0x55, 0x4d, 0x50 };
#define START_OF_DUMP_HEADER_SIG_IN_MAGIC_STRING (sizeof(InMemoryDumpHeaderMagicString))

//
// --------------------------- Function Prototypes ------------------------------------------------------------
//
HRESULT
UpdateContextWithAPReg(
    _Inout_ PDMP_CONTEXT Context
 );

HRESULT
GetAPReg(
    _Inout_ PDMP_CONTEXT Context
);
//
// ------------------------- Function Definitions -------------------------------------------------------------
//

HRESULT
ExtractRawDumpFile(
    PDMP_CONTEXT Context,
    LPCWSTR FileName
    )
{
    HRESULT hr = E_FAIL; 

    Context->fileOffset.QuadPart = 0;
    Context->RawDumpFileLength.QuadPart = 0;

    if (FAILED(Context->hRawFile.Open(FileName)))
    {
        hr = HRESULT_FROM_WIN32(GetLastError());
        TraceHRESULT("CreateFileW of Open Raw Dump failed", hr);
        goto Exit;
    }
    else
    { // Open was sucessful, get the file size
        Context->RawDumpFileLength.QuadPart = Context->hRawFile.GetCurrentFileSize();
        if (0 == Context->RawDumpFileLength.QuadPart)
        { // Fail if file is zero
            TraceInfo("Error: RawDumpFileLength is invalid");
            goto Exit;
        }
    }

    if (Context->IsDeviceInfoInRawDump) {
        hr = UpdateContextFromEmbedDeviceInfo(Context);
        if (FAILED(hr)){
            TraceHRESULT("Failed to read device info embedded in the rawdump", hr);
            goto Exit;
        }
    }
    else {
        hr = UpdateContextFromXml(Context);
        if (FAILED(hr)){
            TraceHRESULT("Failed to read the raw dump info file", hr);
            goto Exit;
        }
    }

    NTSTATUS status = ExtractRawDumpToFile(Context);
    hr = HRESULT_FROM_NT(status);

Exit:
    Context->hRawFile.Close();
    return hr;
}



NTSTATUS
ExtractRawDumpToFile(PDMP_CONTEXT Context)
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    HRESULT  hr = E_FAIL;

    TraceInfo("Found the File. Checking for valid RAW_DUMP_HEADER");
    hr = VerifyRawDumpHeader(Context);
    if (FAILED(hr)) {
        TraceHRESULT("Raw Dump Partition header is invalid", hr);
        goto Exit;
    }

    TraceInfo("Found a valid dump header. Validating the section table");
    status = VerifyRawDumpSectionTable(Context);
    if (FAILED(status)) {
        TraceNTSTATUS("Raw Dump Partition Section table is invalid", status);
        goto Exit;
    }

    TraceInfo("Got a valid section table. Building a memory based on DDR sections");
    status = BuildDDRMemoryMap(Context);
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to Build DDR Memory Map", status);
        goto Exit;
    }

    TraceInfo("Built memory map. Reading rawdumpinfo xml file to get more info.");

    //
    // Allocate a large chunk of memory for buffering.
    //
    Context->IoBuffer = (PVOID)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, IO_BUFFER_SIZE);
    if (Context->IoBuffer == nullptr) {
        status = STATUS_NO_MEMORY;
        TraceNTSTATUS("Failed to allocate IoBuffer", status);
        goto Exit;
    }

    TraceInfo("Getting the pre-built DUMP_HEADER");
    hr = GetDumpHeader(Context);
    if (FAILED(hr)) {
        TraceHRESULT("GetDumpHeader failed", hr);
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }
    if(Context->DumpHeaderStatus != DHS_VALID) {
        TraceInfo("Could not match the dump header instance ID");
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    if(Context->Is64Bit) {
        status = ExtractWindowsDumpFile64(Context);
    }
    else {
        status = ExtractWindowsDumpFile(Context);
    }

Exit:
    return status;
}


HRESULT
ExtractWindowsDumpFile(PDMP_CONTEXT Context)
/*--
The function does the following.

1. search in memory Dump header
2. validate DDR
3. Init Dump file
4. get debug block data
5. write dump header to dump file
6. write DDR to dump file
7. update dump file with cpu context

++*/
{
    NTSTATUS    status = STATUS_UNSUCCESSFUL;
    HRESULT     hr = S_OK;

    TraceInfo("Validating DUMP_HEADER's memory descriptors against DDR sections");
    hr = ValidateDDRAgainstPhysicalMemoryBlock(Context);
    if (FAILED(hr)) {
        TraceHRESULT("ValidateDDRAgainstPhysicalMemoryBlock failed", hr);
        goto ExitHR;
    }

    //
    // Build the complete DDR memory map. 
    //
    TraceInfo("Memory descriptor validated. Building complete memory map");
    hr = BuildCompleteMemoryMap(Context);
    if (FAILED(hr)) {
        TraceHRESULT("BuildCompleteMemoryMap failed", hr);
        goto ExitHR;
    }

    TraceInfo("Memory Map built. Init the Windows dump file");
    hr = InitDumpFile(Context);
    if (FAILED(hr)){
        TraceHRESULT("InitDumpFile failed", hr);
        goto ExitHR;
    }

    TraceInfo("Writing DUMP_HEADER to the dump file");
    hr = WriteDumpHeader(Context);
    if (FAILED(hr)) {
        TraceHRESULT("WriteDumpHeader failed", hr);
        goto ExitHR;
    }

    TraceInfo("Writing DDR section to the dump file");
    hr = WriteDDR(Context);
    if (FAILED(hr)) {
        TraceHRESULT("WriteDDR failed", hr);
        goto ExitHR;
    }

    TraceInfo("Writing secondary data to the dump file");
    hr = WriteSVSpecific(Context);
    if (FAILED(hr)) {
        TraceHRESULT("WriteSVSpecific failed", hr);
        goto ExitHR;
    }

    if (!DbgClient::Initialize(Context->WindowsDumpFilePath, NULL)) {
        TraceInfo("Error: Failed to initialize Debug Client");
        goto Exit;
    }

    TraceInfo("Try to determine if KdDebuggerDataBlock is encoded");
    status = GetKdDebuggerDataBlock(Context);
    if (FAILED(status)) {
        TraceNTSTATUS("GetKdDebuggerDataBlock failed", status);
        hr = E_FAIL;
        goto Exit;
    }

    TraceInfo("Update CPU context");
    switch (Context->DumpHeader32->MachineImageType){
    case IMAGE_FILE_MACHINE_I386:
        if (Context->CPUContextSectionCount >0){
            status = UpdateContextX86(Context);
        }
        break;

    case IMAGE_FILE_MACHINE_ARM:
    case IMAGE_FILE_MACHINE_ARMNT:
        //
        // Get the AP_REG data from memory and do some validation.
        //
        TraceInfo("Try getting AP_REG");

        hr = GetAPReg(Context);
        if (FAILED(hr)) {
            TraceHRESULT("GetAPReg failed", hr);
        }

        status = UpdateContextWithAPReg(Context);
        if (FAILED(status)) {
            TraceHRESULT("UpdateContextWithAPReg failed", hr);
            goto Exit;
        }
        break;

    default:
        hr = E_FAIL;
        TraceHRESULT("Unsupported CPU Context type", hr);
        break;
    }

 //   if (Context->DumpHeaderStatus != DHS_VALID) {
 //       if (FAILED(hr = WriteFakeDumpHeader(Context))) {
 //           // not fatal
 //           TraceHRESULT("WriteFakeDumpHeader failed", hr);
 //       }
 //   }

    if (FAILED(hr = WriteInMemDiagBuffer(Context))) {
        // not fatal
        TraceHRESULT("WriteInMemDiagBuffer failed", hr);
    }

    status = STATUS_SUCCESS;

    TraceInfo("Wrote to Dump file successfully");

Exit:
    return HRESULT_FROM_NT(status);

ExitHR:
    return hr;

}

HRESULT
UpdateContextWithAPReg(
    _Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:
    Calls Legacy or 64bit supported APREG format based on the global flag gIsAPReg64

    Arguments:
    Context - Pointer to DMPCONTEXT
 
Return Value:
    NT status code.

--*/
{

    NTSTATUS status = STATUS_UNSUCCESSFUL;
    HRESULT  hr = E_FAIL;
    if(Context->isAPREG64 == FALSE) {
        // 
        // AP_REG structure was of legacy type.
        //
        status = UpdateContextWithAPRegLegacy(Context);
        hr = HRESULT_FROM_NT(status);
    }
    else
    {
        //
        //  AP_REG Structure is the new 64bit friendly structure. 
        //
        hr = UpdateContextWithAPReg64(Context);
    }

    return hr;
}

HRESULT
GetAPReg(
    _Inout_ PDMP_CONTEXT Context
    )
{
    HRESULT  hr = E_FAIL;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    TraceInfo("Checking for Legacy AP_REG first"); 
    status = GetAPRegLegacy(Context);

    if (NT_SUCCESS(status)) {
        hr = S_OK;
        goto Exit;
    }

    TraceNTSTATUS("Failed to locate and validate AP_REG. Status", status);
    TraceInfo("Checking if this is the new APREG64 format");

    //
    // APREG address is available and parsing APREG like the legacy way failed. 
    // This could be the new APREG format. 
    //
    hr = GetAPReg64(Context);
    if (!SUCCEEDED(hr)) {
        TraceNTSTATUS(L"Failed to locate and validate AP_REG 64. Status:0x%08X\r\n", status);
        goto Exit;
    }
    Context->isAPREG64 = TRUE;

Exit:
   return hr;
}


HRESULT VerifyRawDumpHeader(PDMP_CONTEXT Context)
/*++

    Routine Description:

    This function validates the RAW_DUMP_HEADER. The following are checked.

    1. RAW_DUMP_HEADER.Signature
    2. RAW_DUMP_HEADER.Version
    3. RAW_DUMP_HEADER.Flags
    4. RAW_DUMP_HEADER.DumpSize
    5. RAW_DUMP_HEADER.SectionCount

    If things check out, it will allocate and read the complete dump table
    to memory.

    Arguments:

        Context - Pointer to DmpContext

    Return Value:

        HRESULT.

--*/
{
    UINT32      expectedBits = 0;
    HRESULT     hr;

    //
    // Read in the header and validate the fields.
    //
    size_t      bytesProcessed = 0;

    if ( FAILED(hr = Context->hRawFile.SetPos(0))
         || FAILED(hr = Context->hRawFile.Read((PCHAR)&Context->RawDumpHeader, sizeof(Context->RawDumpHeader), &bytesProcessed))
         || (0 == bytesProcessed)
      )
    {
        TraceHRESULT("Read RAW_Dump File Header failed", hr);
        goto Exit;
    }

    //
    // Check the signature.
    //
    if (Context->RawDumpHeader.Signature != RAW_DUMP_HEADER_SIGNATURE)
    {
        TraceExpectedActual("Signature validation failed", RAW_DUMP_HEADER_SIGNATURE, Context->RawDumpHeader.Signature);
        hr = E_FAIL;
        goto Exit;
    }

    //
    // Check the version.
    //
    if (Context->RawDumpHeader.Version != RAW_DUMP_HEADER_VERSION) {
        TraceExpectedActual("Version validation failed",
                        RAW_DUMP_HEADER_VERSION, Context->RawDumpHeader.Version);
        hr = E_FAIL;
        goto Exit;
    }

    //
    // Check for expected flags.
    //
    expectedBits = RAW_DUMP_HEADER_FLAGS_VALID |
        RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE;

    if ((Context->RawDumpHeader.Flags & expectedBits) == 0) {
        hr = E_FAIL;
        TraceInfo1("Invalid flags", "Flags", Context->RawDumpHeader.Flags);
        goto Exit;
    }


    //
    // Check dump size. We don't know before hand the size of the dump
    // so we'll check for a non-zero value.
    //
    if (Context->RawDumpHeader.DumpSize == 0) {
        hr = E_FAIL;
        TraceHRESULT("DumpSize is 0", hr);
        goto Exit;
    }

    //
    // There should be at least one section.
    //
    if (Context->RawDumpHeader.SectionsCount == 0) {
        hr = E_FAIL;
        TraceHRESULT("SectionsCount is 0", hr);
        goto Exit;
    }

    //
    // Header looks good and looks like we have a dump. 
    // Read the complete table.
    //
    Context->RawDumpSectionTable = (PRAW_DUMP_SECTION_HEADER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, RawDumpTableSize(Context->RawDumpHeader.SectionsCount));
    if (Context->RawDumpSectionTable == nullptr)
    {
        hr = E_FAIL;
        TraceHRESULT("Failed to allocate memory for dump table", hr);
        goto Exit;
    }

    ZeroMemory(Context->RawDumpSectionTable, RawDumpTableSize(Context->RawDumpHeader.SectionsCount));

    if ( FAILED(hr = Context->hRawFile.Read((PCHAR)Context->RawDumpSectionTable, RawDumpTableSize(Context->RawDumpHeader.SectionsCount), &bytesProcessed)) ||
       (0 == bytesProcessed)
      )
    {
        TraceHRESULT("Read RAW_Dump Section Table failed", hr);
        goto Exit;
    }

    TraceInfo("Dump header validated.");
    hr = S_OK;

#ifdef VERBOSE_MSGS
    DumpRawDumpHeader(&Context->RawDumpHeader, Context->RawDumpSectionTable);
#endif 

Exit:
    return hr;
}


NTSTATUS VerifyRawDumpSectionTable(PDMP_CONTEXT Context)
/*++

    Routine Description:

    This function validates the RAW_DUMP_HEADER.SectionTable. The following are checked.

    1. At least one DDR section.
    2. DDR sections are adjacent to each other?
    3. DDR sections are in ascending order.
    4. No sections with unrecognized section type.
    5. SectionsCount matches number of valid sections.
    6. Only the last section should have the RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE flag.

    Arguments:

        Context - Pointer to DmpContext

    Return Value:

        NT status code.

--*/
{
    UINT32                   index = 0;
    NTSTATUS                 status = STATUS_UNSUCCESSFUL;
    UINT64                   previousDDRStart = 0;
    UINT64                   previousDDREnd = 1;
    UINT64                   currentDDRStart = 0;
    UINT64                   currentDDREnd = 0;
    UINT32                   expectedFlagBits = RAW_DUMP_HEADER_FLAGS_VALID | RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE;
    UINT32                   lastDDRSectionIndex = 0;

    //
    // Initialize the various counters.
    //
    Context->DDRSectionCount = 0;
    Context->SVSectionCount = 0;
    Context->CPUContextSectionCount = 0;
    Context->InvalidSectionCount = 0;
    Context->DDRSectionFragmentationCount = 0;
    Context->MissingFlagsCount = 0;
    Context->InsufficientStorageSectionsCount = 0;
    Context->InvalidVersionCount = 0;
    Context->LargestSVSpecificSectionSize = 0;



    for (index = 0; index < Context->RawDumpHeader.SectionsCount; index++)  {
        //
        // Check version
        //
        if (Context->RawDumpSectionTable[index].Version != RAW_DUMP_SECTION_HEADER_VERSION) {
            TraceInfo2("Section has invalid version", "Index", index, "Version", Context->RawDumpSectionTable[index].Version);
            Context->InvalidVersionCount++;
        }

        //
        // Has the right flags been set?
        //
        if ((Context->RawDumpSectionTable[index].Flags & expectedFlagBits) == 0) {
            TraceInfo1("Expected flags are not set", "Flags", Context->RawDumpSectionTable[index].Flags);
            Context->MissingFlagsCount++;
        }

        //
        // Check if RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE flag is 
        // set. If so, it should only be set for the last section.
        //
        if ((Context->RawDumpSectionTable[index].Flags & RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE) ==
            RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE) {
            if (index != (Context->RawDumpHeader.SectionsCount - 1)) {
                TraceInfo1("Section has insufficient storage flag and is not last section", "Index", index);
                Context->InsufficientStorageSectionsCount++;
            } else  {
                TraceInfo1("Last section has an incomplete flag", "Index", index);
            }
        }

        //
        // Section type-specific checks.
        //
        switch (Context->RawDumpSectionTable[index].Type) {
        case RAW_DUMP_SECTION_TYPE_DDR_RANGE:
            Context->DDRMemoryMapCount++;

            //
            // Save the pointer to the first DDR section.
            // This will make it easier to access stuff 
            // in DDR sections.
            //
            if (Context->DDRSectionCount == 0) {
                Context->DDRSections = &Context->RawDumpSectionTable[index];
                Context->FirstDDRFileIdIndex = index;
            }

            //
            // Do some checking to see if 
            // all DDR sections are adjacent to 
            // each other as called for by the spec.
            //
            if ((Context->DDRSectionCount != 0) &&
                ((index - lastDDRSectionIndex) != 1)) {
                Context->DDRSectionFragmentationCount++;
                TraceInfo1("Discontiguous DDR section found", "Index", index);
            }

            //
            // Check for overlap.
            // Assume each subsequent section is ascending
            // from previous.
            //
            currentDDRStart = Context->RawDumpSectionTable[index].u.DDRInformation.Base;
            currentDDREnd = currentDDRStart + Context->RawDumpSectionTable[index].Size - 1;

            previousDDRStart = currentDDRStart;
            previousDDREnd = currentDDREnd;

            lastDDRSectionIndex = index;
            Context->DDRSectionCount++;

            break;

        case RAW_DUMP_SECTION_TYPE_CPU_CONTEXT:
            Context->CPUContextSectionCount++;
            Context->TotalCpuContextSizeInBytes += Context->RawDumpSectionTable[index].Size;
            break;

        case RAW_DUMP_SECTION_TYPE_SV_SPECIFIC:
            Context->SVSectionCount++;
            Context->TotalSVSpecificSizeInBytes += Context->RawDumpSectionTable[index].Size;

            if (Context->LargestSVSpecificSectionSize < Context->RawDumpSectionTable[index].Size) {
                Context->LargestSVSpecificSectionSize = Context->RawDumpSectionTable[index].Size;
            }

            break;

        default:
            Context->InvalidSectionCount++;
            break;
        } //switch (Context->SectionTable[index].Type)
    } //for dumpHeader->SectionsCount

    TraceInfo1("DDR Sections", "Count", Context->DDRSectionCount);
    TraceInfo1("SV Specific Sections", "Count", Context->SVSectionCount);
    TraceInfo1("CPU Context Sections", "Count", Context->CPUContextSectionCount);

    if (Context->InvalidVersionCount > 0) {
        TraceInfo1("Sections with invalid versions detected", "Count", Context->InvalidVersionCount);
        goto Exit;
    }

    if (Context->MissingFlagsCount > 0) {
        TraceInfo1("Sections with no flags set detected", "Count", Context->MissingFlagsCount);
        goto Exit;
    }

    if (Context->InsufficientStorageSectionsCount > 0)  {
        TraceInfo1("Unexpected sections with insufficient storage flag", "Count",
            Context->InsufficientStorageSectionsCount);
        goto Exit;
    }

    if (Context->InvalidSectionCount++ > 0) {
        TraceInfo1("InvalidSectionCount is not zero", "Count",
            Context->InvalidSectionCount);
        goto Exit;
    }

    TraceInfo("Section table validated");

    status = STATUS_SUCCESS;

Exit:
    return status;
}


NTSTATUS
BuildDDRMemoryMap(
    PDMP_CONTEXT Context
)
/*++

Routine Description:

This routine builds a DDRMemoryMap table.

Arguments:

Context - Pointer to DmpContext

Return Value:

NT status code.

--*/
{
    UINT32              allocationSize = 0;
    UINT64              previousEnd = 0;
    UINT64              currentStart = 0;
    NTSTATUS            status = STATUS_UNSUCCESSFUL;

    //
    // We should have aborted if the DDR assumptions were violated.
    // Let's make sure.
    //

    if ((Context->DDRSectionFragmentationCount != 0) &&
        (Context->DDRSectionsOverlapCount != 0)) {
        TraceNTSTATUS("Section table violates assumptions about DDR sections", status);
        goto Exit;
    }

    Context->TotalDDRSizeInBytes = 0;
    Context->DDRMemoryMapCount = 0;
    Context->DDRMemoryMap = nullptr;
    allocationSize = sizeof(DDR_MEMORY_MAP) * Context->DDRSectionCount;

    Context->DDRMemoryMap = (PDDR_MEMORY_MAP)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, allocationSize);

    if (Context->DDRMemoryMap == nullptr) {
        status = STATUS_NO_MEMORY;
        TraceNTSTATUS("Failed to allocate memory for DDR memory map", status);
        goto Exit;
    }

    //
    // Fill in the table.
    //
    for (UINT32 index = 0; index < Context->DDRSectionCount; index++)  {
        if (Context->DDRSections[index].Type != RAW_DUMP_SECTION_TYPE_DDR_RANGE) {
            TraceInfo2("ERROR!!! DDR section does not have correct type",
                       "Index", index, "Type", Context->DDRSections[index].Type);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        Context->DDRMemoryMapCount++;
        Context->DDRMemoryMap[index].Base = Context->DDRSections[index].u.DDRInformation.Base;
        Context->DDRMemoryMap[index].Size = Context->DDRSections[index].Size;
        Context->DDRMemoryMap[index].Offset = Context->DDRSections[index].Offset;
        Context->DDRMemoryMap[index].End = Context->DDRMemoryMap[index].Base +
            Context->DDRMemoryMap[index].Size - 
            1;
        Context->TotalDDRSizeInBytes += Context->DDRMemoryMap[index].Size;
    }

    // DDR sections layout may be out of order, by base.
    // Need to sort ascending by base address.
    LogLibInfoPrintf(L"Sorting DDR sections");
    for (UINT32 i = 0; i < Context->DDRMemoryMapCount - 1; i++)
    {
        for (UINT32 j = 0; j < Context->DDRMemoryMapCount - 1 - i; j++)
        {
            if (Context->DDRMemoryMap[j].Base > Context->DDRMemoryMap[j + 1].Base)
            {
                DDR_MEMORY_MAP tmpDdrRecord;

                memcpy(&tmpDdrRecord, &Context->DDRMemoryMap[j], sizeof(tmpDdrRecord));
                memcpy(&Context->DDRMemoryMap[j], &Context->DDRMemoryMap[j + 1], sizeof(tmpDdrRecord));
                memcpy(&Context->DDRMemoryMap[j + 1], &tmpDdrRecord, sizeof(tmpDdrRecord));
            }

        }

    }

    //
    // Check for sane DDR sections (no overlap, no negative size, no swapped boundaries)
    //
    LogLibInfoPrintf(L"Checking DDR section sanity");
    for (UINT32 index = 0; index < Context->DDRMemoryMapCount; index++)
    {
        if (Context->DDRMemoryMap[index].End < Context->DDRMemoryMap[index].Base)
        {
            LogLibInfoPrintf(L"DDR Section %u end before start", index);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        if (Context->DDRMemoryMap[index].Size == 0)
        {
            LogLibInfoPrintf(L"DDR Section %u size is zero", index);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        if (index > 0)
        {
            previousEnd = Context->DDRMemoryMap[index - 1].End;
            currentStart = Context->DDRMemoryMap[index].Base;

            //
            // Ideal layout is each new segment starts at the previous end + 1
            // Anything else is a hole (harmless) or overlap (fatal)
            //
            if (previousEnd < currentStart - 1)
            {
                Context->DDRSectionFragmentationCount++;
            }
            else if (previousEnd >= currentStart)
            {
                Context->DDRSectionsOverlapCount++;
            }
            else
            {
                Context->DDRMemoryMap[index].Contiguous = TRUE;
            }
        }
        else
        {
            // The first section is contiguous by default
            Context->DDRMemoryMap[index].Contiguous = TRUE;
        }
    }

    if (Context->DDRSectionFragmentationCount != 0)
    {
        LogLibInfoPrintf(L"Hole count %u in DDR Sections is harmless", Context->DDRSectionFragmentationCount);
    }

    if (Context->DDRSectionsOverlapCount != 0)
    {
        LogLibInfoPrintf(L"Overlap count %u in DDR Sections is fatal", Context->DDRSectionsOverlapCount);
        goto Exit;
    }

    status = STATUS_SUCCESS;
Exit:
    return status;
}


HRESULT ValidateDDRAgainstPhysicalMemoryBlock(_Inout_ PDMP_CONTEXT Context)
/*++

Routine Description:

This function validates that the memory described in the DUMP_HEADER.PhysicalMemoryBlock
is contained within the DDR sections and checks that memory size described by PhysicalMemoryBlock
is at the most, the same as DDR sections.

The following are verified.

1. Total memory size described by the number of pages is the same or less than
the amount of memory described by the DDR sections.
2. Each memory run must be contained in contiguous DDR sections.

Arguments:

Context - PDMP_CONTEXT

Return Value:

NT status code.

--*/
{
    PDDR_MEMORY_MAP               ddrMemoryMap = nullptr;
    UINT64                        endPD;
    UINT32                        indexDDR;
    UINT32                        indexPD;
    UINT64                        startPD;
    UINT32                        spanCount;
    UINT32                        memorySizeFromPD = 0;
    PPHYSICAL_MEMORY_DESCRIPTOR32 physDesc = nullptr;
    NTSTATUS                      status = STATUS_UNSUCCESSFUL;
    BOOLEAN                       startInSection;
    BOOLEAN                       endInSection;

    ddrMemoryMap = Context->DDRMemoryMap;
    physDesc = &(Context->DumpHeader32->PhysicalMemoryBlock);

    memorySizeFromPD = PAGES_TO_BYTES(physDesc->NumberOfPages);
    Context->MemoryDescriptors = physDesc;
    Context->SizeAccordingToMemoryDescriptors = memorySizeFromPD;

    if (memorySizeFromPD > Context->TotalDDRSizeInBytes) {
        TraceInfo2("Size of memory block is larger than DDR size",
                   "Memory Size", memorySizeFromPD, "DDR Size", Context->TotalDDRSizeInBytes);
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    for (indexPD = 0; indexPD < physDesc->NumberOfRuns; indexPD++) {
        startPD = (UINT64)(physDesc->Run[indexPD].BasePage * PAGE_SIZE);
        endPD = startPD + (UINT64)(physDesc->Run[indexPD].PageCount * PAGE_SIZE) - 1;
        spanCount = 0;
        startInSection = FALSE;
        endInSection = FALSE;

        TraceInfo1("Looking at descriptor", "Index", indexPD);

        for (indexDDR = 0; indexDDR < Context->DDRSectionCount; indexDDR++) {
            if ((spanCount > 0) && (!ddrMemoryMap[indexDDR].Contiguous)) {
                //
                // We got a problem. We got a run that spans 
                // DDR sections but we are hitting a DDR section 
                // which don't have memory that's contiguous with 
                // previous. 
                //
                TraceInfo("Error verifying memory run. Run is spanning discontiguous DDR sections.\n");
                TraceInfo1("Run Index", "Index", indexPD);
                TraceInfo1("DDR Index", "Index", indexDDR);
                TraceInfo1("Previous DDR Index", "Index", (indexDDR - 1));

                status = STATUS_BAD_DATA;
                goto Exit;
            }

            //
            // Is startPD in this DDR section?
            //
            if ((startPD >= ddrMemoryMap[indexDDR].Base) && (startPD <= ddrMemoryMap[indexDDR].End)) {
                startInSection = TRUE;
                //
                // Does endPD also fall into this section?
                //
                if ((endPD >= ddrMemoryMap[indexDDR].Base) && (endPD <= ddrMemoryMap[indexDDR].End)) {
                    //
                    // Range is within this section. 
                    //
                    endInSection = TRUE;
                    break;
                } else {
                    //
                    // Range is not contained in this section. 
                    // Move startPD to the end plus 1. 
                    //
                    startPD = ddrMemoryMap[indexDDR].End + 1;
                    spanCount++;
                }
            }
        } //indexDDR

        //
        // Check if the start or the end of a run fell within any DDR section.
        //
        if (!startInSection) {
            TraceInfo2("Start of run does not fall into any DDR section",
                       "Base Addr", (UINT64)(physDesc->Run[indexPD].BasePage * PAGE_SIZE), "IndexPD", indexPD);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        if (!endInSection) {
            TraceInfo2("The end of a run does not fall into any DDR section", "EndPD", endPD, "IndexPD", indexPD);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        TraceInfo1("Descriptor checked out", "Index", indexPD);
    }//indexPD

    status = STATUS_SUCCESS;

Exit:
    return HRESULT_FROM_NT(status);
}


HRESULT
InitDumpFile(
    _Inout_ PDMP_CONTEXT Context
    )
{
    LARGE_INTEGER    fileoffset;
    DWORD            flag = 0;
    HRESULT          hr = E_FAIL;
    Context->WindowsDumpHandle = INVALID_HANDLE_VALUE;

    fileoffset.QuadPart = 0;
    flag = FILE_ATTRIBUTE_NORMAL;
    flag |= FILE_FLAG_OVERLAPPED;

    Context->WindowsDumpHandle = CreateFileW(
                                    Context->WindowsDumpFilePath,
                                    GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_ALWAYS,
                                    flag,
                                    nullptr
                                    );

    if (Context->WindowsDumpHandle == INVALID_HANDLE_VALUE) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        TraceHRESULT("Failed to Create entire file", hr);
        goto Exit;
    }

    Context->SecondaryDataBlobCount = Context->CPUContextSectionCount + 
                                      Context->SVSectionCount + 
                                      1 +  //memory map 
                                      1;   //NonOS DDR
    //
    // Get the sum of storage requirements for data.
    //
    Context->ActualDumpFileUsedInBytes.QuadPart = (ULONGLONG)(Context->SizeAccordingToMemoryDescriptors + 
                                                  Context->TotalSVSpecificSizeInBytes + 
                                                  Context->TotalCpuContextSizeInBytes + 
                                                  Context->TotalNonOSDDRSizeInBytes);

    //
    // Get the sum of storage requiremetns for metadata.
    //
    if (Context->Is64Bit) {
        Context->ActualDumpFileUsedInBytes.QuadPart += sizeof(DUMP_HEADER64);
    }
    else {
        Context->ActualDumpFileUsedInBytes.QuadPart += sizeof(DUMP_HEADER32);
    }
    TraceInfo1("Size of the dump contents", "Bytes", Context->ActualDumpFileUsedInBytes.QuadPart);

    hr = S_OK;

Exit:
    return hr;
}


HRESULT WriteDumpHeader(_Inout_ PDMP_CONTEXT Context)
/*++

Routine Description:

This function writes DUMP_HEADER to dump file.

Arguments:

Context - Pointer to the global context structure.

Return Value:

NT status code.

--*/
{
    UINT32                  dataSize = 0;
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    IO_STATUS_BLOCK         statusBlock;

    Context->WindowsDumpFileOffset.QuadPart = 0;

    //
    // Update the DUMP_HEADER
    //
    TraceInfo("Updating DUMP_HEADER32 before it is written to dump");
    Context->DumpHeader32->RequiredDumpSpace  = Context->ActualDumpFileUsedInBytes;
    Context->DumpHeader32->BugCheckCode       = Context->BugCheckCode;
    Context->DumpHeader32->BugCheckParameter1 = (ULONG)Context->BugCheckParam1;
    Context->DumpHeader32->BugCheckParameter2 = (ULONG)Context->BugCheckParam2;
    Context->DumpHeader32->BugCheckParameter3 = (ULONG)Context->BugCheckParam3;
    Context->DumpHeader32->BugCheckParameter4 = (ULONG)Context->BugCheckParam4;

    if ((Context->SVSectionCount > 0) || (Context->CPUContextSectionCount > 0)) {
        Context->DumpHeader32->SecondaryDataState = STATUS_SUCCESS;
    }

    // Clear the comment field since it was overloaded with the dump instance id.
    RtlZeroMemory(&Context->DumpHeader32->Comment, DMP_HEADER_COMMENT_SIZE);

    //
    // Write the header to dump.
    //
    dataSize = sizeof(DUMP_HEADER32);

    status = NtWriteFile(
                 Context->WindowsDumpHandle,
                 nullptr,
                 nullptr,
                 nullptr,
                 &statusBlock,
                 Context->DumpHeader32,
                 dataSize,
                 &Context->WindowsDumpFileOffset,
                 nullptr
                 );

    if (FAILED(status)) {
        TraceNTSTATUS("Error:  Failed to write DumpHeader" , status);
        goto Exit;
    }

    NtFlushBuffersFile(Context->WindowsDumpHandle, &statusBlock);
    Context->WindowsDumpFileOffset.QuadPart += dataSize;

    //
    // Save the starting offset for DDR for easier future access.
    //
    Context->DDRFileOffset = Context->WindowsDumpFileOffset;

    status = STATUS_SUCCESS;

Exit:
    return HRESULT_FROM_NT(status);
}


HRESULT WriteDDR(_Inout_ PDMP_CONTEXT Context)
/*++

    Routine Description:

    This function writes DDR sections based on the DUMP_HEADER's physical memory descriptor
    to dump file.

    Arguments:

        Context - Pointer to the global context structure.

    Return Value:

        NT status code.

--*/
{
    LARGE_INTEGER                   basePA;
    ULONG                           PageRemain = 0;
    LARGE_INTEGER                   bytesWritten = { 0 };
    UINT32                          io = 0;
    UINT32                          index = 0;
    UINT32                          ioCountPerRun = 0;
    LARGE_INTEGER                   runSize;
    LARGE_INTEGER                   startPA;
    ULONG                           ioSize = 0;
    ULONG                           buffersize = DEFAULT_DMP_BUF_SZ;
    NTSTATUS                        status = STATUS_UNSUCCESSFUL;
    PVOID                           tempBuffer = nullptr;
    IO_STATUS_BLOCK                 statusBlock;


    //
    // Allocate the intermediate buffer to read memory from DDR section
    // to the dump file.
    //
    tempBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffersize);
    if (tempBuffer == nullptr) {
        TraceNTSTATUS("Unable to allocate 0x%x bytes buffer for writing DDR memory to dump.\n", PAGE_SIZE);
        status = STATUS_NO_MEMORY;
        goto Exit;
    }

    ZeroMemory(tempBuffer, buffersize);

    for (index = 0; index < Context->DumpHeader32->PhysicalMemoryBlock.NumberOfRuns; index++)  {

        basePA.QuadPart = (Context->DumpHeader32->PhysicalMemoryBlock.Run[index].BasePage * PAGE_SIZE);
        PageRemain = Context->DumpHeader32->PhysicalMemoryBlock.Run[index].PageCount;
        runSize.QuadPart = (Context->DumpHeader32->PhysicalMemoryBlock.Run[index].PageCount *PAGE_SIZE);
        ioCountPerRun = (UINT32)(runSize.QuadPart / buffersize);

        if ((runSize.QuadPart % buffersize) > 0) {
            ioCountPerRun++;
        }

        TraceInfo1("IOs required for this run", "Count", ioCountPerRun);

        for (io = 0; io < ioCountPerRun; io++) {
            startPA.QuadPart = basePA.QuadPart + io * ioSize;

            ioSize = ((PageRemain * PAGE_SIZE) < buffersize) ? (PageRemain * PAGE_SIZE) : buffersize;

            status = ReadFromDDRSectionByPhysicalAddress(
                         Context,
                         startPA,
                         ioSize,
                         tempBuffer
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to read from DDR sections", status);
                goto Exit;
            }

            status = NtWriteFile(
                         Context->WindowsDumpHandle,
                         nullptr,
                         nullptr,
                         nullptr,
                         &statusBlock,
                         tempBuffer,
                         ioSize,
                         &Context->WindowsDumpFileOffset,
                         nullptr
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("NtWriteFile failed", status);
                goto Exit;

            }
            NtFlushBuffersFile(Context->WindowsDumpHandle, &statusBlock);
            bytesWritten.QuadPart += ioSize;
            Context->WindowsDumpFileOffset.QuadPart += ioSize;
            PageRemain -= ioSize / PAGE_SIZE;
            TraceInfo2("Memory Run", "Index", index, "Pages remaining", PageRemain);
        }// for io

#ifdef VERBOSE
        TraceInfo1("**** Finished writing run 0x%x. ****\n", index);
#endif
    }//for index

    if ((bytesWritten.QuadPart / PAGE_SIZE) != Context->DumpHeader32->PhysicalMemoryBlock.NumberOfPages) {
        TraceExpectedActual("Pages written not",
            Context->DumpHeader32->PhysicalMemoryBlock.NumberOfPages,
            bytesWritten.QuadPart / PAGE_SIZE);
        goto Exit;
    }

    status = STATUS_SUCCESS;

Exit:

    if (tempBuffer != nullptr) {
        HeapFree(GetProcessHeap(), NULL, tempBuffer);
        tempBuffer = nullptr;
    }

    return HRESULT_FROM_NT(status);
}

NTSTATUS
ReadFromDDRSectionByPhysicalAddress(
    _In_ PDMP_CONTEXT Context,
    _In_ LARGE_INTEGER PhysicalAddress,
    _In_ UINT32 Length,
    _Out_ PVOID Buffer
    )
/*++

Routine Description:

This function reads the contents of memory in DDR sections.

Arguments:

Context - Dmp_CONTEXT

PhysicalAddress - Physical address of memory in DDR sections which we want
to read from.

Length - Number of bytes to read.

Buffer - Buffer holding the contents of the read.

Return Value:

NT status code.

--*/
{
    UINT64                      addressStart;
    UINT64                      addressEnd;
    UINT32                      bytesToRead = 0;
    UINT32                      bytesRemain;
    UINT32                      ddrSectionsCount = 0;
    LARGE_INTEGER               offset;
    PDDR_MEMORY_MAP             ddrMap = nullptr;
    UINT32                      index = 0;
    UINT64                      sectionStart;
    UINT64                      sectionEnd;
    UINT32                      sectionSpanCount = 0;
    NTSTATUS                    status = STATUS_UNSUCCESSFUL;
    PVOID                       temp;

    ddrSectionsCount = Context->DDRMemoryMapCount;
    ddrMap = Context->DDRMemoryMap;
    addressStart = PhysicalAddress.QuadPart;
    addressEnd = addressStart + Length - 1;
    bytesRemain = Length;
    offset.QuadPart = 0;
    temp = Buffer;

    //
    // Determine the right section.
    //
    for (index = 0; index < ddrSectionsCount; index++) {
        sectionStart = ddrMap[index].Base;
        sectionEnd = ddrMap[index].End;

        if ((sectionSpanCount != 0) &&
            (bytesRemain != 0) &&
            (!ddrMap[index].Contiguous)) {
            TraceInfo2("Attempting to read spanning discontiguous sections",
                       "Section", index, "BytesRemain", bytesRemain);
            status = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        //
        // Determine if address falls in to this section.
        //
        if ((sectionStart <= addressStart) && (sectionEnd >= addressStart)) {
            size_t bytesProcessed = 0;
            //TraceInfo1("Memory is contained in section 0x%x\n", index);

            //
            // Figure out how many bytes to read.
            // Need to figure this out because we may 
            // cross section boundaries.
            //
            bytesToRead = (sectionEnd >= addressEnd) ?
                (UINT32)(addressEnd - addressStart + 1) :
                (UINT32)(sectionEnd - addressStart + 1);

            offset.QuadPart = Context->fileOffset.QuadPart + (addressStart - sectionStart) + ddrMap[index].Offset;

#ifdef VERBOSE
            TraceInfo2("Reading 0x%x bytes at offset 0x%I64x\n", bytesToRead, offset.QuadPart);
#endif
            status = STATUS_UNSUCCESSFUL;
            if (FAILED(Context->hRawFile.SetPos(offset)))
            {
#ifdef VERBOSE
                TraceInfo("Failed to set position for disk read ", "Result");
#endif
                goto Exit;
            }
            else if (FAILED(Context->hRawFile.Read((PCHAR)temp, bytesToRead, &bytesProcessed)))
            {
#ifdef VERBOSE
                TraceInfo("Failed to read disk", "Result");
#endif
                goto Exit;
            }
            else if (bytesToRead != bytesProcessed)
            {
#ifdef VERBOSE
                TraceInfo("Failed to read correct size from disk", "Result");
#endif
                goto Exit;
            }
            else
            {
                status = STATUS_SUCCESS;
            }


            //
            // Update bytesRemain.
            //
            bytesRemain = bytesRemain - bytesToRead;

            // TraceInfo1("0x%x bytes remain to be read.\n", bytesRemain);

            if (bytesRemain != 0) {
                //
                // Time to move to next section.
                // Update temp.
                //
                Buffer = (PVOID)((UINT32)(Buffer)+bytesToRead);

                //
                // Update addressStart.
                // AddressEnd should not be updated. 
                //
                addressStart = addressStart + bytesToRead;
                sectionSpanCount++;
            } else {
                //
                // No more bytes to read. Time to leave.
                //
                status = STATUS_SUCCESS;
                break;
            }
        }//(sectionStart <= addressStart)
    }//for (index = 0; index < ddrSectionsCount; index++)

    //
    // We end up here either because bytesRemain is zero or 
    // we went through all the DDR sections and still couldn't 
    // read all the bytes. 
    //
    if ((index == ddrSectionsCount) && (bytesRemain != 0)) {
        TraceInfo2("Read incomplete", "Remaining bytes", bytesRemain, "Total bytes", Length);
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

Exit:

    return status;
}

NTSTATUS
GetKdDebuggerBlockFromInMemAddresses(
_Inout_ PDMP_CONTEXT Context,
_In_ PKDDEBUGGER_DATA64 pdbgDataBlock,
_Inout_ PLARGE_INTEGER pdbgDataHeaderPA
)
/*++

Routine Description:

This function attempts to get the physical memory address of the KDDebugger Block
from the In Memory Datastructures.

Arguments:

Context - PDMP_CONTEXT

Return Value:

NT status code.

--*/
{

    NTSTATUS      status;
    LARGE_INTEGER OriginalBlockPAOffset;
    status = STATUS_UNSUCCESSFUL;

    //
    // KDDebugger Physical Address may be present in DUMP_HEADER+KdebuugerBlock location. 
    //
    OriginalBlockPAOffset.QuadPart = Context->DumpHeaderPA.QuadPart + PAGE_SIZE + sizeof(KDDEBUGGER_DATA64);
    TraceInfo1("KdDebuggerDataBlock should be at ", "PA", OriginalBlockPAOffset.QuadPart);

    //
    // Read the Physical address where you would get the 
    //
    status = ReadFromDDRSectionByPhysicalAddress(Context, 
                OriginalBlockPAOffset,
                sizeof(LARGE_INTEGER),
                pdbgDataHeaderPA);
    if (!NT_SUCCESS(status)) {
        TraceNTSTATUS("Read Decoded Debug Block failed", GetLastError());
        goto Exit;
    }
 
    //
    // Read the Physical address where you would get original (not the decoded)
    // KdDebuggerData Block should be present.
    //
    status = ReadFromDDRSectionByPhysicalAddress(Context, 
                *pdbgDataHeaderPA, 
                sizeof(KDDEBUGGER_DATA64), 
                pdbgDataBlock);
    if (!NT_SUCCESS(status)) {
        TraceNTSTATUS(L"Read Decoded Debug Block PA failed. Status:0x%x\r\n", status);
        goto Exit;
    }

    if (pdbgDataBlock->Header.Size > MAX_KDDEBUGGER_BLOCK_SIZE || !ValidateKdDebuggerDataBlock(&(pdbgDataBlock->Header))) {
        //
        // The debugger block is successfully read but it seems to be encoded.
        //
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }
    Context->GetKdBlockPAFromImMemData = TRUE;

Exit:
    return status;
}


NTSTATUS
GetKdDBlockViaMemoryTranslation(
_Inout_ PDMP_CONTEXT        Context,
_In_    ULONG64             dbgDataHeaderVA,
_In_    PKDDEBUGGER_DATA64  pdbgDataBlock,
_Inout_ PLARGE_INTEGER      pdbgDataHeaderPA
)
{
    NTSTATUS      status;
    status = STATUS_UNSUCCESSFUL;

    status = ReadFromDDRSectionByVirtualAddress(Context,
        dbgDataHeaderVA,
        sizeof(KDDEBUGGER_DATA64),
        pdbgDataBlock,
        pdbgDataHeaderPA);

    if (!NT_SUCCESS(status)) {
        TraceNTSTATUS("Failed to read the address for CONTEXT.", status);
        goto Exit;
    }

    //
    // Check the size.
    //
    TraceInfo1("DBGKD_DEBUG_DATA_HEADER64.Size", "Bytes", pdbgDataBlock->Header.Size);

    if (pdbgDataBlock->Header.Size > MAX_KDDEBUGGER_BLOCK_SIZE || !ValidateKdDebuggerDataBlock(&(pdbgDataBlock->Header))) {
        //
        // The debugger block is successfully read but it seems to be encoded.
        //
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

Exit:
    return status;
}

NTSTATUS
GetKdDebuggerDataBlock(
    _Inout_ PDMP_CONTEXT Context
    )
/*++

    Routine Description:

    This function attempts to determine if KdDebuggerDataBlock is encoded or not by
    performing the following checks.

    1. Read the value pointed to by Context->OfflineDumpKDBEP
    2. Or check the DBGKD_DEBUG_DATA_HEADER64.Size value.

    Arguments:

        Context - PDMP_CONTEXT

    Return Value:

        NT status code.

--*/
{
    UINT32             dataSize;
    PKDDEBUGGER_DATA64 dbgDataBlock = nullptr;
    LARGE_INTEGER      dbgDataHeaderPA;
    LARGE_INTEGER      decodedBlockPA;
    ULONG64            dbgDataHeaderVA = (Context->Is64Bit ? Context->DumpHeader64->KdDebuggerDataBlock : Context->DumpHeader32->KdDebuggerDataBlock);

    NTSTATUS           status = STATUS_UNSUCCESSFUL;

    TraceInfo("Checking DBGKD_DEBUG_DATA_HEADER.Size");
    Context->KdDebuggerDataBlockPA.QuadPart = ADDRESS_NOT_PRESENT;

    //
    // Read DBGKD_DEBUG_DATA_HEADER
    // 
    dataSize = sizeof(KDDEBUGGER_DATA64);
    dbgDataBlock = (PKDDEBUGGER_DATA64)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dataSize);
    if (dbgDataBlock == nullptr) {
        goto Exit;
    }

    //
    // Atempt reading the data block via physical address 
    //

    status = GetKdDBlockViaMemoryTranslation(Context, dbgDataHeaderVA, dbgDataBlock, &dbgDataHeaderPA);
    if (!NT_SUCCESS(status)){
        TraceInfo("The KdDebuggerDataBlock appears to be encoded.");

        //
        // As ARM platform DumperHeader size is PageSize so 
        // directly use PAGE_SIZE
        //
        decodedBlockPA.QuadPart = Context->DumpHeaderPA.QuadPart + PAGE_SIZE;
        TraceInfo1("Decoded version of KdDebuggerDataBlock should be at ", "PA", decodedBlockPA.QuadPart);

        //directly read to buffer
        status = ReadFromDDRSectionByPhysicalAddress(Context,
            decodedBlockPA,
            dataSize,
            dbgDataBlock);
        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Read Decoded Debug Block failed", GetLastError());
            goto Exit;
        }
    }
    else {
        TraceInfo("Read KdDebuggerDataBlock via memory translation.");
        TraceInfo1("DBGKD_DEBUG_DATA_HEADER64.", "Size", dbgDataBlock->Header.Size);
        TraceInfo("KdDebuggerDataBlock appears to be valid and decoded. Read it in to memory.");
    }
 
    Context->KdDebuggerDataBlock = dbgDataBlock;
    Context->KdDebuggerDataBlockPA.QuadPart = dbgDataHeaderPA.QuadPart;
    if (!ValidateKdDebuggerDataBlock(&(Context->KdDebuggerDataBlock->Header))) {
        status = STATUS_UNSUCCESSFUL;
        TraceInfo("The KdDebuggerDataBlock is not valid.");
        goto Exit;
    }

    TraceInfo("KdDebuggerDataBlock - updating bugcheck parameters");
    if (0 != Context->KdDebuggerDataBlock->KiBugcheckData)
    { // Update the KdDebuggerDataBlock with the Bugcheck data

        UINT64                      dbgBugcheckVA = (UINT64)Context->KdDebuggerDataBlock->KiBugcheckData;
        UINT                        dbgBugcheckDataBlock[DBG_BUGCHECK_SIZE];
        LARGE_INTEGER               dbgBugcheckPA = { 0 };

        status = ReadFromDDRSectionByVirtualAddress(
            Context,
            dbgBugcheckVA,
            DBG_BUGCHECK_SIZE,
            dbgBugcheckDataBlock,
            &dbgBugcheckPA);
        if (!NT_SUCCESS(status))
        {
            TraceNTSTATUS("Read KiBugcheckData failed", status);
            goto Exit;
        }

        dbgBugcheckDataBlock[BUGCHECK_CODE_IDX] = (UINT)Context->BugCheckCode;
        dbgBugcheckDataBlock[BUGCHECK_PARAM1_IDX] = (UINT)Context->BugCheckParam1;
        dbgBugcheckDataBlock[BUGCHECK_PARAM2_IDX] = (UINT)Context->BugCheckParam2;
        dbgBugcheckDataBlock[BUGCHECK_PARAM3_IDX] = (UINT)Context->BugCheckParam3;
        dbgBugcheckDataBlock[BUGCHECK_PARAM4_IDX] = (UINT)Context->BugCheckParam4;

        status = WriteToDumpByPhysicalAddress(
            Context,
            dbgBugcheckPA,
            DBG_BUGCHECK_SIZE,
            dbgBugcheckDataBlock);
        if (!NT_SUCCESS(status))
        {
            TraceNTSTATUS("Failed write the KiBugcheckData to dump", status);
            goto Exit;
        }
        else
        {
            TraceInfo("KiBugcheckData - Written successfully to dump");
        }

    }

    status = WriteToDumpByPhysicalAddress(Context,
        Context->KdDebuggerDataBlockPA,
        dataSize,
        Context->KdDebuggerDataBlock);

    if (!NT_SUCCESS(status)) {
        TraceNTSTATUS("Failed write the decoded copy of KdDebuggerData to dump.", status);
        goto Exit;
    }

    status = STATUS_SUCCESS;
    TraceInfo2("KdDebuggerDataBlock Writen to into memory", 
        "Address", Context->KdDebuggerDataBlockPA.QuadPart, 
        "Size", dataSize);
Exit:
    return status;
}

NTSTATUS
ReadFromDDRSectionByVirtualAddress(
    _In_ PDMP_CONTEXT Context,
    _In_ UINT64 VirtualAddress,
    _In_ UINT64 Length,
    _Out_ PVOID Buffer,
    _Out_opt_ PLARGE_INTEGER PhysicalAddress
)
{
    NTSTATUS status;
    ULONG64 PA64;
    HRESULT hr;
    UNREFERENCED_PARAMETER(Context);

    IDebugDataSpaces2 *DebugDataSpaces = DbgClient::GetDataSpaces();
    if (DebugDataSpaces == nullptr) {
        LogLibInfoPrintf(L"Failure in dbgeng, KdDebuggerDataBlock may be unreadable");
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    hr = DebugDataSpaces->ReadVirtual(VirtualAddress, Buffer, (ULONG)Length, NULL);
    if (FAILED(hr)){
        TraceHRESULT("Failed to find physical address Error %x", hr);
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    hr = DebugDataSpaces->VirtualToPhysical(VirtualAddress, &PA64);
    if (FAILED(hr)) {
        TraceHRESULT("Failed to find physical address Error %x", hr);
        status = STATUS_UNSUCCESSFUL;
    }
    else{
        status = STATUS_SUCCESS;
        if (PhysicalAddress != nullptr)
            PhysicalAddress->QuadPart = PA64;
    }

 Exit:
    return status;
}


NTSTATUS
ReadFromDDRSectionByVirtualAddress32(
_In_ PDMP_CONTEXT Context,
_In_ UINT32 VirtualAddress,
_In_ UINT32 Length,
_Out_ PVOID Buffer,
_Out_opt_ PLARGE_INTEGER physicalAddress
)
/*++

Routine Description:

This function reads the contents of memory in DDR sections based on a virtual address.

Arguments:

Context - DMP_CONTEXT

VirtualAddress - Virtual address of memory in DDR sections which we want
to read from.

Length - Number of bytes to read.

Buffer - Buffer holding the contents of the read.

Return Value:

NT status code.

--*/
{
    UINT32              endVA;
    UINT32              nextPageVA;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // Validate if we are crossing page boundary. If we are
    // return STATUS_INVALID_PARAMETER. 
    //
    nextPageVA = (VirtualAddress & 0xFFFFF000) + 0x1000;
    endVA = VirtualAddress + Length - 1;

    if (endVA >= nextPageVA) {
        //
        // This address could be in a Large Page (4MB)
        //
        nextPageVA = (VirtualAddress & 0xFF000000) + 0x1000000;
        endVA = VirtualAddress + Length - 1;

        if (endVA >= nextPageVA) {
            status = STATUS_INVALID_PARAMETER;
            TraceNTSTATUS("The read crosses page boundary", status);
            goto Exit;
        }
    }

    //
    // Get the physical address.
    //
    status = VirtualToPhysical(
                Context,
                VirtualAddress, 
                physicalAddress);
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to convert virtual address to physical address", status);
        goto Exit;
    }

    //
    // Read this from DDR
    //
    status = ReadFromDDRSectionByPhysicalAddress(
                 Context,
                 *physicalAddress,
                 Length,
                 Buffer
                 );
    if (FAILED(status)) {
        TraceInfo1("Failed to read from physical address", "PA", physicalAddress->QuadPart);
        goto Exit;
    }
 
    TraceInfo3("Read At", "Bytes", Length, "VA", VirtualAddress, "PA", physicalAddress->QuadPart);
    status = STATUS_SUCCESS;

Exit:
    return status;
}


NTSTATUS
ReadFromDDRSectionByVirtualAddress64(
_In_ PDMP_CONTEXT Context,
_In_ UINT64 VirtualAddress,
_In_ UINT64 Length,
_Out_ PVOID Buffer,
_Out_opt_ PLARGE_INTEGER physicalAddress
)
/*++

Routine Description:

This function reads the contents of memory in DDR sections based on a virtual address.

Arguments:

Context - DMP_CONTEXT

VirtualAddress - Virtual address of memory in DDR sections which we want
to read from.

Length - Number of bytes to read.

Buffer - Buffer holding the contents of the read.

PhysicalAddress - Optional. Returns the physical address.

Return Value:

NT status code.

--*/
{
    UINT64              endVA;
    UINT64              nextPageVA;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    //
    // Validate if we are crossing page boundary. If we are
    // return STATUS_INVALID_PARAMETER. 
    //
    nextPageVA = (VirtualAddress & 0xFFFFF000) + 0x1000;
    endVA = VirtualAddress + Length - 1;

    if (endVA >= nextPageVA) {
        //
        // This address could be in a Large Page (4MB)
        //
        nextPageVA = (VirtualAddress & 0xFF000000) + 0x1000000;
        endVA = VirtualAddress + Length - 1;

        if (endVA >= nextPageVA) {
            status = STATUS_INVALID_PARAMETER;
            TraceNTSTATUS("The read crosses page boundary", status);
            goto Exit;
        }
    }

    //
    // Get the physical address.
    //
    status = VirtualToPhysical64(Context,
                VirtualAddress,
                physicalAddress);

    if (!NT_SUCCESS(status)) {
        TraceNTSTATUS("Failed to convert virtual address to physical address", status);
        goto Exit;
    }

    //
    // Read this from DDR
    //
    status = ReadFromDDRSectionByPhysicalAddress(Context,
                    *physicalAddress,
                    (UINT32)Length,
                    Buffer);

    if (!NT_SUCCESS(status)) {
          TraceInfo1("Failed to read from physical address", "PA", physicalAddress->QuadPart);
        goto Exit;
    }

    TraceInfo3("Read At", "Bytes", Length, "VA", VirtualAddress, "PA", physicalAddress->QuadPart);
    status = STATUS_SUCCESS;

Exit:
    return status;
}

NTSTATUS
UpdateContextX86(
    _Inout_ PDMP_CONTEXT Context
    )
/*++

    Routine Description:

    This function updates CONTEXT with X86 CPU context dump.

    The location of CONTEXT for a processor is stored at PRCB + OffsetPrcbContext.
    To get to the above, we need to do the following:

    1. Get the physical address of KdDebuggerDataBlock->KiProcessorBlock.
        KiProcessorBlock is an array of pointers to the PRCB for each processor.
    2. Calculate the offset to the PRCB address and read it.
    3. Get the physical address of the PRCB
    4. Calculate the physical address of PRCB + OffsetPrcbContext and read the value
        which is a pointer to the CONTEXT.
    5. Get the physical address of CONTEXT
    6. Read CONTEXT.

    Arguments:

        Context - Pointer to PDMP_CONTEXT

    Return Value:

        NT status code.

--*/
{
    PX86_CONTEXT        x86Context = nullptr;
    UINT32              contextVA = 0;
    UINT32              dataSize = 0;
    UINT32              indexProcessor = 0;
    PKDDEBUGGER_DATA64  kdDebuggerDataBlock;
    UINT32              kiProcessorBlockVA = 0;
    UINT32              offsetPrcbContext = 0;
    UINT32              prcbAddressVA = 0;
    UINT32              processorCount = 0;
    NTSTATUS            status = STATUS_UNSUCCESSFUL;
    UINT32              tempVA = 0;
    LARGE_INTEGER       curoffset = Context->fileOffset;
    IO_STATUS_BLOCK     statusBlock;
    LARGE_INTEGER       physicalAddress;
    LARGE_INTEGER       ContextPAOffset;
    PLARGE_INTEGER      ContextPA = nullptr;
    HRESULT             hr;

    //
    // Check prereqs...
    //
    TraceInfo("Checking prerequisites");

    if (Context->KdDebuggerDataBlock == nullptr) {
        status = STATUS_UNSUCCESSFUL;
        TraceNTSTATUS("KdDebuggerDataBlock not available", status);
        goto Exit;
    }

    status = GetX86CPUContext(Context);

    //
    // Allocate the space for context. 
    //
    dataSize = sizeof(X86_CONTEXT);
    x86Context = (PX86_CONTEXT)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dataSize);

    if (x86Context == nullptr) {
        status = STATUS_NO_MEMORY;
        TraceNTSTATUS("Failed to allocate memory for CONTEXT", status);
        goto Exit;
    }

    ZeroMemory(x86Context, dataSize);
    //
    // Get the addresses of various variables we will need.
    //
    kdDebuggerDataBlock = Context->KdDebuggerDataBlock;
    kiProcessorBlockVA = (UINT32)(kdDebuggerDataBlock->KiProcessorBlock);
    offsetPrcbContext = (UINT32)(kdDebuggerDataBlock->OffsetPrcbContext);
    processorCount = Context->DumpHeader32->NumberProcessors;

    TraceInfo3("Context", "KiProcessorBlock", kiProcessorBlockVA,
               "OffsetPrcbContext", offsetPrcbContext, "ProcessorCount", processorCount);

    //
    // The Physical addresses of the Contexts are avaialable at location DUMPP_HEADER + KDDEBUGGERBLOCK 
    // + 8 bytes (for KDBlockAddress) 
    //
    ContextPAOffset.QuadPart = Context->DumpHeaderAddress.QuadPart +
        PAGE_SIZE +
        sizeof(KDDEBUGGER_DATA64) +
        sizeof(LARGE_INTEGER);

    ContextPA = (PLARGE_INTEGER)HeapAlloc(GetProcessHeap(), 
                                        HEAP_ZERO_MEMORY, 
                                        sizeof(LARGE_INTEGER)*processorCount);

    status = ReadFromDDRSectionByPhysicalAddress(Context,
        ContextPAOffset,
        (processorCount * sizeof(LARGE_INTEGER)),
        ContextPA);
    if (!NT_SUCCESS(status)) {
        TraceNTSTATUS("Read Decoded Debug Block failed", GetLastError());
        goto Exit;
    }

    //
    // Update the CONTEXT for each processor.
    //
    for (indexProcessor = 0; indexProcessor < processorCount; indexProcessor++) {
        if (Context->GetKdBlockPAFromImMemData == TRUE) {

            //
            // Read the CONTEXT
            //
            RtlZeroMemory(x86Context, dataSize);

            // Read the Physical address where you would get the 
            status = ReadFromDDRSectionByPhysicalAddress(Context,
                ContextPA[indexProcessor],
                sizeof(X86_CONTEXT),
                x86Context);

            if (!NT_SUCCESS(status)) {
                TraceNTSTATUS("Failed to read the address for CONTEXT.", status);
                goto Exit;
            }

            Context->X86ContextPA = ContextPA[indexProcessor];
        }
        else {
            size_t bytesProcessed = 0;
            //
            // Calculate the virtual address of the location containing the PRCB virtual address.
            //
            tempVA = kiProcessorBlockVA + (sizeof(UINT32)* indexProcessor);

            TraceInfo2("PRCB", "CPU", indexProcessor, "VA", tempVA);

            status = ReadFromDDRSectionByVirtualAddress(Context,
                tempVA,
                sizeof(prcbAddressVA),
                &prcbAddressVA,
                &physicalAddress);
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to read the address for PRCB", status);
                goto Exit;
            }

            if (prcbAddressVA == 0) {
                status = STATUS_UNSUCCESSFUL;
                TraceNTSTATUS("PRCB VA is 0! Abort processing", status);
                goto Exit;
            }
            //
            // Calculate the address of CONTEXT and read it.
            //
            tempVA = prcbAddressVA + offsetPrcbContext;

            TraceInfo2("Context", "CPU", indexProcessor, "Address", tempVA);

            status = ReadFromDDRSectionByVirtualAddress(Context,
                tempVA,
                sizeof(contextVA),
                &contextVA,
                &physicalAddress);
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to read the address for CONTEXT", status);
                goto Exit;
            }

            if (contextVA == 0) {
                status = STATUS_UNSUCCESSFUL;
                TraceNTSTATUS("Context VA is 0! Abort processing", status);
                goto Exit;
            }

            status = ReadFromDDRSectionByVirtualAddress(
                Context,
                contextVA,
                dataSize,
                x86Context,
                &Context->X86ContextPA
                );
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to read the address for CONTEXT", status);
                goto Exit;
            }

            TraceInfo2("X86Context", "CPU", indexProcessor, "PA", Context->X86ContextPA.QuadPart);

            RtlZeroMemory(x86Context, dataSize);
            curoffset.QuadPart = Context->fileOffset.QuadPart + Context->CPUContextAddress.QuadPart + indexProcessor*dataSize;
            if ( FAILED(hr = Context->hRawFile.SetPos(curoffset))
                 || FAILED(hr = Context->hRawFile.Read((PCHAR)x86Context, dataSize, &bytesProcessed))
                 || (0 == bytesProcessed)
               )
            {
                TraceHRESULT("Failed to read x86CONTEXT", hr);
                goto Exit;
            }

        }

        //
        // Write the CONTEXT to dump file.
        //
        status = WriteToDumpByPhysicalAddress(
                     Context,
                     Context->X86ContextPA,
                     dataSize,
                     x86Context
                     );
        if (FAILED(status)) {
            TraceNTSTATUS("Failed to update dump's CONTEXT", status);
            goto Exit;
        }

        TraceInfo1("Wrote updated CONTEXT to dump", "CPU", indexProcessor);
    } //for indexProcessor

    status = NtFlushBuffersFile(Context->WindowsDumpHandle, &statusBlock);
    if (NT_SUCCESS(status) && (status != STATUS_PENDING))  {
        status = statusBlock.Status;
    }

Exit:

    if (x86Context != nullptr) {
        HeapFree(GetProcessHeap(), NULL, x86Context);
    }

    if (ContextPA != nullptr) {
        HeapFree(GetProcessHeap(), NULL, ContextPA);
    }
    return status;
}

// JFP - ARM64
typedef struct _GenericPhysicalDecriptor_ 
{
    ULONG   NumberOfRuns;
    ULONG64 NumberOfPages;
    PVOID   pRun;
} PHYS_DESCRIPTOR, *pPHYS_DESCRIPTOR;


NTSTATUS
WriteToDumpByPhysicalAddress(
    _Inout_ PDMP_CONTEXT Context,
    _In_ LARGE_INTEGER PhysicalAddress,
    _In_ UINT32 Size,
    _Inout_ PVOID Buffer
    )
/*++

    Routine Description:

    This function reads and writes to the dedicated dump file based on physical address.

    Important assumption is that the physical memory descriptors are not contiguous and that
    all pages should be contained in one run.

    Arguments:

    Context - Pointer to PDMP_CONTEXT

    IsWrite - Boolean. TRUE for write. FALSE for read.

    PhysicalAddress - The base physical address of memory to be read from or written to in the
                        dedicated dump file.

    Size - Size of buffer in bytes to write.

    Buffer - Pointer to the buffer.

    Return Value:

        NT status code.

--*/
{
    UNREFERENCED_PARAMETER(Buffer);

    BOOLEAN                         foundDescriptor = FALSE;
    LARGE_INTEGER                   endPA;
    LARGE_INTEGER                   endPD;
    UINT32                          indexPD = 0;
    LARGE_INTEGER                   ioOffset;
    LARGE_INTEGER                   lengthPD;
    LARGE_INTEGER                   offset;
    PHYS_DESCRIPTOR                 physDesc = { 0 };
    LARGE_INTEGER                   startPA;
    LARGE_INTEGER                   startPD;
    NTSTATUS                        status = STATUS_UNSUCCESSFUL;
    IO_STATUS_BLOCK                 statusBlock;

    endPA.QuadPart = 0;
    endPD.QuadPart = 0;
    startPA.QuadPart = 0;
    startPD.QuadPart = 0;
    ioOffset.QuadPart = 0;
    offset.QuadPart = 0;
    lengthPD.QuadPart = 0;

    physDesc.NumberOfRuns = (Context->Is64Bit ? Context->DumpHeader64->PhysicalMemoryBlock.NumberOfRuns : Context->DumpHeader32->PhysicalMemoryBlock.NumberOfRuns);
    physDesc.NumberOfPages = (Context->Is64Bit ? Context->DumpHeader64->PhysicalMemoryBlock.NumberOfPages : Context->DumpHeader32->PhysicalMemoryBlock.NumberOfPages);
    physDesc.pRun = (Context->Is64Bit ? (PVOID)&Context->DumpHeader64->PhysicalMemoryBlock.Run[0] : (PVOID)&Context->DumpHeader32->PhysicalMemoryBlock.Run[0]);

    startPA.QuadPart = PhysicalAddress.QuadPart;
    endPA.QuadPart = startPA.QuadPart + Size - 1;

    TraceInfo2("Looking up physical memory descriptor", "Start PA", startPA.QuadPart, "End PA", endPA.QuadPart);

    //
    // Look for the descriptor containing the memory
    //
    for (indexPD = 0; indexPD < physDesc.NumberOfRuns; indexPD++) {
        if (Context->Is64Bit)
        {
            PPHYSICAL_MEMORY_RUN64 tmp = (PPHYSICAL_MEMORY_RUN64)physDesc.pRun;
            startPD.QuadPart = tmp[indexPD].BasePage * PAGE_SIZE;
            lengthPD.QuadPart = tmp[indexPD].PageCount * PAGE_SIZE;
        }
        else
        {
            PPHYSICAL_MEMORY_RUN32 tmp = (PPHYSICAL_MEMORY_RUN32)physDesc.pRun;
            startPD.QuadPart = tmp[indexPD].BasePage * PAGE_SIZE;
            lengthPD.QuadPart = tmp[indexPD].PageCount * PAGE_SIZE;
        }

        endPD.QuadPart = startPD.QuadPart + lengthPD.QuadPart - 1;

        if ((startPA.QuadPart >= startPD.QuadPart) &&
            (endPD.QuadPart >= endPA.QuadPart)) {
            foundDescriptor = TRUE;
#ifdef VERBOSE_MSGS
            wprintf(L"Descriptor %u contains the memory. Base: 0x%I64x End:0x%I64x Offset: 0x%I64x\n",
                indexPD,
                startPD.QuadPart,
                endPD.QuadPart,
                offset.QuadPart);
#endif
            break;
        }

        offset.QuadPart += lengthPD.QuadPart;
    }

    if (!foundDescriptor) {
        status = STATUS_INVALID_PARAMETER;
        TraceNTSTATUS("Failed to locate physical memory descriptor", status);
        goto Exit;
    }

    //
    // Now that we got the memory descriptor, calculate the offset and
    // perform the IO.
    //
    ioOffset.QuadPart = (startPA.QuadPart - startPD.QuadPart) + offset.QuadPart + Context->DDRFileOffset.QuadPart;

#ifdef VERBOSE_MSGS
    wprintf(L"IO offset is: 0x%I64x\n", ioOffset.QuadPart);
#endif

    status = NtWriteFile(
                 Context->WindowsDumpHandle,
                 nullptr,
                 nullptr,
                 nullptr,
                 &statusBlock,
                 Buffer,
                 Size,
                 &ioOffset,
                 nullptr
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to write to dump file", status);
        goto Exit;
    }

    NtFlushBuffersFile(Context->WindowsDumpHandle, &statusBlock);
    status = STATUS_SUCCESS;

#ifdef VERBOSE_MSGS
    wprintf(L"Context: 0x%I64x\n", (Context->Is64Bit ? (PVOID)&(Context->DumpHeader64->PhysicalMemoryBlock) : (PVOID)&(Context->DumpHeader32->PhysicalMemoryBlock)));
#endif

Exit:
    return status;
}


NTSTATUS
GetX86CPUContext(
_Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function writes CPU Context Specific sections based on the DUMP_HEADER's physical memory descriptor
to dump file.

Arguments:

Context - Pointer to the global context structure.

Return Value:

NT status code.

--*/
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    for (UINT32 idx = 0; idx < Context->RawDumpHeader.SectionsCount; idx++)
    {
        if (Context->RawDumpSectionTable[idx].Type == RAW_DUMP_SECTION_TYPE_CPU_CONTEXT)
        {
            Context->CPUContextAddress.QuadPart = Context->RawDumpSectionTable[idx].Offset;
            Context->TotalCpuContextSizeInBytes = Context->RawDumpSectionTable[idx].Size;
            status = STATUS_SUCCESS;
        }

    }

    return status;
}


BOOL
ValidateKdDebuggerDataBlock(
_In_ PDBGKD_DEBUG_DATA_HEADER64 Header
)
/*++

Routine Description:

This function will consider that there's a valid data block
if the following conditions are met:
1. Header.Size is sizeof(KDDEBUGGER_DATA64)
2. Header.OwnerTag is 'GBDK'

Arguments:

Header - Pointer to the KdDebuggerDataBlock.Header.

Return Value:

TRUE - Datablock is valid.
--*/
{
    if (Header->Size != sizeof(KDDEBUGGER_DATA64))  {
        TraceExpectedActual("Header size does not equal sizeof KDDEBUGGER_DATA64",
            sizeof(KDDEBUGGER_DATA64),
            Header->Size);
        return FALSE;
    }

    if (Header->OwnerTag != KDBG_TAG) {
        TraceExpectedActual("OwnerTag does not match expected",
            KDBG_TAG,
            Header->OwnerTag);
        return FALSE;
    }

    TraceInfo("KdDebuggerDataBlock.Header checks out");
    return TRUE;
}

VOID
DumpGUID(
_In_ GUID*  Guid
)
{
    UNREFERENCED_PARAMETER(Guid);

    LogLibInfoPrintf(L"\t\t{%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}\n",
        Guid->Data1, Guid->Data2, Guid->Data3,
        Guid->Data4[0], Guid->Data4[1], Guid->Data4[2],
        Guid->Data4[3], Guid->Data4[4], Guid->Data4[5],
        Guid->Data4[6], Guid->Data4[7]);
}

NTSTATUS
VirtualToPhysical(
    _In_ PDMP_CONTEXT Context,
    _In_ UINT32 VirtualAddress,
    _Out_ PLARGE_INTEGER PhysicalAddress
    )
/*++

Routine Description:

This function determines the physical address given a virtual address.
This only works for 4K pages.

Arguments:

Context - DMP_CONTEXT

VirtualAddress - The virtual address of some memory in the DDR sections.

PhysicalAddress - Physical address of memory in DDR sections which we want
to read from.

Return Value:

NT status code.

--*/
{
    NTSTATUS            status = STATUS_UNSUCCESSFUL;
    UINT32              directoryTableBase = 0;
    UINT32              pdeAddress;
    LARGE_INTEGER       temp;
    UINT32              pdi;
    UINT32              PPE;
    UINT32              pde;
    UINT32              pdeoffset = 0;
    UINT32              pageTableAddress;
    UINT32              pteAddress;
    UINT32              pte;
    UINT32              pteoffset = 0;
    UCHAR               PaeEnabled;
    PAE_ADDRESS         virtualaddinPAE;
 #if defined (_ARM_)
    PHARDWARE_PTE       PointerPte;
#endif

    PhysicalAddress->QuadPart = (ULONGLONG)-1;

    //
    // ARM_VALID_PFN_MASK           0xFFFFF000
    // X86_VALID_PFN_MASK           0xFFFFF000
    directoryTableBase = ((Context->Is64Bit) ? (UINT32)(Context->DumpHeader64->DirectoryTableBase) : (UINT32)(Context->DumpHeader32->DirectoryTableBase)) & ARM_VALID_PFN_MASK;
    if (directoryTableBase == 0) {
        TraceInfo("Directory table base is NULL.\n");
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    PaeEnabled = (Context->Is64Bit) ? FALSE : Context->DumpHeader32->PaeEnabled;
    if (PaeEnabled != TRUE) {
        TraceInfo2("PAE Disabled: ", "VirtualAddress",  VirtualAddress, "directory table base", directoryTableBase);
        pdeoffset = VirtualAddress;
        pdeoffset = (pdeoffset >> 22)*sizeof (UINT32);
        pdeAddress = directoryTableBase + pdeoffset;
        temp.QuadPart = pdeAddress;
        status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT32), &pde);

        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Failed to read PDE. Status.", status);
            goto Exit;
        }

        TraceInfo2("Virtual to Phyical:", "Physical PDE Address", pdeAddress, "offset", pdeoffset); 
        TraceInfo2("Virtual to Phyical:", "PDE Content",  pde, "VirtualAddress",  VirtualAddress);

#if defined (_ARM_)
        // 
        // This file is compiled for both ARM32 and ARM64. PHARDWARE_PTE doesnt have LargePages 
        // as its element for ARM64. ARM64 page walk is handled by VirtualToPhysical64.
        // Checking PDE attributes.
        //
        PointerPte = (PHARDWARE_PTE) &pde;
        if(PointerPte->LargePage) {
            // 
            // we have a 4 MB page. Dont go to any more levels from here. Least 22 bits will be the offset.
            //
            PhysicalAddress->QuadPart = (UINT32)(pde & ARM_PDE_MASK) + (UINT32)(VirtualAddress & ARM_LARGE_PAGE_ADDR_OFFSET_MASK);
            TraceInfo3("LAGE PAGE: ", "Decoded Physical Address",  PhysicalAddress->QuadPart, "PDE",  pde, "Virtual address",  VirtualAddress);
            status = STATUS_SUCCESS;
            goto Exit;
        }

#endif

        pageTableAddress = pde & ARM_VALID_PFN_MASK;
        pteoffset = VirtualAddress;
        pteoffset = ((pteoffset & 0x003FF000) >> 12) * sizeof(UINT32);
        pteAddress = pageTableAddress + pteoffset;
        temp.QuadPart = pteAddress;
        status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT32), &pte);

        TraceInfo2("Virtual to Phyical:", "Physical PTE Address", pteAddress, "offset", pteoffset); 
        TraceInfo2("Virtual to Phyical:", "PDE Content",  pte, "VirtualAddress",  pageTableAddress);

        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Failed to read PTE. Status", status);
            goto Exit;
        }

        if (pte == 0) {
            TraceInfo("Error: PTE is zero");
            status = STATUS_UNSUCCESSFUL;
            goto Exit;
        }

        PhysicalAddress->QuadPart = (UINT32)(pte & 0xFFFFF000) + (UINT32)(VirtualAddress & 0x00000FFF);
        TraceInfo3("Virtual to Phyical:", "Decoded Physical Address", PhysicalAddress->QuadPart, "PTE", pte, "Virtual address", VirtualAddress);
        status = STATUS_SUCCESS;
    }
    else {
        TraceInfo(L"PAE enabled system, we will use PDI->PDE->PTE->physic address decoding");

        //
        // Page Directory Index (PDI)->Page Directory Entries (PDE) ->PTE->Physic address
        //
        //  highest 2bit are PDI, 00,01,10,11 are valid
        //
        //
        virtualaddinPAE.DwordPart = VirtualAddress;
        pdi = virtualaddinPAE.DirectoryPointer;

        pageTableAddress = (directoryTableBase & 0xffffffe0) + pdi*sizeof(ULONGLONG);
        temp.QuadPart = pageTableAddress;
        status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT32), &PPE);
        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Failed to read PPE. Status", status);
            goto Exit;
        }

        TraceInfo2("Virtual to Physical", "VirtualAddress", VirtualAddress, "PPE", PPE);

        pdeoffset = virtualaddinPAE.Directory;
        pdeAddress = (PPE & 0xFFFFF000) + pdeoffset*sizeof(ULONGLONG);
        temp.QuadPart = pdeAddress;
        status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT32), &pde);
        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Failed to read PDE. Status", status);
            goto Exit;
        }

        TraceInfo2("Virtual to Phyical:", "Physical PDE Address", pdeAddress, "offset", pdeoffset); 
        TraceInfo2("Virtual to Phyical:", "PDE Content",  pde, "VirtualAddress",  VirtualAddress);

        pteoffset = virtualaddinPAE.Table;
        pteAddress = (pde & 0xFFFFF000) + pteoffset*sizeof(ULONGLONG);
        temp.QuadPart = pteAddress;
        status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT32), &pte);

        TraceInfo3("Virtual to Phyical:", "Physical PTE Address", pteAddress, "offset", pteoffset, "PTE Content", pte);
        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS(L"Failed to read PTE.", status);
            goto Exit;
        }

        PhysicalAddress->QuadPart = (UINT32)(pte & 0xFFFFF000) + (UINT32)(VirtualAddress & 0x00000FFF);
        TraceInfo3("Virtual to Physical", "Decoded Physical Address", PhysicalAddress->QuadPart, "PTE", pte, "Virtual address", VirtualAddress);
        status = STATUS_SUCCESS;
    }

Exit:
    return status;
}


NTSTATUS
VirtualToPhysical64(
_In_ PDMP_CONTEXT Context,
_In_ UINT64 VirtualAddress,
_Out_ PLARGE_INTEGER PhysicalAddress
)
/*++

Routine Description:

This function determines the physical address given a virtual address.
This only works for 4K pages.

Arguments:

Context - DMP_CONTEXT

VirtualAddress - The virtual address of some memory in the DDR sections.

PhysicalAddress - Physical address of memory in DDR sections which we want
to read from.

Return Value:

NT status code.

--*/
{ 
    NTSTATUS      status = STATUS_UNSUCCESSFUL;


    UINT64        directoryTableBase = 0;
    LARGE_INTEGER temp;
    UINT64        pxe        = 0;
    UINT64        pxeAddress = 0;
    UINT64        ppe        = 0;
    UINT64        ppeAddress = 0;
    UINT64        pde        = 0;
    UINT64        pdeAddress = 0;
    UINT64        pte        = 0;
    UINT64        pteAddress = 0;
    UINT64        Addr;

    PhysicalAddress->QuadPart = (ULONGLONG)-1;

    //
    // Get the Directory Table Base Address
    //
    directoryTableBase = (UINT64)(Context->DumpHeader64->DirectoryTableBase & ARM64_VALID_PFN_MASK);
    if (directoryTableBase == 0) {
        TraceInfo("Directory table base is NULL.");
        status = STATUS_BAD_DATA;
        goto Exit;
    }
 

    //
    // Currently only part of the VA is actually used
    // and the rest must be in canonical form (MSB-extended)
    // so kick out any non-canonical addresses.
    //

    Addr = VirtualAddress >> ARM64_USED_VA_BITS;
    if (((VirtualAddress & (1UI64 << (ARM64_USED_VA_BITS - 1))) != 0 &&
         Addr != ARM64_UNUSED_VA_MASK) ||
        ((VirtualAddress & (1UI64 << (ARM64_USED_VA_BITS - 1))) == 0 &&
         Addr != 0))
    {
        LogLibInfoPrintf(L"ARM64VtoP: Non-canonical address\n");
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    // Read the Page Map Level 4 entry.
    LogLibInfoPrintf(L"64Bit system, we will use PXE-> PPE->PDE->PTE->physic address decoding");

    //
    // PXE(9bit)-->PPE(9bit)-->PDE(9bit)-->PTE(9bit)-->PHYoffset(12bit)
    //

    //
    //  Read PXE
    //
    pxeAddress = (((VirtualAddress >> ARM64_PML4E_SHIFT) & ARM64_PML4E_MASK) * sizeof(UINT64)) + directoryTableBase;
    temp.QuadPart = pxeAddress;
    status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT64), &pxe);
    if (!NT_SUCCESS(status)) {
        LogLibInfoPrintf(L"Failed to read PPE. Status: 0x%llx\r\n", status);
        goto Exit;
    }
    LogLibInfoPrintf(L"VirtualAddress: 0x%llx PXEAddress: 0x%llx PXE:0x%llx\r\n", VirtualAddress, pxeAddress, pxe);

    ppeAddress = (pxe & ARM64_VALID_PFN_MASK) +  (((VirtualAddress >>ARM64_PDPE_SHIFT) & ARM64_PDPE_MASK) * sizeof(UINT64)) ;
    temp.QuadPart = ppeAddress;
    status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT64), &ppe);
    if (!NT_SUCCESS(status)) {
        LogLibInfoPrintf(L"Failed to read PPE. Status: 0x%llx\r\n", status);
        goto Exit;
    }
    LogLibInfoPrintf(L"VirtualAddress: 0x%llx PPEAddress :0x%llx PPE:0x%llx\r\n", VirtualAddress, ppeAddress, ppe);
     if(!((PARM64_HARDWARE_PTE)&ppe)->NotLargePage) {
        LogLibInfoPrintf(L"1GB Page!!\n\r");
        PhysicalAddress->QuadPart = (ppe & ARM64_1GB_PPE_PAGE_MASK) + (VirtualAddress & ARM64_1GB_PPE_ADDR_OFFSET_MASK);
        LogLibInfoPrintf(L"Decoded Physical Address: 0x%llx    PPE: 0x%x   Virtual address: 0x%llx\r\n", PhysicalAddress->QuadPart, ppe, VirtualAddress);
        goto Exit;
    }

    pdeAddress = (ppe & ARM64_VALID_PFN_MASK) + ((VirtualAddress >> ARM64_PDE_SHIFT) & ARM64_PDE_MASK)*sizeof(UINT64);
    temp.QuadPart = pdeAddress;
    status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT64), &pde);
    if (!NT_SUCCESS(status)) {
        LogLibInfoPrintf(L"Failed to read PDE. Status: 0x%llx\r\n", status);
        goto Exit;
    }
    LogLibInfoPrintf(L"VirtualAddress:0x%llx  PDEAdress:0x%llx PDE:0x%llx\r\n", VirtualAddress, pdeAddress, pde);
    if(!((PARM64_HARDWARE_PTE)&pde)->NotLargePage) {
        LogLibInfoPrintf(L"Large Page!!\n\r");
        PhysicalAddress->QuadPart = (pde & ARM64_LARGE_PAGE_PDE_MASK) + (VirtualAddress & ARM64_LARGE_PAGE_ADDR_OFFSET_MASK);
        LogLibInfoPrintf(L"Decoded Physical Address: 0x%llx    PDE: 0x%x   Virtual address: 0x%llx\r\n", PhysicalAddress->QuadPart, pde, VirtualAddress);
        goto Exit;
    }

    pteAddress = (pde & ARM64_VALID_PFN_MASK) + (((VirtualAddress >> ARM64_PTE_SHIFT) & ARM64_PTE_MASK) * sizeof(UINT64)) ;
    temp.QuadPart = pteAddress;
    status = ReadFromDDRSectionByPhysicalAddress(Context, temp, sizeof(UINT64), &pte);

    LogLibInfoPrintf(L"VirtualAddress:0x%llx  PTEAdress:0x%llx PTE:0x%llx  \r\n", VirtualAddress, pteAddress, pte);
    if (!NT_SUCCESS(status)) {
        LogLibInfoPrintf(L"Failed to read PTE. Status: 0x%llx\r\n", status);
        goto Exit;
    }

    PhysicalAddress->QuadPart = (pte & ARM64_VALID_PFN_MASK) + (VirtualAddress & ARM64_VALID_PFN_MASK);
    LogLibInfoPrintf(L"Virtual address= 0x%llx  decoded physical address= 0x%llx \r\n", VirtualAddress, PhysicalAddress->QuadPart);
    status = STATUS_SUCCESS;
Exit:

    // @@END_NOTPUBLICCODE

    return status;
}


HRESULT GetDumpHeader(_Inout_ PDMP_CONTEXT Context)
/*++

    Routine Description:

    This function searches DDR sections for the in-memory dump data built by
    nt!IopInitializeInMemoryDumpData. This is accomplished by searching for the
    InMemoryDumpHeaderMagicString at page intervals. DUMP_HEADER.ValidDump values
    determines if the dump is 32 bit or a 64 bit format.

    The following items are checked.
    1. DUMP_HEADER.Signature
    2. DUMP_HEADER.ValidDump
    3. DUMP_HEADER.BugCheckCode
    4. DUMP_HEADER.DumpType
    5. DUMP_HEADER.RequiredDumpSpace
    6. Instance ID matches.

    Arguments:

        Context - DMP_CONTEXT

    Return Value:

        HRESULT

--*/
{
    HRESULT         hr = HRESULT_FROM_NT(STATUS_NOT_FOUND);
    ULONG           dataSize = 0;
    UINT32          bytesToRead = 0;
    UINT32          bytesRemain = 0;
    UINT32          ddrSectionsCount = 0;
    UINT32          indexDDR = 0;
    UINT32          indexBuffered = 0;
    UINT32          indexPage = 0;
    UINT32          ioBufferSize = IO_BUFFER_SIZE;
    PVOID           ioBuffer = nullptr;
    UINT32          iterationsPerDDR = 0;
    PDDR_MEMORY_MAP ddrMemoryMap = nullptr;
    LARGE_INTEGER   offset;
    UINT64          pagesCount = 0;
    UINT32          remainder = 0;
    UINT32          stringSize;
    PVOID           temp = nullptr;
    PULARGE_INTEGER dumpInstance = nullptr;
    DUMP_HEADER32   dumpHeader32;
    DUMP_HEADER64   dumpHeader64;

    BOOL IsHeaderValid = FALSE;

    TraceInfo("Searching for dump data built by the kernel");

    stringSize = sizeof(InMemoryDumpHeaderMagicString);

    ddrMemoryMap = Context->DDRMemoryMap;
    ddrSectionsCount = Context->DDRSectionCount;
    ioBuffer = Context->IoBuffer;
    RtlZeroMemory(ioBuffer, IO_BUFFER_SIZE);
    dataSize = sizeof(DUMP_HEADER);
    Context->Is64Bit = FALSE;
    //
    // Allocate a buffer to store the dump header. 
    //


    Context->DumpHeaderStatus = DHS_NOT_FOUND;

    // 
    // Raw dump xml already contains the dump header instance id. We will go ahead and find 
    // the dump header and validate the instance id of the header with the one read from the 
    // the info file.
    //

    for (indexDDR = 0; indexDDR < ddrSectionsCount; indexDDR++) {
        //
        // To reduce costly disk IO. We will read a large chunk 
        // of memory at a time rather than a page at a time.
        //
        iterationsPerDDR = (UINT32)(ddrMemoryMap[indexDDR].Size / ioBufferSize);
        remainder = ddrMemoryMap[indexDDR].Size % ioBufferSize;

        if (remainder > 0) {
            iterationsPerDDR++;
        }

        bytesRemain = (UINT32)ddrMemoryMap[indexDDR].Size;
        offset.QuadPart = Context->fileOffset.QuadPart + ddrMemoryMap[indexDDR].Offset;
        bytesToRead = 0;

        //sectionIndex = Context->fileOffset.QuadPart + Context->FirstDDRFileIdIndex + indexDDR;

        TraceInfo2("DDR section requires iterations", "Index", indexDDR, "Iterations", iterationsPerDDR);

        for (indexBuffered = 0; indexBuffered < iterationsPerDDR; indexBuffered++) {
            size_t bytesProcessed = 0;

            offset.QuadPart = offset.QuadPart + bytesToRead;
            bytesRemain = bytesRemain - bytesToRead;
            bytesToRead = (ioBufferSize < bytesRemain) ? ioBufferSize : bytesRemain;

            if (FAILED(hr = Context->hRawFile.SetPos(offset)))
            {
                TraceHRESULT("Failed to set position to DDR section ", hr);
                goto Exit;
            }
            else if (FAILED(hr = Context->hRawFile.Read((PCHAR)ioBuffer, bytesToRead, &bytesProcessed)))
            {
                TraceHRESULT("Failed to read DDR section from device", hr);
                goto Exit;
            }
            else if (bytesToRead != bytesProcessed)
            {
                TraceHRESULT("Failed to read correct size of DDR section from device", hr);
                goto Exit;
            }

            TraceInfo2("Processing DDR Section %d iteration", "indexDDR", indexDDR, "indexBuffered", indexBuffered);

            //
            // Search the buffer for the signature.
            //
            pagesCount = bytesToRead / PAGE_SIZE;
            remainder = bytesToRead % PAGE_SIZE;

            if ((remainder > 0) && (remainder >= stringSize)) {
                pagesCount++;
            }

            for (indexPage = 0; indexPage < pagesCount; indexPage++) {
                temp = Add2Ptr(ioBuffer, (indexPage * PAGE_SIZE));

                if (RtlEqualMemory(InMemoryDumpHeaderMagicString, temp, stringSize)) {
                    Context->DumpHeaderOffset = offset.QuadPart + (UINT64)((PUCHAR)temp - (PUCHAR)ioBuffer);

                    //
                    // Convert the offset back to a physical address.
                    //
                    Context->DumpHeaderPA.QuadPart = Context->DumpHeaderOffset -
                        ddrMemoryMap[indexDDR].Offset +
                        START_OF_DUMP_HEADER_SIG_IN_MAGIC_STRING +
                        ddrMemoryMap[indexDDR].Base;

                    TraceInfo2("Found a possible match", "Offset", Context->DumpHeaderOffset,
                        "PA", Context->DumpHeaderPA.QuadPart);

                    if (FAILED(hr = HRESULT_FROM_NT(ReadFromDDRSectionByPhysicalAddress( Context, Context->DumpHeaderPA, dataSize, &dumpHeader32))))
                    {
                        TraceHRESULT("ReadFromDDRSectionByPhysicalAddress failed", hr);
                        goto Exit;
                    }

                    //
                    // Check signature. Signatures for 64 bit and 32 bit are the same.
                    //
                    if (dumpHeader32.Signature != DUMP_SIGNATURE32) {
                        TraceExpectedActual("Invalid DUMP_HEADER.Signature",
                                            DUMP_SIGNATURE32, Context->DumpHeader32->Signature);

                        Context->DumpHeaderStatus = DHS_INVALID;
                        continue;
                    }

                    //
                    // Check valid dump value. This where we would see a difference between 32 bit and 64 bit header.
                    //
                    if (dumpHeader32.ValidDump == DUMP_VALID_DUMP64) {
                        Context->Is64Bit = TRUE;

                        //
                        // This dump has come from a 64 bit machine, we need to read the dumpheader 
                        // again. This time to a DUMP_HEADER64 structure.
                        //
                        dataSize = sizeof(DUMP_HEADER64);
                        if (FAILED(hr = HRESULT_FROM_NT(ReadFromDDRSectionByPhysicalAddress(Context, Context->DumpHeaderPA, dataSize, &dumpHeader64))))
                        {
                            TraceHRESULT("ReadFromDDRSectionByPhysicalAddress failed", hr);
                            goto Exit;
                        }

                        //
                        // Check bugcheck code.
                        //
                        if (dumpHeader64.BugCheckCode != FATAL_ABNORMAL_RESET_ERROR) {
                            TraceExpectedActual("Invalid DUMP_HEADER.BugCheckCode",
                                                FATAL_ABNORMAL_RESET_ERROR, dumpHeader64.BugCheckCode);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }

                        //
                        // Check dump type
                        //
                        if (dumpHeader64.DumpType != DUMP_TYPE_FULL) {
                            TraceExpectedActual("Invalid DUMP_HEADER.DumpType",
                                (ULONG)DUMP_TYPE_FULL, (ULONG)dumpHeader64.DumpType);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }

                        //
                        // Check RequiredDumpSpace
                        //
                        if (dumpHeader64.RequiredDumpSpace.LowPart != DUMP_SIGNATURE) {
                            TraceExpectedActual("Invalid DUMP_HEADER.RequiredDumpSpace.LowPart",
                                                DUMP_SIGNATURE, dumpHeader64.RequiredDumpSpace.LowPart);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }

                        TraceInfo1("Expected dump instance", "Instance", Context->DumpInstance.QuadPart);

                        dumpInstance = (PULARGE_INTEGER)dumpHeader64.Comment;

                        if (Context->DumpInstance.QuadPart != dumpInstance->QuadPart) {
                            TraceExpectedActual("Instance ID does not match",
                                                Context->DumpInstance.QuadPart,
                                                dumpInstance->QuadPart);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }

                    }
                    else if (dumpHeader32.ValidDump == DUMP_VALID_DUMP32) {

                        //
                        // This dump has come from a 64 bit machine.
                        // Check bugcheck code.
                        //
                        if (dumpHeader32.BugCheckCode != FATAL_ABNORMAL_RESET_ERROR) {
                            TraceExpectedActual("Invalid DUMP_HEADER.BugCheckCode",
                                                FATAL_ABNORMAL_RESET_ERROR, dumpHeader32.BugCheckCode);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }

                        //
                        // Check dump type
                        //
                        if (dumpHeader32.DumpType != DUMP_TYPE_FULL) {
                            TraceExpectedActual("Invalid DUMP_HEADER.DumpType",
                                (ULONG)DUMP_TYPE_FULL, (ULONG)dumpHeader32.DumpType);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }

                        //
                        // Check RequiredDumpSpace
                        //
                        if (dumpHeader32.RequiredDumpSpace.LowPart != DUMP_SIGNATURE) {
                            TraceExpectedActual("Invalid DUMP_HEADER.RequiredDumpSpace.LowPart",
                                                DUMP_SIGNATURE, dumpHeader32.RequiredDumpSpace.LowPart);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }

                        TraceInfo1("Expected dump instance", "Instance", Context->DumpInstance.QuadPart);

                        dumpInstance = (PULARGE_INTEGER)dumpHeader32.Comment;

                        if (Context->DumpInstance.QuadPart != dumpInstance->QuadPart) {
                            TraceExpectedActual("Instance ID does not match",
                                                Context->DumpInstance.QuadPart,
                                                dumpInstance->QuadPart);

                            Context->DumpHeaderStatus = DHS_INVALID;
                            continue;
                        }
                    }
                    else {
                        //
                        // Invalid dump header. 
                        //
                        TraceExpectedActual("Invalid DUMP_HEADER.ValidDump",
                                            DUMP_VALID_DUMP32, Context->DumpHeader32->ValidDump);

                        Context->DumpHeaderStatus = DHS_INVALID;
                        continue;
                    }

                    TraceInfo1("Dump header verified", "PA", Context->DumpHeaderPA.QuadPart);

                    hr = HRESULT_FROM_NT(STATUS_SUCCESS);
                    IsHeaderValid = TRUE;
                    Context->DumpHeaderStatus = DHS_VALID;
                    goto AllocateDumpHeader;
                }
            } //for indexPage
        }//indexBuffered
    }//for indexDDR

    if (IsHeaderValid == FALSE) {
        TraceInfo("Failed to find a valid DUMP_HEADER");
        goto Exit;
    }

AllocateDumpHeader:
    //
    // Allocate memory to Context->DumpHeaderxx and copy the contents.
    //
    if (Context->Is64Bit) {
        Context->DumpHeader64 = (PDUMP_HEADER64)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DUMP_HEADER64));
        if (Context->DumpHeader64 == nullptr) {
            hr = HRESULT_FROM_NT(STATUS_NO_MEMORY);
            TraceHRESULT("Failed to allocate memory for DUMP_HEADER", hr);
            goto Exit;
        }
        memcpy(Context->DumpHeader64, &dumpHeader64, sizeof(DUMP_HEADER64));
    }
    else {
        Context->DumpHeader32 = (PDUMP_HEADER32)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DUMP_HEADER32));
        if (Context->DumpHeader32 == nullptr) {
            hr = HRESULT_FROM_NT(STATUS_NO_MEMORY);
            TraceHRESULT("Failed to allocate memory for DUMP_HEADER", hr);
            goto Exit;
        }
        memcpy(Context->DumpHeader32, &dumpHeader32, sizeof(DUMP_HEADER32));
    }

Exit:
    return hr;

}


NTSTATUS BuildCompleteMemoryMap(_Inout_ PDMP_CONTEXT Context)
/*++

    Routine Description:

    This function builds a memory map consisting of nonOS
    and OS memory ranges in DDR sections.

    Arguments:

        Context - PDMP_CONTEXT

    Return Value:

        NT status code.

--*/
{
    PDDR_MEMORY_MAP               completeMemoryMap = nullptr;
    UINT32                        completeMemoryMapAllocSize = 0;
    UINT32                        currentComplete = 0;
    UINT32                        currentDDR = 0;
    UINT32                        currentPhysDesc = 0;
    UINT64                        cursor = 0;
    UINT64                        startDDR = 0;
    UINT64                        start = 0;
    UINT64                        endDDR = 0;
    UINT64                        end = 0;
    UINT32                        maxComplete = 0;
    UINT32                        maxDDR = 0;
    UINT32                        maxPhysDesc = 0;
    PVOID                         physDesc = nullptr;
    PDDR_MEMORY_MAP               ddrMemoryMap = nullptr;
    NTSTATUS                      status = STATUS_SUCCESS;
    UINT64                        tempBase = 0;
    UINT64                        tempEnd = 0;
    UINT64                        tempLength = 0;
    MEMORY_TYPE                   type;


    TraceInfo("BEGIN: build complete memory map");

    Context->CompleteMemoryMap = nullptr;
    Context->CompleteMemoryMapCount = 0;
    Context->TotalNonOSDDRSizeInBytes = 0;

    ddrMemoryMap = Context->DDRMemoryMap;
    maxDDR = Context->DDRMemoryMapCount;
    physDesc = ((Context->Is64Bit) ? (PVOID)Context->MemoryDescriptors64 : (PVOID)Context->MemoryDescriptors);
    maxPhysDesc = ((Context->Is64Bit) ? ((PPHYSICAL_MEMORY_DESCRIPTOR64)physDesc)->NumberOfRuns : ((PPHYSICAL_MEMORY_DESCRIPTOR32)physDesc)->NumberOfRuns);

    //
    // Figure out how many DDR memory map descriptors to allocate.
    // For now, just quadruple the count of physical memory descriptors.
    //
    maxComplete = maxPhysDesc * 4;
    completeMemoryMapAllocSize = maxComplete * sizeof(DDR_MEMORY_MAP);

    completeMemoryMap = (PDDR_MEMORY_MAP)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, completeMemoryMapAllocSize);
    if (completeMemoryMap == nullptr) {
        status = STATUS_NO_MEMORY;
        TraceNTSTATUS("Failed to allocate memory for non-OS memory map", status);
        goto Exit;
    }

    for (currentDDR = 0; currentDDR < maxDDR; currentDDR++) {
        TraceInfo1("Processing DDR", "Index", currentDDR);

#ifdef VERBOSE_MSGS
        wprintf(L"= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =\r\n");
        wprintf(L"Current DDR Section: %d\r\n", currentDDR);
        wprintf(L"     Base: 0x%I64x (startDDR)\r\n", ddrMemoryMap[currentDDR].Base);
        wprintf(L"      End: 0x%I64x (endDDR)\r\n", ddrMemoryMap[currentDDR].End);
        wprintf(L"     Size: 0x%I64x\r\n", ddrMemoryMap[currentDDR].Size);
        wprintf(L"   Offset: 0x%I64x\r\n", completeMemoryMap[currentComplete].Offset);
        wprintf(L"     Type: %d - %s\r\n", ddrMemoryMap[currentDDR].Type,
                            (RAW_DUMP_SECTION_TYPE_RESERVED == ddrMemoryMap[currentDDR].Type) ? L"Reserved" :
                                (RAW_DUMP_SECTION_TYPE_DDR_RANGE == ddrMemoryMap[currentDDR].Type) ? L"DDR" :
                                (RAW_DUMP_SECTION_TYPE_CPU_CONTEXT == ddrMemoryMap[currentDDR].Type) ? L"CPU Context" :
                                (RAW_DUMP_SECTION_TYPE_SV_SPECIFIC == ddrMemoryMap[currentDDR].Type) ? L"SV Specific" :
                                L"Unidentified"
               );
#endif

        startDDR = ddrMemoryMap[currentDDR].Base;
        endDDR = ddrMemoryMap[currentDDR].End;

        if (currentDDR == 0)
        {
            cursor = startDDR;
        }

        while ((cursor < endDDR) &&
            (currentComplete < maxComplete) &&
            (currentPhysDesc < maxPhysDesc))
        {

#ifdef VERBOSE_MSGS
            LogLibInfoPrintf(L"In while loop. cursor: 0x%I64x currentComplete: 0x%x currentPhysDesc: 0x%x\n",
                cursor,
                currentComplete,
                currentPhysDesc);
#endif

            if (Context->Is64Bit)
            {
                tempBase   = PAGES_TO_BYTES( ((PPHYSICAL_MEMORY_DESCRIPTOR64)physDesc)->Run[currentPhysDesc].BasePage  );
                tempLength = PAGES_TO_BYTES( ((PPHYSICAL_MEMORY_DESCRIPTOR64)physDesc)->Run[currentPhysDesc].PageCount );
            }
            else
            {
                tempBase   = PAGES_TO_BYTES( ((PPHYSICAL_MEMORY_DESCRIPTOR32)physDesc)->Run[currentPhysDesc].BasePage  );
                tempLength = PAGES_TO_BYTES( ((PPHYSICAL_MEMORY_DESCRIPTOR32)physDesc)->Run[currentPhysDesc].PageCount );
            }

            tempEnd = tempBase + tempLength - 1;

            if (cursor < tempBase) {
                start = cursor;
                end = tempBase - 1;
                end = (end < endDDR) ? end : endDDR;
                //
                // ASSUMPTION: There will not be any non-OS memory beyond NON_OS_MEMORY_LIMIT.
                //
                if ((cursor < NON_OS_MEMORY_LIMIT) || 
                    (tempLength < NON_OS_SIZE_LIMIT)) 
                { 
                    type = MEMORY_NONOS;
                }
                else {
                    type = MEMORY_NA;
                }
            } else {
                start = (tempBase < cursor) ? cursor : tempBase;
                end = tempEnd;
                type = MEMORY_OS;

                if (end <= endDDR) {
                    currentPhysDesc++;
                } else {
                    end = endDDR;
                }
            }

            cursor = end + 1;
            completeMemoryMap[currentComplete].Base = start;
            completeMemoryMap[currentComplete].End = end;
            completeMemoryMap[currentComplete].Size = completeMemoryMap[currentComplete].End -
                completeMemoryMap[currentComplete].Base +
                1;
            completeMemoryMap[currentComplete].Type = type;
            completeMemoryMap[currentComplete].Offset = ddrMemoryMap[currentDDR].Offset +
                (completeMemoryMap[currentComplete].Base -
                ddrMemoryMap[currentDDR].Base);
            completeMemoryMap[currentComplete].DDRIndex = currentDDR;

#ifdef VERBOSE_MSGS
            wprintf(L"    %2d - Base: 0x%I64x  End: 0x%I64x  Size: 0x%I64x  Offset: 0x%I64x  Type: %s\r\n",
                        currentComplete,
                        completeMemoryMap[currentComplete].Base,
                        completeMemoryMap[currentComplete].End,
                        completeMemoryMap[currentComplete].Size,
                        completeMemoryMap[currentComplete].Offset,
                        (completeMemoryMap[currentComplete].Type == MEMORY_NONOS) ? L"NON-OS" :
                            (completeMemoryMap[currentComplete].Type == MEMORY_OS)    ? L"OS" :
                            (completeMemoryMap[currentComplete].Type == MEMORY_NA)    ? L"NA" :
                            L"UNIDENTIFIED"
                    );
#endif

            //
            // Do some bookeeping.
            //
            if (completeMemoryMap[currentComplete].Type == MEMORY_NONOS) {
                Context->TotalNonOSDDRSizeInBytes += completeMemoryMap[currentComplete].Size;
            }

            currentComplete++;
        } //while cursor
    }// for currentDDR

    //
    // Append a final entry.
    //
    if ( (cursor <= endDDR) && 
         !(currentPhysDesc < maxPhysDesc) && 
         (currentComplete < maxComplete) &&
         (cursor < NON_OS_MEMORY_LIMIT)
       )
    {
         TraceInfo("Appending final entry");

        completeMemoryMap[currentComplete].Base = cursor;
        completeMemoryMap[currentComplete].End = endDDR;
        completeMemoryMap[currentComplete].Size = completeMemoryMap[currentComplete].End -
            completeMemoryMap[currentComplete].Base +
            1;
        completeMemoryMap[currentComplete].Type = 
            (completeMemoryMap[currentComplete].Size < NON_OS_SIZE_LIMIT) ? MEMORY_NONOS : MEMORY_NA;
        completeMemoryMap[currentComplete].Offset = ddrMemoryMap[currentDDR - 1].Offset +
            (completeMemoryMap[currentComplete].Base -
            ddrMemoryMap[currentDDR - 1].Base);
        completeMemoryMap[currentComplete].DDRIndex = currentDDR - 1;

#ifdef VERBOSE_MSGS
        wprintf(L"= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =\r\nAppended entry:\r\n");
        wprintf(L"        %2x -- Base: 0x%016I64x  End: 0x%016I64x  Size: 0x%016I64x  Offset: 0x%016I64x  Type: %s\r\n",
                currentComplete,
                completeMemoryMap[currentComplete].Base,
                completeMemoryMap[currentComplete].End,
                completeMemoryMap[currentComplete].Size,
                completeMemoryMap[currentComplete].Offset,
                (completeMemoryMap[currentComplete].Type == MEMORY_NONOS) ? L"NON-OS" :
                (completeMemoryMap[currentComplete].Type == MEMORY_OS) ? L"OS" :
                (completeMemoryMap[currentComplete].Type == MEMORY_NA) ? L"NA" :
                L"UNIDENTIFIED"
        );
#endif

        Context->TotalNonOSDDRSizeInBytes += completeMemoryMap[currentComplete].Size;
        currentComplete++;
    }

#ifdef VERBOSE_MSGS
    wprintf(L"= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =\r\n");
    wprintf(L"Total NonOS size: 0x%I64x   Total OS size: 0x%I64x   Total DDR size: 0x%I64x\n",
            Context->TotalNonOSDDRSizeInBytes,
            Context->SizeAccordingToMemoryDescriptors,
            Context->TotalDDRSizeInBytes);
#endif

    LogLibInfoPrintf(L"Total NonOS size: 0x%I64x   Total OS size: 0x%I64x\n",
        Context->TotalNonOSDDRSizeInBytes,
        Context->SizeAccordingToMemoryDescriptors);

    //
    // Do a sanity check. TotalNonOSDDRSizeInBytes + SizeAccordingToMemoryDescriptors 
    // should not be greater than TotalDDRSizeInBytes.
    //
    if ((Context->TotalNonOSDDRSizeInBytes + Context->SizeAccordingToMemoryDescriptors) >  Context->TotalDDRSizeInBytes)
    {
        LogLibInfoPrintf(L"Sanity check failed! Total NonOSDDR size + OS memory size is greater than total DDR size.\n");
        LogLibInfoPrintf(L"NonOSDDRSize: 0x%I64x  OS Memory Size: 0x%I64x  DDR Size: 0x%I64x\n",
            Context->TotalNonOSDDRSizeInBytes,
            Context->SizeAccordingToMemoryDescriptors,
            Context->TotalDDRSizeInBytes);

#ifdef VERBOSE_MSGS
        wprintf(L"ERROR: Sanity Check failed!\r\n");
#endif

        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    Context->CompleteMemoryMap = completeMemoryMap;
    Context->CompleteMemoryMapCount = currentComplete;

    LogLibInfoPrintf(L"END: Complete memory map contains %u memory ranges.\n",
        Context->CompleteMemoryMapCount);

Exit:

    return status;
}


HRESULT WriteInMemDiagBuffer(_Inout_ PDMP_CONTEXT Context)
{
    PVOID pDiagBuff = nullptr;
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    // Read the InMemDiag buffer from the raw dump and write it to the windows dump

    if (Context->InMemDataInfo.Size != 0) {
        status = STATUS_NO_MEMORY;
        pDiagBuff = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Context->InMemDataInfo.Size);
        if (pDiagBuff) {
            status = ReadFromDDRSectionByPhysicalAddress(
                         Context,
                         Context->InMemDataInfo.DataPA,
                         Context->InMemDataInfo.Size,
                         pDiagBuff
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Read InMemDiag buffer failed" , status);
                goto Exit;
            }

            status = WriteToDumpByPhysicalAddress(
                         Context,
                         Context->InMemDataInfo.DataPA,
                         Context->InMemDataInfo.Size,
                         pDiagBuff
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Write InMemDiag buffer failed" , status);
                goto Exit;
            }
        }
    }

Exit:
    if (pDiagBuff) {
        HeapFree(GetProcessHeap(), NULL, pDiagBuff);
        pDiagBuff = nullptr;
    }
    return HRESULT_FROM_WIN32(status);
}


HRESULT WriteFakeDumpHeader(_Inout_ PDMP_CONTEXT Context)
/*++

    Routine Description:

    This function modifies the DUMP_HEADER32 for the cases where the
    best effort (fake) dump header is needed. 

    Arguments:

        Context - PDMP_CONTEXT

    Return Value:

        HRESULT

--*/
{
    NTSTATUS        status = STATUS_UNSUCCESSFUL;
    IO_STATUS_BLOCK statusBlock;

    switch (Context->DumpHeaderStatus) {
    case DHS_UNKNOWN:
    case DHS_VALID:
    default:
        return S_OK;

    case DHS_NO_SVINFO:
    case DHS_NOT_FOUND:
    case DHS_INVALID:
        break;
    }

    // Indicate a best effort dump header, aka fake
    Context->DumpHeader32->BugCheckParameter1 = 0xFFFF;

    // Fake param2 is 0 if no AP_REG, 1 if no DUMP_HEADER found or if invalid
    switch (Context->DumpHeaderStatus) {
    case DHS_NO_SVINFO:
        Context->DumpHeader32->BugCheckParameter2 = FAKE_PARAM2_NO_AP_REG; //0
        break;

    case DHS_NOT_FOUND:
    case DHS_INVALID:
        Context->DumpHeader32->BugCheckParameter2 = FAKE_PARAM2_INVALID_DUMP_HEADER; //1
        break;
    }

    // Fold the attempted bug check parameters into param3
    Context->DumpHeader32->BugCheckParameter3 = 0;
    Context->DumpHeader32->BugCheckParameter3 = Context->BugCheckParam1 & 0xFF;
    Context->DumpHeader32->BugCheckParameter3 |= ((Context->BugCheckParam2 & 0xFF) << 8);
    Context->DumpHeader32->BugCheckParameter3 |= ((Context->BugCheckParam3 & 0xFF) << 16);

    // Param4 is the virtual address of the SV specific crash reason
    Context->DumpHeader32->BugCheckParameter4 = (ULONG)Context->InMemDataInfo.DataVA;

    // Write the fake header to dump.
    ULONG dataSize = sizeof(DUMP_HEADER32);
    LARGE_INTEGER offSet = {0};

    status = NtWriteFile(
                 Context->WindowsDumpHandle,
                 nullptr,
                 nullptr,
                 nullptr,
                 &statusBlock,
                 Context->DumpHeader32,
                 dataSize,
                 &offSet,
                 nullptr
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to write fake DumpHeader", status);
        goto Exit;
    }

    NtFlushBuffersFile(Context->WindowsDumpHandle, &statusBlock);

Exit:
    return HRESULT_FROM_NT(status);
}

HRESULT UpdateContextFromEmbedDeviceInfo(_Inout_ PDMP_CONTEXT Context)
/*++

Routine Description:

This function updates the Context with the information embedded in the 
rawdump.bin file. If the rawdumpinfo.xml file will not get processed, the same 
infomation is also embedded at the end of the rawdump.bin file form the start
of a 1K buffer.

Arguments:

Context - PDMP_CONTEXT

Return Value:

HRESULT

--*/
{
    HRESULT hr;
    DEVICE_SPECIFIC_INFO DeviceSpecificInfo = { 0 };

    if (SUCCEEDED(hr = ReadDeviceSpecificInfo(&Context->hRawFile, &DeviceSpecificInfo, ((ULONGLONG)Context->RawDumpFileLength.QuadPart - DEVICE_SPECIFIC_INFO_BUFFER_LENGTH) )))
    {
        Context->DumpInstance.QuadPart = DeviceSpecificInfo.DumpHeaderInstanceID;
        switch (DeviceSpecificInfo.Type)
        {
            case DEVICE_TYPE_INTELx86:            // Keeping this for backward compatibility
            case PROCESSOR_ARCHITECTURE_INTEL:
                Context->CPUContextAddress.QuadPart = DeviceSpecificInfo.CpuContextAddress;
                break;

            case DEVICE_TYPE_QCOM32:            // Keeping this for backward compatibility
            case PROCESSOR_ARCHITECTURE_ARM:
            case PROCESSOR_ARCHITECTURE_ARM64:
                Context->APRegAddress.QuadPart = DeviceSpecificInfo.APRegPA;
                Context->InMemDataInfo.DataVA = (PVOID)(DeviceSpecificInfo.VA);
                Context->InMemDataInfo.DataPA.QuadPart = DeviceSpecificInfo.PA;
                Context->InMemDataInfo.Size = DeviceSpecificInfo.Size;
                break;

            default:
                TraceInfo("UpdateContextFromEmbedDeviceInfo: Unknown Device Id");
                break;
        }

        Context->BugCheckCode   = DeviceSpecificInfo.BugCheckCode;
        Context->BugCheckParam1 = DeviceSpecificInfo.BugCheckParam1;
        Context->BugCheckParam2 = DeviceSpecificInfo.BugCheckParam2;
        Context->BugCheckParam3 = DeviceSpecificInfo.BugCheckParam3;
        Context->BugCheckParam4 = DeviceSpecificInfo.BugCheckParam4;
    }

    return hr;
}

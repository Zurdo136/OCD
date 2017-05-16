/*++

Copyright (c) April 2013 Microsoft Corporation, All Rights Reserved

Module Name:
   Dumputil.cpp

Environment:
   User Mode

Author:
   Jinsheng Shi (jinsh)  04/10/2013
   Jose Pagan (JoPagan)  01/06/2016

--*/

#include "common.h"
#include "DbgClient.h"
#include "DiskUtil.h"
#include "DumpUtil.h"
#include "apreg64.h"
#include "KdDebuggerData.h"


BOOL CheckDebugPolicyEnabled()
/*++

Routine Description:

  This routine fetch current SecureBoot policy and compare to the production policy.
  Return true if the current policy is production, 
  else return false.

Arguments:

Return Value:

    BOOL.

--*/
{
    NTSTATUS status;
    SYSTEM_SECUREBOOT_POLICY_INFORMATION policyInfo = {0};

    status = NtQuerySystemInformation(SystemSecureBootPolicyInformation,
                                      &policyInfo,
                                      sizeof(policyInfo),
                                      NULL);

    if (NT_SUCCESS(status)) {

        //Compare current GUID with Production GUID
        GUID SecBootPolicyPublisherDebug = SECUREBOOT_DEBUG_GUID;
        if (memcmp(&SecBootPolicyPublisherDebug, &policyInfo.PolicyPublisher, sizeof(GUID)) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL
QueryAbnromalResetEvents(
    VOID
    )
/*++

Routine Description:

    Check whether abnormal reset event (Windows Kernel Power 142) logged or not

Arguments:

Return Value:

    BOOL

--*/
{
    EVT_HANDLE QueryHandle;
    EVT_HANDLE EventHandle;
    DWORD      Returned;
    BOOL       FoundEvent;

    FoundEvent = FALSE;

    //
    // Obtain a query handle to the system event log
    //

    QueryHandle = EvtQuery(
                    NULL, 
                    L"System", 
                    L"*[System[EventID=142][Provider[@Name=\"Microsoft-Windows-Kernel-Power\"]]]",
                    EvtQueryChannelPath | EvtQueryForwardDirection);

    if (QueryHandle != NULL) {

        //
        // Get the next watchdog timer event
        //

        while (EvtNext(
                QueryHandle,
                1,
                &EventHandle,
                (DWORD)-1,
                0,
                &Returned)) {

            FoundEvent = TRUE;
            EvtClose(EventHandle);
            break;
        }

        EvtClose(QueryHandle);
    }

    return FoundEvent;
}

HRESULT FindDumpPartition(PDMP_CONTEXT Context)
/*++

Routine Description:

This function will find the raw dump partition on the disk, it will 
enumerate all GPT partitions and compare the GUID SVRAWDUMP_PARTITION_GUID

Arguments:

Context - Pointer to DmpContext

Return Value:

HRESULT.

--*/
{
    SYSTEM_DEVICE_INFORMATION   DeviceInfo;
    HRESULT                     result = E_FAIL;

    //
    // Get the number of Disks in the system
    //
    NTSTATUS Status = NtQuerySystemInformation(SystemDeviceInformation, &DeviceInfo, sizeof(SYSTEM_DEVICE_INFORMATION), 0);
    if (Status != ERROR_SUCCESS){
        LogLibErrorPrintf(
            result,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" NtQuerySystemInformation. STATUS: 0x%08x", Status);
    }
    else
    {
        //
        // Loop through the disks
        //
        for (UINT32 DiskIndex = 0; DiskIndex < DeviceInfo.NumberOfDisks; DiskIndex++)
        {

            //
            // Create a Handle to the Hard Disk
            //
            LogLibInfoPrintf(L"[%d] Opening disk: \\\\.\\PhysicalDrive%d", DiskIndex, DiskIndex);
            Context->hDisk.Open(DiskIndex);

            //
            // Only interested in GPT disk, if not then loop.
            //
            if (Context->hDisk.GetDevicePartitionType() != PARTITION_STYLE_GPT)
            {
                switch (Context->hDisk.GetDevicePartitionType())
                {
                    case PARTITION_STYLE_MBR:
                        LogLibInfoPrintf(L"\tPartition type is MBR (0x%x), only GPT is supported", Context->hDisk.GetDevicePartitionType());
                        break;
                    case PARTITION_STYLE_RAW:
                        LogLibInfoPrintf(L"\tPartition type is RAW (0x%x), only GPT is supported", Context->hDisk.GetDevicePartitionType());
                        break;
                    default:
                        LogLibInfoPrintf(L"\tPartition type 0x%x is unsupported, only GPT is supported", Context->hDisk.GetDevicePartitionType());
                        break;
                }

                Context->hDisk.Close();
                continue;
            }
            else
            {
                LogLibInfoPrintf(L"\tFound GPT partitioned device");
            }

            if (SUCCEEDED(Context->hDisk.SetPartition(SVRAWDUMP_PARTITION_GUID)))
            {
                LogLibInfoPrintf(L"\tFound SVRawDump partition!");
                result = S_OK;
                break;
            }

            LogLibInfoPrintf(L"\tCould not find SVRawDump partition on device");
            Context->hDisk.Close();
        }
    }

    if (FAILED(result))
    {
        SetLastError(ERROR_PARTITION_FAILURE);
    }

    return result;
}

HRESULT
VerifyRawDumpHeader(
    PDMP_CONTEXT Context,
    BOOL* IsRawDumpHeaderValid
    )
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

    IsRawDumpHeaderValid - Returned to caller. TRUE if RAW_DUMP_HEADER is valid. FALSE if 
                           no valid dump found.

Return Value:

    HRESULT:
        S_OK
        E_FAIL
        E_INVALIDARG
        E_OUTOFMEMORY
--*/
{
    HRESULT hr = E_FAIL;

    if ((nullptr == IsRawDumpHeaderValid) || (nullptr == Context))
    {
        hr = E_INVALIDARG;
        LogLibInfoPrintf(L"ERROR: VerifyRawDumpHeader() called with invalid parameter(s)");
    }
    else
    {
        PRAW_DUMP_HEADER    dumpHeader = &(Context->RawDumpHeader);

        *IsRawDumpHeaderValid = FALSE;

        if (FAILED(hr = Context->hDisk.ReadAtOffset((PCHAR)dumpHeader, sizeof(RAW_DUMP_HEADER), (LARGE_INTEGER){0}, DEVICE_IO::READ_EXACT)))
        { // failed to read the dump header
            LogLibInfoPrintf(L"Read RAW_Dump Partition failed. Status:0x%x\r\n", hr);
        }
        else if (!IsValidRawDumpHeader(dumpHeader))
        { // Dump header is invalid
            LogLibInfoPrintf(L"ERROR: validating Raw Dump Header -> %s", HEADER_ERROR_WSTRING);

            switch (HEADER_ERROR)
            {
                case RAW_DUMP_HEADER_SIGNATURE_INVALID:
                    LogLibInfoPrintf(L"    Expected: 0x%I64x Actual:0x%I64x", RAW_DUMP_HEADER_SIGNATURE, dumpHeader->Signature);
                    break;

                case RAW_DUMP_HEADER_VERSION_INVALID:
                    LogLibInfoPrintf(L"    Actual: 0x%x Expected: 0x%x", dumpHeader->Version, RAW_DUMP_HEADER_VERSION);
                    break;

                case RAW_DUMP_HEADER_EXPECTED_BITS_INVALID:
                    LogLibInfoPrintf(L"    Flags: 0x%x", dumpHeader->Flags);
                    break;

                case RAW_DUMP_HEADER_OK:
                case RAW_DUMP_HEADER_SIZE_INVALID:
                case RAW_DUMP_HEADER_SECTION_COUNT_INVALID:
                default:
                    break;
            }

        }
        else
        { // Valid dump hear, read section table
            ULONG   RawDumpTableSize = RawDumpTableSize(dumpHeader->SectionsCount);

            Context->pRawDumpSectionTable = (PRAW_DUMP_SECTION_HEADER)malloc(RawDumpTableSize);
            if (nullptr == Context->pRawDumpSectionTable)
            {
                hr = E_OUTOFMEMORY;
                LogLibInfoPrintf(L"Failed to allocate 0x%x bytes of memory for dump table.", RawDumpTableSize);
            }
            else
            {
                size_t bytesRead = 0;
                ZeroMemory(Context->pRawDumpSectionTable, RawDumpTableSize);
                if (FAILED(hr = Context->hDisk.Read((PCHAR)Context->pRawDumpSectionTable, RawDumpTableSize, &bytesRead)))
                {
                    LogLibInfoPrintf(L"Failed to read raw dump table.");
                }
                else if (RawDumpTableSize != bytesRead)
                {
                    hr = E_FAIL;
                    LogLibInfoPrintf(L"Failed to read raw dump table, invalid number of bytes.");
                    LogLibInfoPrintf(L"    Expected: %#x  Actual: %#x", RawDumpTableSize, bytesRead);
                }
                else
                {
                    hr = S_OK;
                    *IsRawDumpHeaderValid = TRUE;
                    LogLibInfoPrintf(L"Dump header validated.");
                    DumpRawDumpHeader(&Context->RawDumpHeader, Context->pRawDumpSectionTable);
                }

            }

        }

    }

    return hr;
}

HRESULT
VerifyRawDumpSectionTable(
    PDMP_CONTEXT Context,
    BOOL* IsRawDumpSectionTableValid
    )
/*++

Routine Description:

    This function validates the RAW_DUMP_HEADER.SectionTable. The following are checked.

    1. At least one DDR section.
    2. No sections with unrecognized section type.
    3. SectionsCount matches number of valid sections.
    4. Only the last section should have the RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE flag.

Arguments:

    Context - Pointer to DmpContext

    IsRawDumpSectionTableValid - Returned to the caller to indicate whether we have a good 
                                 section table or not.

Return Value:

    HRESULT:
        S_OK
        E_FAIL
        E_INVALIDARG

--*/
{
    HRESULT hr = E_INVALIDARG;

    if ((nullptr == Context) ||
        (nullptr == IsRawDumpSectionTableValid)
        )
    {
        LogLibInfoPrintf(L"ERROR: VerifyRawDumpSectionTable() called with invalid parameter(s)");
    }
    else if (E_INVALIDARG != (hr = ValidateRawDumpSectionTable(Context->pRawDumpSectionTable, Context->RawDumpHeader.SectionsCount, &Context->SectionStats)))
    {
        LogLibInfoPrintf(L"       DDR Sections : 0x%x", Context->SectionStats.DDRSectionCount);
        LogLibInfoPrintf(L"SV Specific Sections: 0x%x", Context->SectionStats.SVSectionCount);
        LogLibInfoPrintf(L"CPU Context Sections: 0x%x", Context->SectionStats.CPUContextSectionCount);

        if (0 != Context->SectionStats.CPUContextSectionCount)
        { // Only display this if a CPU context is present
            switch (Context->SectionStats.CpuArchitecture)
            {
                case PROCESSOR_ARCHITECTURE_INTEL:
                    LogLibInfoPrintf(L"This is a x86 machine Dump");
                    break;

                case PROCESSOR_ARCHITECTURE_ARM:
                    LogLibInfoPrintf(L"This is a ARM machine Dump");
                    break;

                case PROCESSOR_ARCHITECTURE_AMD64:
                    LogLibInfoPrintf(L"This is a x64 machine Dump");
                    break;

                case PROCESSOR_ARCHITECTURE_ARM64:
                    LogLibInfoPrintf(L"This is a ARM64 machine Dump");
                    break;

                case PROCESSOR_ARCHITECTURE_UNKNOWN:
                default:
                    LogLibInfoPrintf(L"This is a unknown type machine Dump");
                    break;
            }

        }

        if (SUCCEEDED(hr))
        {
            *IsRawDumpSectionTableValid = TRUE;
            LogLibInfoPrintf(L"Section table validated...");
        }
        else
        {
            *IsRawDumpSectionTableValid = FALSE;
            LogLibInfoPrintf(L"ERROR: Last error encountered is \"%s\"", HEADER_ERROR_WSTRING);
            if (0 == Context->SectionStats.DDRSectionCount)
            {
                LogLibInfoPrintf(L"ERROR: No DDR Sections found");
            }

            if (0 != Context->SectionStats.InvalidVersionCount)
            {
                LogLibInfoPrintf(L"ERROR: Section(s) with invalid version detected. Count: 0x%x", Context->SectionStats.InvalidVersionCount);
            }

            if (0 != Context->SectionStats.InvalidFlagsCount)
            {
                LogLibInfoPrintf(L"ERROR: Section(s) with no flags set detected. Count: 0x%x", Context->SectionStats.InvalidFlagsCount);
            }

            if (0 != Context->SectionStats.InsufficientStorageSectionsCount)
            {
                LogLibInfoPrintf(L"ERROR: Unexpected section(s) with insufficient storage flag. Count: 0x%x", Context->SectionStats.InsufficientStorageSectionsCount);
            }

            if (0 != Context->SectionStats.InvalidSectionCount)
            {
                LogLibInfoPrintf(L"ERROR: Invalid Section(s) Found. Count: 0x%x", Context->SectionStats.InvalidSectionCount);
            }

        }

    }

    return hr;
}


NTSTATUS
BuildDDRMemoryMap(
    PDMP_CONTEXT Context
    )
/*++

Routine Description:

   This routine builds a DDRMemoryMap table.
   The DDR sections in the map will be sorted by base address
   in ascending order, then sanity checked:
   1. swapped start and end is fatal
   2. zero size is fatal
   3. overlap is fatal
   4. fragmentation is non-fatal
   
Arguments:

    Context - Pointer to DmpContext

Return Value:

    NT status code.

--*/
{
    UINT32          allocationSize;
    UINT32          index;
    UINT64          previousEnd;
    UINT64          currentStart;
    NTSTATUS        status = STATUS_UNSUCCESSFUL;
    UINT64          TotalDDRSizeInBytes = 0;

    Context->DDRMemoryMapCount = 0;
    Context->DDRMemoryMap = NULL;

    allocationSize = sizeof(DDR_MEMORY_MAP) * Context->SectionStats.DDRSectionCount;

    Context->DDRMemoryMap = (PDDR_MEMORY_MAP)malloc(allocationSize);

    if (Context->DDRMemoryMap == NULL) {
        LogLibInfoPrintf(L"Failed to allocate memory for DDR memory map.\r\n");
        status = STATUS_NO_MEMORY;
        goto Exit;
    }

    ZeroMemory(Context->DDRMemoryMap, allocationSize);

    //
    // Fill in the table.
    //
    for (index = 0; index <Context->SectionStats.DDRSectionCount; index++)
    {
        if (Context->SectionStats.FirstDDRSection[index].Type != RAW_DUMP_SECTION_TYPE_DDR_RANGE)
        {
            LogLibInfoPrintf(L"ERROR: DDR section 0x%p does not have correct type. Type: 0x%x\r\n",
                             &Context->SectionStats.FirstDDRSection[index],
                             Context->SectionStats.FirstDDRSection[index].Type);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        Context->DDRMemoryMapCount++;
        Context->DDRMemoryMap[index].Base = Context->SectionStats.FirstDDRSection[index].u.DDRInformation.Base;
        Context->DDRMemoryMap[index].Size = Context->SectionStats.FirstDDRSection[index].Size;
        Context->DDRMemoryMap[index].Offset = Context->SectionStats.FirstDDRSection[index].Offset;
        Context->DDRMemoryMap[index].End =  Context->DDRMemoryMap[index].Base + 
                                            Context->DDRMemoryMap[index].Size - 
                                            1;
        TotalDDRSizeInBytes += Context->DDRMemoryMap[index].Size;
    }

    if (TotalDDRSizeInBytes != Context->SectionStats.TotalDDRSizeInBytes)
    { // Sanity cheeck - sum of DDR sections should match
        LogLibInfoPrintf(L"ERROR!!! DDR sections size is not correct.  Expected:%#x  Actual:%#x\r\n",
                         Context->SectionStats.TotalDDRSizeInBytes, TotalDDRSizeInBytes);
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    //
    // DDR sections layout may be random,
    // sort by base address in ascending order
    //
    LogLibInfoPrintf(L"Sorting DDR sections");
    for (UINT32 i = 0; i < Context->DDRMemoryMapCount - 1; i++)
    {
        for (UINT32 j = 0; j < Context->DDRMemoryMapCount - 1 - i; j++)
        {
            if (Context->DDRMemoryMap[j].Base > Context->DDRMemoryMap[j + 1].Base)
            {
                DDR_MEMORY_MAP  tmpDdrRecord;

                memcpy(&tmpDdrRecord, &Context->DDRMemoryMap[j], sizeof(tmpDdrRecord));
                memcpy(&Context->DDRMemoryMap[j], &Context->DDRMemoryMap[j + 1], sizeof(tmpDdrRecord));
                memcpy(&Context->DDRMemoryMap[j + 1], &tmpDdrRecord, sizeof(tmpDdrRecord));
            }
        }
    }

    LogLibInfoPrintf(L"-  -  -  -  Sorted DDR Memory  -  -  -  -");
    for (unsigned int idx = 0; (idx < Context->DDRMemoryMapCount); idx++)
    {
        LogLibInfoPrintf(L"\r\n  Section %d:", idx);
        LogLibInfoPrintf(L"      Context->DDRMemoryMap[%03d].Base  : 0x%llx", idx, Context->DDRMemoryMap[idx].Base);
        LogLibInfoPrintf(L"      Context->DDRMemoryMap[%03d].Size  : 0x%llx", idx, Context->DDRMemoryMap[idx].Size);
        LogLibInfoPrintf(L"      Context->DDRMemoryMap[%03d].Offset: 0x%llx", idx, Context->DDRMemoryMap[idx].Offset);
        LogLibInfoPrintf(L"      Context->DDRMemoryMap[%03d].End   : 0x%llx", idx, Context->DDRMemoryMap[idx].End);
    }

    LogLibInfoPrintf(L"\r\n-  -  -  -  -  -  -  -  -  -  -  -  -\r\n");

    //
    // Check for sane DDR sections (no overlap, no negative size, no swapped boundaries)
    //
    LogLibInfoPrintf(L"Checking DDR section sanity");
    for (index = 0; index < Context->DDRMemoryMapCount; index++)
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
            previousEnd     = Context->DDRMemoryMap[index - 1].End;
            currentStart    = Context->DDRMemoryMap[index].Base;
            
            //
            // Ideal layout is each new segment starts at the previous end + 1
            // Anything else is a hole (harmless) or overlap (fatal)
            //
            if (previousEnd < currentStart - 1)
            {
                Context->SectionStats.DDRSectionFragmentationCount++;
            }
            else if (previousEnd >= currentStart)
            {
                Context->SectionStats.DDRSectionsOverlapCount++;
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
    
    if (Context->SectionStats.DDRSectionFragmentationCount != 0)
    {
        LogLibInfoPrintf(L"Hole count %u in DDR Sections is harmless", Context->SectionStats.DDRSectionFragmentationCount);
    }
    
    if (Context->SectionStats.DDRSectionsOverlapCount != 0)
    {
        LogLibInfoPrintf(L"Overlap count %u in DDR Sections is fatal", Context->SectionStats.DDRSectionsOverlapCount);
        goto Exit;
    }

    status = STATUS_SUCCESS;
Exit:
    return status;
}

NTSTATUS
ReadFromDDRSectionByPhysicalAddress(
    _In_ PDMP_CONTEXT Context,
    _In_ LARGE_INTEGER PhysicalAddress,
    _In_ UINT64 Length,
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
    UINT64              addressStart = 0; 
    UINT64              addressEnd = 0;
    UINT32              bytesToRead = 0;
    UINT64              bytesRemain = 0;
    UINT32              ddrSectionsCount = 0;
    LARGE_INTEGER       offset = { 0 };
    PDDR_MEMORY_MAP     ddrMap = NULL;
    UINT32              index = 0;
    UINT64              sectionStart = 0;
    UINT64              sectionEnd = 0;
    UINT32              sectionSpanCount = 0;
    NTSTATUS            status = STATUS_UNSUCCESSFUL;
    PVOID               temp = 0;

    ddrSectionsCount = Context->DDRMemoryMapCount;
    ddrMap = Context->DDRMemoryMap;
    addressStart = PhysicalAddress.QuadPart;
    addressEnd = addressStart + Length - 1;
    bytesRemain = Length;
    temp = Buffer;

    //
    // Determine the right section.
    //
    for (index = 0; index < ddrSectionsCount; index++) {
        
        sectionStart = ddrMap[index].Base;
        sectionEnd = ddrMap[index].Base + ddrMap[index].Size -1 ;       

        if ((sectionSpanCount != 0) &&
            (bytesRemain != 0) &&
            (!ddrMap[index].Contiguous)) {
            LogLibInfoPrintf(L"Attempting to read spanning discontiguous sections. "
                      L"Section: 0x%x BytesRemain: 0x%x\r\n",
                      index,
                      bytesRemain);
            status = STATUS_INVALID_PARAMETER;
            goto Exit;
        }

        //
        // Determine if address falls in to this section.
        //
        if ((sectionStart <= addressStart) &&
            (sectionEnd >= addressStart)) {
            LogLibInfoPrintf(L"Section %d:    sectionStart=0x%llx    sectionEnd=0x%llx", index, sectionStart, sectionEnd);
            LogLibInfoPrintf(L"  Memory found in ddrmap[%d]:", index);
            LogLibInfoPrintf(L"      Base          0x%llx (sectionStart)", ddrMap[index].Base);
            LogLibInfoPrintf(L"      End           0x%llx", ddrMap[index].End);
            LogLibInfoPrintf(L"      Size          0x%llx", ddrMap[index].Size);
            LogLibInfoPrintf(L"      Offset        0x%llx", ddrMap[index].Offset);
            LogLibInfoPrintf(L"      Contiguous    %s", ddrMap[index].Contiguous ? L"True" : L"False");

            //
            // Figure out how many bytes to read.
            // Need to figure this out because we may 
            // cross section boundaries.
            //
            bytesToRead = (sectionEnd >= addressEnd) ? 
                          (UINT32)(addressEnd - addressStart + 1) : 
                          (UINT32)(sectionEnd - addressStart + 1);

            offset.QuadPart  = (addressStart - sectionStart) + ddrMap[index].Offset;

            LogLibInfoPrintf(L"addressStart=0x%I64x, sectionStart=0x%I64x, ddrMap[%d].Offset=0x%I64x\r\n",
                addressStart, sectionStart, index, ddrMap[index].Offset); 
            LogLibInfoPrintf(L"Reading 0x%x bytes at offset 0x%I64x\r\n", bytesToRead, offset.QuadPart);
            
            if (FAILED(Context->hDisk.ReadAtOffset((PCHAR)temp, bytesToRead, offset, DEVICE_IO::READ_ANY)))
            {
                status = STATUS_UNSUCCESSFUL;
                LogLibInfoPrintf(L"[ERROR] ReadDisk Failed with result=FALSE");
                goto Exit;
            }
                
                        
            //
            // Update bytesRemain.
            //
            bytesRemain = bytesRemain - bytesToRead;

            // LogLibInfoPrintf(L"0x%x bytes remain to be read.\r\n", bytesRemain);

            if (bytesRemain != 0)
            {
                //
                // Time to move to next section.
                // Update temp.
                //
                Buffer = (PVOID)((UINT32)(Buffer) + bytesToRead);

                //
                // Update addressStart.
                // AddressEnd should not be updated. 
                //
                addressStart = addressStart + bytesToRead;
                sectionSpanCount++;
            }
            else
            {
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
    if ((index == ddrSectionsCount) &&
        (bytesRemain != 0))
    {
        LogLibInfoPrintf(L"Read incomplete. 0x%x bytes not read out of 0x%x bytes.\r\n", 
                  bytesRemain,
                  Length);

        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

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
    IDebugDataSpaces2 *DebugDataSpaces = DbgClient::GetDataSpaces();
    if (DebugDataSpaces == nullptr) {
        LogLibInfoPrintf(L"Failure in dbgeng, KdDebuggerDataBlock may be unreadable");
        return STATUS_UNSUCCESSFUL;
    }
        
    if (Context->Is64Bit) {
//        status = ReadFromDDRSectionByVirtualAddress64(Context,
//                    VirtualAddress,
//                    Length,
//                    Buffer,
//                    PhysicalAddress);

        HRESULT hr = DebugDataSpaces->VirtualToPhysical(VirtualAddress, &PA64);
        if (FAILED(hr)) {
            LogLibInfoPrintf(L"Failed to find physical address Error %x", hr);
            status =  STATUS_UNSUCCESSFUL;
        }
        else{
            status =  STATUS_SUCCESS;
            if (PhysicalAddress != nullptr)
                PhysicalAddress->QuadPart = PA64;
        }
    }
    else {
         status = ReadFromDDRSectionByVirtualAddress32(Context,
                    (UINT32)VirtualAddress,
                    (UINT32)Length,
                    Buffer,
                    PhysicalAddress);
    }
    
    return status;
}


BOOL
ISContainDumpHeader(
    _In_ PDMP_CONTEXT Context,
    _In_ BYTE* buffer,
    _In_ LONG  buffersize,
    _In_ LONG* offset)
{
    BOOL                foundDumpHeader = FALSE;
    LONG                i =0;
    LONG                magicstringlength = 0;

    UCHAR PAGEDUMP[] = { // Spells --> PAGEDUMP
    0x50, 0x41, 0x47, 0x45, 0x44, 0x55, 0x4D, 0x50
    };
    
    UCHAR PAGEDUMP64[] = { // Spells --> PAGEDU64
    0x50, 0x41, 0x47, 0x45, 0x44, 0x55, 0x36, 0x34
    };

    // Magic String --> ;ISS”E.0ÔËÚ—ñµè6aˆp›
    UCHAR InMemoryDumpHeaderMagicString[] = {
    0x3B,0x49,0x53,0x53,0x94,0x45,0x2E,0x30,
    0xD4,0xCB,0xDA,0x97,0xF1,0x11,0x02,0xB5,
    0xE8,0x36,0x08,0x61,0x88,0x70,0x9B,0x19
    };
    
    magicstringlength = sizeof (InMemoryDumpHeaderMagicString) + sizeof (PAGEDUMP);

    for(i =0 ; i<buffersize - magicstringlength; i++)
    {
        if( memcmp((buffer + i), InMemoryDumpHeaderMagicString, sizeof(InMemoryDumpHeaderMagicString)) == 0)
        {
            *offset = i + sizeof(InMemoryDumpHeaderMagicString);
            foundDumpHeader = TRUE;
            LogLibInfoPrintf(L"Found dump header magic string at offset: 0x%llx", i);
            LogLibInfoPrintf(L"      dump header begins at offset: 0x%llx", *offset);

            if( memcmp((buffer + *offset), PAGEDUMP, sizeof(PAGEDUMP)) == 0 )
            {
                LogLibInfoPrintf(L"Dump header type is PAGEDUMP");
                goto EXIT;
            }
            else if (memcmp((buffer + *offset), PAGEDUMP64, sizeof(PAGEDUMP64)) == 0)
            {
                Context->Is64Bit = TRUE;
                LogLibInfoPrintf(L"Dump header type is PAGEDU64");
                goto EXIT;
            }
            else
            { // Invalid Header - continue looking...
                TCHAR szDisplay[max(sizeof(PAGEDUMP), sizeof(PAGEDUMP64)) + 1] = { 0 };
                
                memcpy(szDisplay, (buffer + *offset), sizeof(szDisplay) - 1);

                LogLibInfoPrintf(L"Failed to find dump header type, expected string PAGEDUMP or PAGEDU64");
                LogLibInfoPrintf(L"   Actual: %S", szDisplay);
                foundDumpHeader = FALSE;

            }
        }
    }

EXIT:
    return foundDumpHeader;
}

HRESULT
SearchDumpHeaderAndAPREG(_Inout_ PDMP_CONTEXT Context)
{
    ULONG               index = 0;
    LONG                buffersize = PAGE_SIZE*1024*8;
    LONGLONG            totalread =0; 
    LARGE_INTEGER       curOffset = { 0 };
    PCHAR               buffer = nullptr;
    HRESULT             hr = E_FAIL;
    BOOL                foundDumpHeader = FALSE;
    BOOL                foundAPRG = FALSE;
    
    Context->Is64Bit = FALSE;    
    //
    // if this is 64 bit APREG, dont attempt to find th eAPREG in the memory
    //
    foundAPRG = (Context->isAPREG64 ||  !Context->IsAPREGRequested) ? TRUE : FALSE; 

    // calculate how much data to read;
    //
    if (DEVICE_IO::PLAIN_FILE_DEVICE_TYPE == Context->hDisk.GetDeviceType())
    { // entire file by (size)
        totalread = Context->hDisk.GetCurrentFileSize();
    }
    else
    { // number of reads with the buffer (size) being used
        totalread = Context->hDisk.GetCurrentPartitionSize() / buffersize;
    }

    //
    // allocate the buffer
    //
    buffer = (PCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffersize);
    if (nullptr == buffer)
    {
        LogLibInfoPrintf(L"Could not allocate memory for read buffer %ld", buffersize);
        goto EXIT;
    }

    // start at the beginning of the partition
    if (FAILED(hr = Context->hDisk.SetPos(0)))
    {
        LogLibInfoPrintf(L"Failed to find the beginning of partition! ");
        goto EXIT;
    }
    
    while (curOffset.QuadPart < totalread)
    { // Search for Magicstring and AP_Reg
        size_t  bRead = 0;
        LONG    bufferOffset = 0;

        if (FAILED(hr = Context->hDisk.Read(buffer, buffersize, &bRead)))
        {
            LogLibInfoPrintf(L"Failed to Read partition! ");
            goto EXIT;
        }

        if (!foundDumpHeader)
        { // Check for a Dump Header in this buffer
            foundDumpHeader = ISContainDumpHeader(Context, (BYTE*)buffer, buffersize, &bufferOffset);
            if (foundDumpHeader)
            {
                Context->DumpHeaderAddress.QuadPart = curOffset.QuadPart + bufferOffset;
                LogLibInfoPrintf(L"   Dump Header found at address 0x%llx (%lld)", Context->DumpHeaderAddress.QuadPart, Context->DumpHeaderAddress.QuadPart);
            }

        }

        if (!foundAPRG)
        { // Check for an AP_REG in this buffer
            foundAPRG = ISContainAPREG((BYTE*)buffer, buffersize, &bufferOffset);
            if (foundAPRG)
            {
                Context->APRegAddress.QuadPart = curOffset.QuadPart + bufferOffset;
                LogLibInfoPrintf(L"   AP_REG found at address 0x%llx (%lld)", Context->APRegAddress.QuadPart, Context->APRegAddress.QuadPart);
            }

        }

        if (foundDumpHeader && foundAPRG)
        {
            LogLibInfoPrintf(L"   Found Dump Header and AP_REG!");
            break;
        }

        curOffset.QuadPart += bRead;
        wprintf(L"      ");
        switch (index % 4)
        {
            case 0:
                wprintf(L"-");
                break;
            case 1:
                wprintf(L"\\");
                break;
            case 2:
                wprintf(L"|");
                break;
            case 3:
                wprintf(L"/");
                break;
            default:
                wprintf(L"*");
                break;
        }
        wprintf(L" we searched %ld MB   \r", (index*buffersize) / (1024 * 1024));
        index++;
    }

    hr = S_OK;

EXIT:
    LogLibInfoPrintf(L"       Completed search %ld MB ", (index*buffersize) / (1024 * 1024));

    if (nullptr != buffer)
    {
        HeapFree(GetProcessHeap(), 0, buffer);
    }

    if (FAILED(hr))
    {
        if (foundDumpHeader != TRUE)
        {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L"We Searched the whole memory could not find pre-built Dump header");
        }

        if (foundAPRG != TRUE)
        {
            Context->HasValidAP_REG = FALSE;
            LogLibInfoPrintf(L"Error:Failed to find APReg, looks like this offline dump not triggered by secure watchdog! \n");
        }

    }

    return hr;
}


VOID
DumpRawDumpHeader(
    _In_ PRAW_DUMP_HEADER           Header,
    _In_ PRAW_DUMP_SECTION_HEADER   SectionTable
)
{
    LogLibInfoPrintf(L"*******  RAW DUMP HEADER  *******");
    LogLibInfoPrintf(L"\t           Signature: 0x%I64x", Header->Signature);
    LogLibInfoPrintf(L"\t             Version: 0x%x", Header->Version);
    LogLibInfoPrintf(L"\t               Flags: 0x%x", Header->Flags);
    LogLibInfoPrintf(L"\t              OsData: 0x%I64x", Header->OsData);
    LogLibInfoPrintf(L"\t          CpuContext: 0x%I64x", Header->CpuContext);
    LogLibInfoPrintf(L"\t        ResetTrigger: 0x%x", Header->ResetTrigger);
    LogLibInfoPrintf(L"\t            DumpSize: 0x%I64x", Header->DumpSize);
    LogLibInfoPrintf(L"\tTotalDumpSizeRequire: 0x%I64x", Header->TotalDumpSizeRequired);
    LogLibInfoPrintf(L"\t       SectionsCount: 0x%x", Header->SectionsCount);

    LogLibInfoPrintf(L"\r\n*******  RAW DUMP SECTIONS  *******");
    for (ULONG index = 0; index < Header->SectionsCount; index++)
    {
        UINT32  namelength = 0;
        CHAR name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH] = { 0 };
        WCHAR sectionName[RAW_DUMP_SECTION_HEADER_NAME_LENGTH + 1] = { 0 };

        LogLibInfoPrintf(L"\t    Section[%d]", index);
        LogLibInfoPrintf(L"\t               Flags: 0x%x", SectionTable[index].Flags);
        LogLibInfoPrintf(L"\t             Version: 0x%x", SectionTable[index].Version);
        LogLibInfoPrintf(L"\t              Offset: 0x%I64x", SectionTable[index].Offset);
        LogLibInfoPrintf(L"\t                Size: 0x%I64x", SectionTable[index].Size);

        memset(name, 0, RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
        memcpy(name, (void*) &(SectionTable[index].Name), RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
        namelength = (UINT32)strlen(name);
        sectionName[namelength] = 0;
        mbstowcs(sectionName, name, namelength);
        LogLibInfoPrintf(L"\t                Name: %s", sectionName);

        if (SectionTable[index].Type < RAW_DUMP_SECTION_TYPE_MAX)
        {
            switch (SectionTable[index].Type)
            {
                case RAW_DUMP_SECTION_TYPE_RESERVED:
                    LogLibInfoPrintf(L"\t                Type: 0x%x == > reserved", SectionTable[index].Type);
                    break;

                case RAW_DUMP_SECTION_TYPE_DDR_RANGE:
                    LogLibInfoPrintf(L"\t                Type: 0x%x == > DDR Section", SectionTable[index].Type);
                    LogLibInfoPrintf(L"\t                Base: 0x%I64x", SectionTable[index].u.DDRInformation.Base);
                    break;

                case RAW_DUMP_SECTION_TYPE_CPU_CONTEXT:
                    LogLibInfoPrintf(L"\t                Type: 0x%x == > CPU CONTEXT", SectionTable[index].Type);
                    break;

                case RAW_DUMP_SECTION_TYPE_SV_SPECIFIC:
                    LogLibInfoPrintf(L"\t                Type: 0x%x == > SV SPECIFIC", SectionTable[index].Type);
                    DumpGUID((GUID*)SectionTable[index].u.SVSpecificInformation.SVSpecificData);
                    break;

                default:
                    LogLibInfoPrintf(L"\t                Type: 0x%x == > UNKNOWN", SectionTable[index].Type);
                    break;
            }

        }

    }

    return;
}


HRESULT 
InitDumpFile(
        _Inout_ PDMP_CONTEXT Context)
{
    LARGE_INTEGER    fileoffset;
    HRESULT          result = E_FAIL;
    Context->DedicatedDumpHandle = INVALID_HANDLE_VALUE;
    Context->DedicatedDumpFilePath = L"raw_dump.dmp";

    fileoffset.QuadPart = 0;

    Context->DedicatedDumpHandle  = CreateFileW( Context->DedicatedDumpFilePath,
                                                    GENERIC_READ | GENERIC_WRITE,
                                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                    NULL,
                                                    OPEN_ALWAYS,
                                                    FILE_ATTRIBUTE_NORMAL,
                                                    NULL );

    if (Context->DedicatedDumpHandle  == INVALID_HANDLE_VALUE) {
        LogLibErrorPrintf(
            result,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,L"Failed to Create entire file!\n");
        goto Exit;
    }
    wprintf(L"Created Dedicated Dump File \'%s\', Context->DedicatedDumpHandle=0x%x!\n", Context->DedicatedDumpFilePath, (UINT32)Context->DedicatedDumpHandle);

    //
    // Get the sum of storage requirements for data.
    //
    Context->ActualDumpFileUsedInBytes.QuadPart = Context->SizeAccordingToMemoryDescriptors + Context->SectionStats.TotalCpuContextSizeInBytes;

    //
    // Get the sum of storage requiremetns for metadata.
    //
    if (Context->Is64Bit==TRUE)
    {
        Context->ActualDumpFileUsedInBytes.QuadPart += sizeof(DUMP_HEADER64);
    }
    else
    {
        Context->ActualDumpFileUsedInBytes.QuadPart += sizeof(DUMP_HEADER32);
    }

    LogLibInfoPrintf(L"  0x%llx (%llu) bytes are required to store the dump contents.\r\n", 
              Context->ActualDumpFileUsedInBytes, Context->ActualDumpFileUsedInBytes);
    
    result = ERROR_SUCCESS;

Exit:
    return result;
}

HRESULT
WriteRAWDDRToBinAndSearchHeaders(
    _Inout_ PDMP_CONTEXT Context,
    _In_    BOOL         DDR_WriteFlag
    )
/*++

Routine Description:

    This function writes RAW DDR sections to bin files 

Arguments:

    Context - Pointer to the global context structure.

Return Value:

    NT status code.

--*/
{
    UINT32                  index = 0;
    UINT32                  counter = 0;
    ULONG                   bufferSize = DEFAULT_DMP_BUF_SZ; //4MB
    ULONG                   readbufferSize = 0;
    LONGLONG                remainsize = 0; //4MB
    PCHAR                   tempBuffer = nullptr;
    LARGE_INTEGER           offset = { 0 };
    LARGE_INTEGER           sectionEnd;
    LARGE_INTEGER           fileoffset = { 0 };
    HRESULT                 hr = E_FAIL;
    HANDLE                  hFile = INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK         statusBlock;
    WCHAR                   Filename[MAX_PATH];
    BOOL                    foundDumpHeader = FALSE;
    // if this is 64 bit APREG, dont attempt to find the APREG in the memory
    BOOL                    foundAPRG = (Context->isAPREG64 || !Context->IsAPREGRequested) ? TRUE : FALSE;

    Context->Is64Bit = FALSE;
     
    tempBuffer = (PCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferSize);
    if (nullptr == tempBuffer) {
        LogLibInfoPrintf(L"Failed to allocate intermediate buffer for writing SV specific sections. Size: %u\r\n", 
                  Context->SectionStats.LargestSVSpecificSectionSize);
        goto Exit;
    }

    for (index = 0; index <Context->SectionStats.DDRSectionCount; index++)
    {
        if (RAW_DUMP_SECTION_TYPE_DDR_RANGE != Context->SectionStats.FirstDDRSection[index].Type)
        {
            LogLibInfoPrintf(L"ERROR!!! DDR section 0x%p does not have correct type. Type: 0x%x\r\n",
                             &Context->SectionStats.FirstDDRSection[index],
                             Context->SectionStats.FirstDDRSection[index].Type);
            hr = HRESULT_FROM_NT(STATUS_BAD_DATA);
            goto Exit;
        }

        //
        // Only include DDR sections that are valid.
        //
        if (RAW_DUMP_HEADER_FLAGS_VALID != (Context->SectionStats.FirstDDRSection[index].Flags & RAW_DUMP_HEADER_FLAGS_VALID))
        {
            LogLibInfoPrintf(L"Skipping DDR section with incomplete memory.\r\n");
            continue;
        }

        if (TRUE == DDR_WriteFlag)
        {
            wsprintf(Filename, L"DDRSection_%d.bin", index);
            fileoffset.QuadPart = 0;
            hFile = CreateFileW(Filename,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL);
            if (hFile == INVALID_HANDLE_VALUE)
            {
                LogLibErrorPrintf(
                    E_FAIL,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" Create File Failed. INVALID_HANDLE_VALUE. %ls\n", Filename);
                goto Exit;
            }

        }
        else
        {
            LogLibInfoPrintf(L"  Processing DDR Section %d", index);
        }

        offset.QuadPart = Context->SectionStats.FirstDDRSection[index].Offset;
        sectionEnd.QuadPart = Context->SectionStats.FirstDDRSection[index].Offset + Context->SectionStats.FirstDDRSection[index].Size;
        readbufferSize = bufferSize;

        while (offset.QuadPart < sectionEnd.QuadPart)
        {
            LONG    internalOffset = 0;

            wprintf(L"      ");
            switch (counter%4)
            {
                case 0:
                    wprintf(L"-");
                    break;
                case 1:
                    wprintf(L"\\");
                    break;
                case 2:
                    wprintf(L"|");
                    break;
                case 3:
                    wprintf(L"/");
                    break;
                default:
                    wprintf(L"*");
                    counter++;
                    break;
            }
            wprintf(L" Processing DDR Memory %d MB     \r", counter*bufferSize / (1024 * 1024));
            counter++;

            if (FAILED(hr = Context->hDisk.ReadAtOffset((PCHAR)tempBuffer, readbufferSize, offset, DEVICE_IO::READ_ANY)))
            {
                LogLibInfoPrintf(L"Failed to read DDR section from device. HRESULT: 0x%x\r\n", hr);
                goto Exit;
            }
            
            if (TRUE == DDR_WriteFlag)
            {
                hr = HRESULT_FROM_NT(NtWriteFile(hFile,
                                                 NULL,
                                                 NULL,
                                                 NULL,
                                                 &statusBlock,
                                                 tempBuffer,
                                                 readbufferSize,
                                                 &fileoffset,
                                                 NULL));
                if (FAILED(hr))
                {
                    LogLibInfoPrintf(L"Failed to write DDR section to Bin file. Status: 0x%x\r\n", hr);
                    goto Exit;
                }

                NtFlushBuffersFile(hFile, &statusBlock);
                fileoffset.QuadPart += readbufferSize;
            }

            //
            // as we already read data to memory we can process it
            //
            if(!foundDumpHeader){
                foundDumpHeader = ISContainDumpHeader(Context, (BYTE*)tempBuffer, readbufferSize, &internalOffset);
                if(foundDumpHeader) {
                    Context->DumpHeaderAddress.QuadPart = offset.QuadPart + internalOffset;
                    LogLibInfoPrintf(L"We found the magicstring at 0x%llx", Context->DumpHeaderAddress.QuadPart);
                }
            }

            if(!foundAPRG){
                foundAPRG = ISContainAPREG((BYTE*)tempBuffer , readbufferSize, &internalOffset);
                if(foundAPRG) {
                    Context->APRegAddress.QuadPart = offset.QuadPart + internalOffset;
                    LogLibInfoPrintf(L"We found the AP_REG at 0x%llx",  Context->APRegAddress.QuadPart);
                }
            }
            
            if (foundAPRG && foundDumpHeader && (!Context->DumpDDR) && !(Context->isAPREG64))
            { // End if - both AP_Reg (ARM32) and Magicstring found and not dumping DDR
                LogLibInfoPrintf(L"Completed search for AP_REG and Magicstring");
                break;
            }
            
            offset.QuadPart += readbufferSize;
            remainsize = sectionEnd.QuadPart - offset.QuadPart;
            if (remainsize  < readbufferSize) {
                readbufferSize = (ULONG)remainsize;
            }
        }

        readbufferSize = bufferSize;
        if (TRUE == DDR_WriteFlag)
        {

            LogLibInfoPrintf(L"\r      DDR Section %d - write completed",index );
            CloseHandle(hFile);
            hFile = INVALID_HANDLE_VALUE;
        }
        LogLibInfoPrintf(L"      Processed DDR Memory %d MB.\n", counter*bufferSize / (1024 * 1024));
    }

    hr = S_OK;

Exit:
    if(!foundDumpHeader){
        hr = E_FAIL;
        LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__, 
                L"Failed to find the pre-built Dump header in memory.  HRESULT:0x%x\r\n", hr);
    }
    
    if (!foundAPRG) {
        Context ->HasValidAP_REG = FALSE;
        LogLibInfoPrintf(L"Error:Failed to find APReg, looks like this offline dump not triggered by secure watchdog! \n"); 
    }
    if (tempBuffer != NULL) {
        HeapFree(GetProcessHeap(),0, tempBuffer);
    }
    return hr;
}


NTSTATUS
WriteSVSpecific(
    _Inout_ PDMP_CONTEXT Context
    )
/*++

Routine Description:

    This function writes SV Specific sections based on the DUMP_HEADER's physical memory descriptor 
    to dump file.

Arguments:

    Context - Pointer to the global context structure.

Return Value:

    NT status code.

--*/
{
    UINT32                  sectionsCount = 0;
    UINT32                  sectionIndex = 0;
    UINT32                  namelength = 0 ;
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    PCHAR                   tempBuffer = nullptr;
    WCHAR                   Filename[MAX_PATH];
    CHAR                    name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];    
    WCHAR                   sectionName[RAW_DUMP_SECTION_HEADER_NAME_LENGTH+1];
    LARGE_INTEGER           offset = { 0 };
    LARGE_INTEGER           fileoffset;
    LARGE_INTEGER           sectionOffset;
    HANDLE                  hFile = INVALID_HANDLE_VALUE;
    IO_STATUS_BLOCK         statusBlock;

    sectionName[RAW_DUMP_SECTION_HEADER_NAME_LENGTH] = 0;

    sectionsCount = Context->RawDumpHeader.SectionsCount;
    tempBuffer = (PCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (UINT32)Context->SectionStats.LargestSVSpecificSectionSize);
    if (nullptr == tempBuffer) {
        LogLibInfoPrintf(L"Failed to allocate intermediate buffer for writing SV specific sections. Size: %u\r\n", 
                  Context->SectionStats.LargestSVSpecificSectionSize);
        goto Exit;
    }

    for (sectionIndex = 0; sectionIndex < sectionsCount; sectionIndex++) {
        PRAW_DUMP_SECTION_HEADER section = &(Context->pRawDumpSectionTable[sectionIndex]);

        if (section->Type == RAW_DUMP_SECTION_TYPE_SV_SPECIFIC) {
            sectionOffset.QuadPart = section->Offset;
            if (FAILED(Context->hDisk.ReadAtOffset(tempBuffer, (ULONG)section->Size, sectionOffset, DEVICE_IO::READ_EXACT)))
            {
                LogLibInfoPrintf(L"Failed to read SV section from device.\r\n");
                goto Exit;
            }

            memset(name, 0, RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
            memcpy(name, (void*) &(section->Name), RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
            namelength = (UINT32)strlen(name);
            if(0 == namelength) {
                   LogLibErrorPrintf(
                    E_FAIL,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" The SV Section Name length is 0!!! Seciton Index is = %d \n", sectionIndex);
                    goto Exit;
            }
            sectionName[namelength] = 0;
            mbstowcs(sectionName, name, namelength);
            //write to disk
            wsprintf(Filename, L"SV_%s.bin", sectionName);

            fileoffset.QuadPart = 0;

            hFile = CreateFileW( Filename,
                                GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL,
                                OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                NULL );
            if (hFile == INVALID_HANDLE_VALUE){
                   LogLibErrorPrintf(
                    E_FAIL,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" Create File Failed. INVALID_HANDLE_VALUE. %s\n", Filename);
                goto Exit;
            }
           
            status = NtWriteFile( hFile,
                        NULL,
                        NULL,
                        NULL,
                        &statusBlock,
                        tempBuffer,
                        (ULONG)section->Size,
                        &fileoffset,
                        NULL ); 
            if (!NT_SUCCESS(status)) {
                LogLibInfoPrintf(L"Failed to write SV section to dump file. Status: 0x%x\r\n", 
                            status);
                goto Exit;
            }

            CloseHandle(hFile);
            LogLibInfoPrintf(L"Written SV section to File %s.\r\n", Filename);
        } // RAW_DUMP_SECTION_TYPE_SV_SPECIFIC
    }//for sectionindex

    status = STATUS_SUCCESS;
Exit:
    if (nullptr != tempBuffer)
    {
        HeapFree(GetProcessHeap(),0, tempBuffer);
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
    //
    // Consider that the debug data block may be from an older dump (smaller in size)
    //
    if (Header->Size == DDEBUGGER_DATA64_CURRENT_SIZE)
    {
        LogLibInfoPrintf(L"Header size matches the current KDDEBUGGER_DATA64 structure. Size: 0x%x\r\n", Header->Size);
    }
    else if (Header->Size == DDEBUGGER_DATA64_WIN81_SIZE)
    {
        LogLibInfoPrintf(L"Header size matches the Windows 8.1 KDDEBUGGER_DATA64 structure. Size: 0x%x\r\n", Header->Size);
    }
    else if (Header->Size == DDEBUGGER_DATA64_WIN80_SIZE)
    {
        LogLibInfoPrintf(L"Header size matches the Windows 8.0 KDDEBUGGER_DATA64 structure. Size: 0x%x\r\n", Header->Size);
    }
    else if (Header->Size == DDEBUGGER_DATA64_WIN70_SIZE)
    {
        LogLibInfoPrintf(L"Header size matches the Windows 7.0 KDDEBUGGER_DATA64 structure. Size: 0x%x\r\n", Header->Size);
    }
    else if (Header->Size == DDEBUGGER_DATA64_LEGACY_SIZE)
    {
        LogLibInfoPrintf(L"Header size matches the Pre-Windows 7.0 KDDEBUGGER_DATA64 structure. Size: 0x%x\r\n", Header->Size);
    }
    else
    {
        LogLibInfoPrintf(L"Header size does not equal any of the supported KDDEBUGGER_DATA64 structures. Actual: 0x%x Expected: 0x%x\r\n", Header->Size, sizeof(KDDEBUGGER_DATA64));
        return FALSE;
    }

    if (Header->OwnerTag != KDBG_TAG)
    {
        LogLibInfoPrintf(L"OwnerTag does not match expected. Actual: 0x%x Expected: 0x%x\r\n",
                          Header->OwnerTag,
                          KDBG_TAG);
        return FALSE;
    }

    LogLibInfoPrintf(L"KdDebuggerDataBlock.Header checks out.\r\n");
    return TRUE;
}


#define GET_PHYSICAL_MEMORY_DESCRIPTOR_VALUE(fld, TYPE) ((Context->Is64Bit) \
    ? ((TYPE)(((PPHYSICAL_MEMORY_DESCRIPTOR64)physDesc)->fld)) \
    : ((TYPE)(((PPHYSICAL_MEMORY_DESCRIPTOR32)physDesc)->fld)) )

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
    BOOLEAN                         foundDescriptor = FALSE;
    LARGE_INTEGER                   endPA = { 0 };
    UINT32                          indexPD = { 0 };
    LARGE_INTEGER                   ioOffset = { 0 };
    LARGE_INTEGER                   offset = { 0 };
    PVOID                           physDesc = nullptr;
    LARGE_INTEGER                   startPA = { 0 };
    NTSTATUS                        status =  STATUS_UNSUCCESSFUL;
    IO_STATUS_BLOCK                 statusBlock;

    physDesc = (Context->Is64Bit)
        ? (PVOID)&(Context->DumpHeader64->PhysicalMemoryBlock)
        : (PVOID)&(Context->DumpHeader->PhysicalMemoryBlock);

    startPA.QuadPart = PhysicalAddress.QuadPart;
    endPA.QuadPart = startPA.QuadPart + Size - 1;

    LogLibInfoPrintf(L"Looking up the physical memory descriptor containing the memory range of interest. Start: 0x%I64x End: 0x%I64x\r\n",
             startPA, 
             endPA);

    //
    // Look for the memory (PhysicalAddress) in the list of memory descriptors.
    // The memory parameters PhysicalAddress and Size describe the memory block 
    // we are looking for. The presumption is that all memory blocks are represented
    // in one of the runs. If the block is not found, this is a serious problem.
    //
    ULONG maxRuns = GET_PHYSICAL_MEMORY_DESCRIPTOR_VALUE(NumberOfRuns, ULONG);
    for (indexPD = 0; indexPD < maxRuns; indexPD++)
    {
        LARGE_INTEGER   startPD = { 0 };
        LARGE_INTEGER   endPD = { 0 };
        LARGE_INTEGER   lengthPD = { 0 };

        if (Context->Is64Bit)
        {
            startPD.QuadPart = GET_PHYSICAL_MEMORY_DESCRIPTOR_VALUE(Run[indexPD].BasePage, ULONG64);
        }
        else
        {
            startPD.QuadPart = GET_PHYSICAL_MEMORY_DESCRIPTOR_VALUE(Run[indexPD].BasePage, ULONG);
        }

        startPD.QuadPart *= PAGE_SIZE;

        if (Context->Is64Bit)
        {
            lengthPD.QuadPart = GET_PHYSICAL_MEMORY_DESCRIPTOR_VALUE(Run[indexPD].PageCount, ULONG64);
        }
        else
        {
            lengthPD.QuadPart = GET_PHYSICAL_MEMORY_DESCRIPTOR_VALUE(Run[indexPD].PageCount, ULONG);
        }

        lengthPD.QuadPart *= PAGE_SIZE;

        endPD.QuadPart = startPD.QuadPart + lengthPD.QuadPart - 1;

        LogLibInfoPrintf(L"Descriptor %u >> Base: 0x%I64x  End:0x%I64x  Offset: 0x%I64x",
                         indexPD,
                         startPD.QuadPart,
                         endPD.QuadPart,
                         offset.QuadPart);

        if ((startPA.QuadPart >= startPD.QuadPart) &&
            (endPD.QuadPart >= endPA.QuadPart))
        {
            foundDescriptor = TRUE;
            LogLibInfoPrintf(L"Descriptor %u contains the memory. Base: 0x%I64x End:0x%I64x Offset: 0x%I64x\r\n",
                      indexPD,
                      startPD.QuadPart, 
                      endPD.QuadPart,
                      offset.QuadPart);
            ioOffset.QuadPart = (startPA.QuadPart - startPD.QuadPart) + offset.QuadPart + Context->DDRFileOffset.QuadPart;
            break;
        }

        offset.QuadPart += lengthPD.QuadPart;
    }

    if (!foundDescriptor) {
        LogLibInfoPrintf(L"Failed to locate physical memory descriptor.\r\n");
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // If the value is non-zero, we found a valid memory descriptor.
    // Its offset is already computed, need to perform the IO.
    //
    if (0 != ioOffset.QuadPart)
    {
        LogLibInfoPrintf(L"IO offset is: 0x%I64x\r\n", ioOffset.QuadPart);
        status = NtWriteFile(Context->DedicatedDumpHandle,
                             NULL,
                             NULL,
                             NULL,
                             &statusBlock,
                             Buffer,
                             Size,
                             &ioOffset,
                             NULL);
        if (!NT_SUCCESS(status))
        {
            LogLibInfoPrintf(L"Failed to write to dump file at offset 0x%I64x for %u bytes.\r\n",
                             ioOffset,
                             Size);
            goto Exit;
        }

        NtFlushBuffersFile(Context->DedicatedDumpHandle, &statusBlock);
        status = STATUS_SUCCESS;
    }

Exit:
    LogLibInfoPrintf(L"Context: 0x%I64x\r\n", &(Context->DumpHeader->PhysicalMemoryBlock));

    return status;
}
#undef GET_PHYSICAL_MEMORY_DESCRIPTOR_VALUE


BOOL ReadRawDumpPartitionToFile(
    _In_ PDMP_CONTEXT    Context,
    _In_ WCHAR*            FilePath)
/*++

Routine Description:

    This function Read Raw Dump Partition to a single file.

Arguments:

    Context - Pointer to PDMP_CONTEXT
    FilePath - the bin file name
Return Value:

    TURE/FALSE

--*/
{
    CHAR            curChar = L'\\';
    BOOL            result = FALSE;
    PCHAR           buffer = nullptr;
    ULONG           ReadBufferSize = DEFAULT_DMP_BUF_SZ;
    LONGLONG        RemainingSize = Context->hDisk.GetCurrentPartitionSize();
	size_t			bWrite = 0;
	DEVICE_IO       hFile(FilePath);

    if (hFile.Open() && (hFile.GetError() != DEVICE_IO::IO_OK))
    {
        LogLibInfoPrintf(L"Could not create file %s", FilePath);
        goto EXIT;
    }

    //
    // Allocate the buffer.
    //
    buffer = (PCHAR)malloc(ReadBufferSize);
    if (buffer == nullptr)
    {
        LogLibInfoPrintf(L"Could not allocate memory for buffer ");
        goto EXIT;
    }

    while (RemainingSize > 0)
    {
        size_t bRead = 0;

        //
        // Read data from disk
        //
        ZeroMemory(buffer, ReadBufferSize);

        if (FAILED(Context->hDisk.Read(buffer, ReadBufferSize, &bRead)))
        {
            result = FALSE;
            LogLibInfoPrintf(L"Failed to Read partition! ");
            goto EXIT;
        }

        if (FAILED(hFile.Write(buffer, bRead, &bWrite)))
        {
            result = FALSE;
            LogLibInfoPrintf(L"Failed to Write to file! ");
            goto EXIT;
            
        }
         
        RemainingSize -= bRead;
        switch (curChar)
        {
            case L'|':
                curChar = L'/';
                break;

            case L'/':
                curChar = L'-';
                break;

            case L'-':
                curChar = L'\\';
                break;

            case L'\\':
                curChar = L'|';
                break;
        }

        wprintf(L"\r  %c  %llu MB written, %llu MB remainig", curChar, bWrite / SIZE_1MB, RemainingSize / SIZE_1MB);
    }
    
    result = TRUE;
    wprintf(L"\r  *  %llu bytes written, raw memory data to file completed successfully!   \r\n", bWrite);
    LogLibInfoPrintf(L"  *  %llu bytes written, raw memory data to file completed successfully!   ", bWrite);

EXIT:
    if (nullptr != buffer) {
        free (buffer);
    }

    hFile.Close();

    return result;

}

NTSTATUS
ExtractRawDumpPartitionToFiles(PDMP_CONTEXT Context )
{
    NTSTATUS    status = STATUS_UNSUCCESSFUL;
    HRESULT     result = ERROR_SUCCESS;
    
    LogLibInfoPrintf(L"=========== Raw dump is expected. Finding the dedicated partition. ===========\r\n");
    result = FindDumpPartition(Context);
    if (result != ERROR_SUCCESS){
            LogLibErrorPrintf(
            result,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Failed to Find the RAW_Dump Partition : %d\n",
            GetLastError());
            goto Exit;
    }
      
    LogLibInfoPrintf(L"=========== Found the partition. Checking if there is a valid RAW_DUMP_HEADER. ===========\r\n");

    status = ExtractRawDumpToFiles(Context);

Exit:
    return status;
}

BOOL
ExistValidOfflineDump( PDMP_CONTEXT Context)
/*++

Routine Description:

    This function validate whether there is a valid offlinedump on the disk.

Arguments:

    Context - Pointer to PDMP_CONTEXT
Return Value:

    TURE/FALSE

--*/
{
    BOOL        ValidateResult= FALSE;
    HRESULT     result = ERROR_SUCCESS;
    
    LogLibInfoPrintf(L"=========== Raw dump is expected. Finding the dedicated partition. ===========\r\n");
    if(FAILED(result = FindDumpPartition(Context)))
    {
            LogLibErrorPrintf(
            result,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Failed to Find the RAW_Dump Partition: %d\n",
            GetLastError());
    }
    else
    {
        LogLibInfoPrintf(L"=========== Found the partition. Checking if there is a valid RAW_DUMP_HEADER. ===========\r\n");
        if (SUCCEEDED(VerifyRawDumpHeader(Context, &ValidateResult)))
        {
            ValidateResult = TRUE;
        }

    }

    return ValidateResult;
}

BOOL
WipeRawDumpHeader( PDMP_CONTEXT Context)
/*++

Routine Description:

    This function will wipe the Raw Dump Partition header.

Arguments:

    Context - Pointer to PDMP_CONTEXT
Return Value:

    TURE/FALSE

--*/
{
    BOOL    ret = FALSE;
    PCHAR   buffer = nullptr;

    LogLibInfoPrintf(L"=========== Raw dump is expected. Finding the dedicated partition. ===========\r\n");
    if (ERROR_SUCCESS != FindDumpPartition(Context))
    {
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Failed to Find the RAW_Dump Partition: %d\n",
            GetLastError());
            goto Exit;
    }
   
    LogLibInfoPrintf(L"=========== Found the Raw Dump header ===========\r\n");
    buffer = (PCHAR) malloc( DEFUALT_SECTOR_SZ );
    if (buffer == nullptr)
    {
        LogLibInfoPrintf(L"Failed to allocate temporary buffer for disk wipe\r\n");
        goto Exit;
    }

    ZeroMemory( buffer,  DEFUALT_SECTOR_SZ );
    if (FAILED(Context->hDisk.Write(buffer, DEFUALT_SECTOR_SZ, NULL)))
    {
        LogLibInfoPrintf(L"Failed to wipe dump header, Error: %ld\r\n", Context->hDisk.GetError());
        goto Exit;
    }

    ret = TRUE;
    LogLibInfoPrintf(L"=========== Wipped the Raw Dump header ===========\r\n");

Exit:
    if (nullptr != buffer)
    {
        free(buffer);
    }

    return ret;

}

NTSTATUS
ExtractRawDumpFileToFiles(
            PDMP_CONTEXT Context, 
            LPCWSTR FileName )
{
    NTSTATUS status = E_FAIL;

    if (FAILED(Context->hDisk.Open(FileName)))
    {
        LogLibErrorPrintf(
                            E_FAIL,
                            __LINE__,
                            WIDEN(__FUNCTION__),
                            __WFILE__,
                            L"Error: Failed to Open Raw Dump device \n");
        goto Exit;
    }

    status = ExtractRawDumpToFiles(Context);

Exit:
    return status;
}

NTSTATUS
ExtractRawDumpToFiles(PDMP_CONTEXT Context )
{
    NTSTATUS   status = STATUS_UNSUCCESSFUL;
    BOOL        ValidateResult= FALSE;

    LogLibInfoPrintf(L"=========== Found the partition. Checking if there is a valid RAW_DUMP_HEADER. ===========\r\n");
    if (FAILED(VerifyRawDumpHeader(Context, &ValidateResult)) || (ValidateResult == FALSE))
    {
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Raw Dump Partition header is invalid \n");
            goto Exit;
    }

    LogLibInfoPrintf(L"=========== Found a valid dump header. Validating the section table. ===========\r\n");
    VerifyRawDumpSectionTable(Context, &ValidateResult);
      
    if (ValidateResult == FALSE) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Raw Dump Partition Section table is invalid \n");
        goto Exit;
    }

    LogLibInfoPrintf(L"=========== Got a valid section table. Populating Bugcheck data from file. ===========\r\n");
    status = UpdateContextFromEmbeddedDeviceInfo(Context);
    if (FAILED(status))
    {
        LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L"Error: Failed to populate Bugcheck data \n");
        goto Exit;
    }

    LogLibInfoPrintf(L"=========== Got a valid section table. Building a memory map based on DDR sections. ===========\r\n");
    status = BuildDDRMemoryMap(Context);
    if ( FAILED(status)) {
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Failed to Build DDR Memory Map \n");
            goto Exit;
    }
     
 
    LogLibInfoPrintf(L"=========== Write DDR sections to file ===========\r\n");
    status = WriteRAWDDRToBinAndSearchHeaders(Context, Context->DumpDDR);
    if ( FAILED(status)) {
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Failed to Write DDR to files \n");
            goto Exit;
    }
    
     if(Context->isAPREG64)
     {
         HRESULT     hr;

        //
        // 64 bit APREG format.
        //
        LogLibInfoPrintf(L"=========== This is a 64 bit APREG format ===========\r\n");
        if(Context->APRegAddress.QuadPart == 0) {
            LogLibInfoPrintf(L"=========== Trying to get APREG Address from OCIMEM ===========\r\n");
            hr = GetAPREG64AddrFromOCIMEMSection(Context);
            if ( FAILED(hr)) {
                LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__, 
                L"Error: Failed to get APREG Address from OCIMEM section \n");
                goto Exit;
            }
            LogLibInfoPrintf(L" Got APREG from OCIMEM\r\n");
        }     
        LogLibInfoPrintf(L"=========== Parsing 64 bit APREG format ===========\r\n");        
        hr = FindAndParseAPREG64(Context, &ValidateResult);
        if ( FAILED(hr)) {
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Failed to Parse APREG sections \n");
            goto Exit;
        }
    }
    
    LogLibInfoPrintf(L"=========== Writing Silicon Specific data to bins ===========\r\n");
    status = WriteSVSpecific(Context);
    if ( FAILED(status)) {
        LogLibErrorPrintf(
        E_FAIL,
        __LINE__,
        WIDEN(__FUNCTION__),
        __WFILE__, 
        L"Error: Failed WriteSVSpecific \n");
        goto Exit;
    }

    if (Context->Is64Bit){
        status = ExtractWindowsDumpFile64(Context);
    }
    else {
        status = ExtractWindowsDumpFile32(Context);
    }

Exit:
    return status;
}

BOOL MergeFile(HANDLE hTo, HANDLE hFrom)
{
    BOOL     fRet = FALSE;
    DWORD    bufferSize = DEFAULT_DMP_BUF_SZ;
    PBYTE    pbBuf = NULL;
    DWORD    cbDone;
    LONG     counter = 0;
    DWORD    bytesRead = 0;

   
    pbBuf = (PBYTE) malloc(bufferSize);

    if (NULL == pbBuf){
        LogLibErrorPrintf(
                        E_FAIL,
                        __LINE__,
                        WIDEN(__FUNCTION__),
                        __WFILE__, 
                        L"Error: Failed to allocate buffer");
        goto Exit;
    }

    SetFilePointer(hTo, 0, 0, FILE_END);
    for (int i = 0; ;i++) {
        if (!ReadFile(hFrom, pbBuf, bufferSize, &cbDone, NULL)) {
            LogLibInfoPrintf(L"[ERROR] read the input file failed.\n");
            goto Exit;
        }
        if (cbDone == 0) {
             LogLibInfoPrintf(L"[INFO] reached the end of file at 0X%X.\n", bytesRead);
            break;
        }
        bytesRead += cbDone;

        if (!WriteFile(hTo, pbBuf, cbDone, &cbDone, NULL)) {
            LogLibInfoPrintf(L"[ERROR] Write to merged file failed at offset 0x%x.\n", bytesRead);
            goto Exit;
        }
        counter++;
        wprintf(L"    ");
        switch (i)
        {
            case 4: 
                i = 0;
            case 0:
                wprintf(L"-");
                break;
            case 1:
                wprintf(L"\\");
                break;
            case 2:
                wprintf(L"|");
                break;
            case 3:
                wprintf(L"/");
                break;
            default:
                wprintf(L"*");
                i=3;
                break;
        }
        wprintf(L" merged %d MB.\r", counter*bufferSize / (1024 * 1024));
    }
    LogLibInfoPrintf(L"    Completed merge of %d MB.\n", counter*bufferSize / (1024 * 1024));

    fRet = TRUE;

Exit:
    if (pbBuf) {
        free(pbBuf);
    }
    return fRet;
}


BOOL MergeDDRFiles( PDMP_CONTEXT Context, 
                   PCOMMAND_LINE_ARGS arguments,
                   LPWSTR mergedfile )
{
    HANDLE hmerged = INVALID_HANDLE_VALUE;
    ULONGLONG lowestbase = 0xFFFFFFFFFFFFFFFF;
    LARGE_INTEGER filesize = { 0 };
    UINT sectionid = 0;
    UINT mergedsections = 0 ;
    ULONGLONG Gap = 0;
    HANDLE  hFrom = INVALID_HANDLE_VALUE;
    BOOL bRet = FALSE;

    Context->DDRMemoryMap = new DDR_MEMORY_MAP[arguments->DDRCount];
    ZeroMemory(Context->DDRMemoryMap, (sizeof(DDR_MEMORY_MAP)*arguments->DDRCount) );

    Context->DDRMemoryMapCount = arguments->DDRCount ;
   Context->SectionStats.DDRSectionCount = arguments->DDRCount;

//    CopyArgsToDdrMap(Context, &arguments->ddr[0], arguments->DDRCount);
//    SortDdrArgsByBase(&arguments->ddr[0], arguments->DDRCount);


#ifndef _DEBUG_NO_MERGE
    hmerged = CreateFileW( mergedfile,
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL,
                            CREATE_ALWAYS,
                            FILE_FLAG_WRITE_THROUGH,
                            NULL );

    if (hmerged == INVALID_HANDLE_VALUE){
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Create File Failed. INVALID_HANDLE_VALUE. %ls\n", mergedfile);
        goto Exit;
    }
#endif

    wprintf(L"Merge file \'%s\' successfully created (handle=0x%08x) \n\n", mergedfile, (UINT32)hmerged);


    //
    // Find the lowest DDR section for use as the base address
    //
    for( UINT index = 0; index < arguments->DDRCount; index++) {
        if (!arguments->ddr[index].AlreadyMerged) {
            if( ( (ULONGLONG)arguments->ddr[index].DDRBase.QuadPart) < lowestbase) {
                lowestbase = (ULONGLONG)arguments->ddr[index].DDRBase.QuadPart;
            }
        }
    }
    wprintf(L"Lowest DDR section; lowestbase = 0x%llx\n", lowestbase);

    // To merge the DDR sections and extract the windows dump file.
    wprintf(L"Begin DDR section Merge for %d files\n", arguments->DDRCount );
    for (mergedsections = 0; mergedsections < arguments->DDRCount; mergedsections++){
        ULONGLONG currentBaseOffset = 0xffffffffffffffff;

        //
        // Find the lowest available DDR section (base)
        //
        for( UINT index = 0; index < arguments->DDRCount; index++) {
            if (arguments->ddr[index].AlreadyMerged == TRUE) {
                continue;
            }
            else if( (ULONGLONG)arguments->ddr[index].DDRBase.QuadPart < currentBaseOffset )
            { // This will revise the value to the lowest one
                currentBaseOffset = arguments->ddr[index].DDRBase.QuadPart;
                sectionid = index;
            }
        }
        wprintf(L"Processing DDR section --> sectionid = %d, base = 0x%llx, File = %s\r\n", sectionid, currentBaseOffset, arguments->ddr[sectionid].DDRFileName);

        //
        // the identified section is the lowest DDR section. Read the file, merge it
        //
        if(mergedsections > 0) {           
            Gap = arguments->ddr[sectionid].DDRBase.QuadPart - currentBaseOffset;
        }

        if(Gap!= 0 ){
            LogLibInfoPrintf(
                L"======= The DDR sections are discontinuous --> hole = 0x%llx, DDRbase = 0x%llx, currentbase = 0x%llx ============\n",
                Gap,
                arguments->ddr[sectionid].DDRBase.QuadPart,
                currentBaseOffset);
        }

        hFrom = CreateFileW(arguments->ddr[sectionid].DDRFileName,
                            GENERIC_READ,
                            0,
                            NULL, 
                            OPEN_EXISTING,
                            0,
                            NULL);
        if (hFrom == INVALID_HANDLE_VALUE) {
                LogLibErrorPrintf(
                                E_FAIL,
                                __LINE__,
                                WIDEN(__FUNCTION__),
                                __WFILE__, 
                                L"Error: Failed to Open input file: %ls\n", arguments->ddr[sectionid].DDRFileName);
            goto Exit;
        }
        wprintf(L"  Reading DDR section %d from file: %ls\n", sectionid, arguments->ddr[sectionid].DDRFileName);
            
        if(!GetFileSizeEx(hFrom, &filesize)) {
            LogLibErrorPrintf(
                    E_FAIL,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__, 
                    L"Error: Failed to Get File Size \n");
            goto Exit;
        }
        wprintf(L"    size of file 0x%llx (%lld)\n", filesize, filesize);

#ifndef _DEBUG_NO_MERGE
        if (!MergeFile(hmerged, hFrom)) {
                LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to merge File %s\n", arguments->ddr[sectionid].DDRFileName);
                goto Exit;
        }
#endif
        Context->DDRMemoryMap[mergedsections].Base = arguments->ddr[sectionid].DDRBase.QuadPart;
        Context->DDRMemoryMap[mergedsections].Size = filesize.QuadPart;

        if( currentBaseOffset >= lowestbase ) {
            Context->DDRMemoryMap[mergedsections].Offset = currentBaseOffset - lowestbase;
        }
        else {
            Context->DDRMemoryMap[mergedsections].Offset = currentBaseOffset;
        }

        Context->DDRMemoryMap[mergedsections].End = arguments->ddr[sectionid].DDRBase.QuadPart + filesize.QuadPart - 1;
        Context->DDRMemoryMap[mergedsections].Contiguous = TRUE;
        Context->SectionStats.TotalDDRSizeInBytes += filesize.QuadPart;

        currentBaseOffset = arguments->ddr[sectionid].DDRBase.QuadPart + filesize.QuadPart;

        arguments->ddr[sectionid].AlreadyMerged = TRUE;

        if (hFrom != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFrom);
        }
        
        //merged successfully 
        if (hmerged != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(hmerged);
        }

        LogLibInfoPrintf(L"         Context->DDRMemoryMap[%03d].Base = 0x%I64x", mergedsections, Context->DDRMemoryMap[mergedsections].Base);
        LogLibInfoPrintf(L"          Context->DDRMemoryMap[%03d].End = 0x%I64x", mergedsections, Context->DDRMemoryMap[mergedsections].End );
        LogLibInfoPrintf(L"         Context->DDRMemoryMap[%03d].Size = 0x%I64x", mergedsections, Context->DDRMemoryMap[mergedsections].Size);
        LogLibInfoPrintf(L"   Context->DDRMemoryMap[%03d].Contiguous = 0x%ls",   mergedsections, (Context->DDRMemoryMap[mergedsections].Contiguous?L"TRUE":L"FALSE") );
        LogLibInfoPrintf(L"                        currentBaseOffset = 0x%I64x", currentBaseOffset);
        LogLibInfoPrintf(L"       Context->DDRMemoryMap[%03d].Offset = 0x%I64x", mergedsections, Context->DDRMemoryMap[mergedsections].Offset);
        LogLibInfoPrintf(L"                        filesize.QuadPart = 0x%I64x", filesize.QuadPart);
    }
    
    if (hmerged != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hmerged);
    }
    bRet = TRUE;

Exit:
    return bRet;
}

NTSTATUS
ExtractWindowsDumpFromDDR( PDMP_CONTEXT Context, 
                           LPWSTR FileName
                          )
{
    NTSTATUS   status = STATUS_SUCCESS;
    BOOL        ValidateResult= FALSE;
    HRESULT     hr;

    Context->DDRFileOffset.QuadPart = 0;
    Context->SectionStats.CPUContextSectionCount = 1;
    Context->SectionStats.SVSectionCount = 0;
    Context->SectionStats.TotalSVSpecificSizeInBytes = 0;
    Context->DumpHeaderAddress.QuadPart = 0;
    Context->DumpHeader = NULL;
    Context->ApReg = NULL;


    wprintf(L"Opening disk \'%s\' for read access\n", FileName);
    if (FAILED(hr = Context->hDisk.Open(FileName)))
    {
        LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__, 
                L"Error: Open disk failed\n");
        goto Exit;
    }

    wprintf(L"  disk size = 0x%llx (%lld)\n", Context->hDisk.GetCurrentFileSize(), Context->hDisk.GetCurrentFileSize());
    if (0 == Context->hDisk.GetCurrentFileSize())
    {
        LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__, 
                L"Error: RawDumpPartitionLength is invalid \n");
        goto Exit;
    }

    LogLibInfoPrintf(L" Try searching windows dump header and other sections. ===========\r\n");
    if (FAILED(hr = SearchDumpHeaderAndAPREG(Context)))
    {
        LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L"Error: Failed get windows dump header, no need continue!\n");
        goto Exit;
        // Dont go to exit as of now
    }

    if(Context->isAPREG64) {
        //
        // 64 bit APREG format.
        // If APREG64 Address is provided in the command line, do not try to find the address 
        // from OCIMEM section.
        //
        if(Context->APRegAddress.QuadPart == 0) {
            LogLibInfoPrintf(L"=========== Trying to get APREG Address from OCIMEM ===========\r\n");
            hr = GetAPREG64AddrFromOCIMEMSection(Context);
            if ( FAILED(hr)) {
                LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__, 
                L"Error: Failed to get APREG Address from OCIMEM section \n");
                goto Exit;
            }
            LogLibInfoPrintf(L" Got APREG from OCIMEM\r\n");
        }       
        LogLibInfoPrintf(L"=========== Parsing 64 bit APREG format ===========\r\n");        
        hr = FindAndParseAPREG64(Context, &ValidateResult);
        if ( FAILED(hr)) {
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error: Failed to Parse APREG sections \n");
            goto Exit;
        }
    }
    else if(Context->IsAPREGRequested && Context->HasValidAP_REG) {
    
        // This code path is for 32 bit (legacy) APREG format.        
        LogLibInfoPrintf(L" ============ Try getting AP_REG. (Legacy) ===========\r\n");
        status = GetAPReg(Context, &ValidateResult);
        if (!NT_SUCCESS(status)) {
            LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, 
            L"Error:Failed to read AP_REG \n");
            goto Exit;
        }
    }

    //
    //
    // Extract Windows dumps
    //
    if (Context->Is64Bit){
        status = ExtractWindowsDumpFile64(Context);
    }
    else {
        status = ExtractWindowsDumpFile32(Context);
    }

Exit:
    return status;
}

HRESULT UpdateContextFromEmbeddedDeviceInfo(_Inout_ PDMP_CONTEXT Context)
{
    HRESULT ret = S_OK;

    DEVICE_SPECIFIC_INFO DeviceSpecificInfo;

    if (FAILED(ret = ReadDeviceSpecificInfo(&Context->hDisk, &DeviceSpecificInfo, (Context->hDisk.GetCurrentFileSize() - DEVICE_SPECIFIC_INFO_BUFFER_LENGTH))))
    {
        LogLibInfoPrintf(L"WARNING: UpdateContextFromEmbedDeviceInfo() Failed to read Device specific info data, using defaults.");
        Context->BugCheckCode = FATAL_ABNORMAL_RESET_ERROR;
        Context->BugCheckParam1 = (UINT64)ONEFOURC_PARAM1_DEFAULT;
        Context->BugCheckParam2 = (UINT64)ONEFOURC_PARAM2_DEFAULT;
        Context->BugCheckParam3 = (UINT64)ONEFOURC_PARAM3_DEFAULT;
        Context->BugCheckParam4 = (UINT64)ONEFOURC_PARAM4_DEFAULT;
        ret = S_OK;
    }
    else
    {
        Context->DumpInstance.QuadPart = DeviceSpecificInfo.DumpHeaderInstanceID;
        Context->BugCheckCode = DeviceSpecificInfo.BugCheckCode;
        Context->BugCheckParam1 = DeviceSpecificInfo.BugCheckParam1;
        Context->BugCheckParam2 = DeviceSpecificInfo.BugCheckParam2;
        Context->BugCheckParam3 = DeviceSpecificInfo.BugCheckParam3;
        Context->BugCheckParam4 = DeviceSpecificInfo.BugCheckParam4;

        switch (DeviceSpecificInfo.Type)
        {
            case DEVICE_TYPE_INTELx86:          // Retained for backward compatibility
            case PROCESSOR_ARCHITECTURE_INTEL:
                Context->CPUContextAddress.QuadPart = DeviceSpecificInfo.CpuContextAddress;
                break;

            case DEVICE_TYPE_QCOM32:            // Retained for backward compatibility
            case PROCESSOR_ARCHITECTURE_ARM:
            case PROCESSOR_ARCHITECTURE_ARM64:
                Context->APRegAddress.QuadPart = DeviceSpecificInfo.APRegPA;
                Context->InMemDataInfo.DataVA = (PVOID)DeviceSpecificInfo.VA;
                Context->InMemDataInfo.DataPA.QuadPart = DeviceSpecificInfo.PA;
                Context->InMemDataInfo.Size = DeviceSpecificInfo.Size;
                break;

            default:
                ret = E_FAIL;
                LogLibErrorPrintf(E_FAIL,
                                  __LINE__,
                                  WIDEN(__FUNCTION__),
                                  __WFILE__,
                                  L"UpdateContextFromEmbedDeviceInfo: Unknown Device Id");
                break;
        }

    }

    return ret;
}

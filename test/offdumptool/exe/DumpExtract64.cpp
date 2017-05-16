/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   dumpextract64.cpp

Environment:
   User Mode

Author:
   radutta 
--*/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>
#include "common.h"
#include "DbgClient.h"
#include "DiskUtil.h"
#include "DumpUtil.h"
#include "apreg64.h"

NTSTATUS
GetDumpHeader64(
_Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function uses variable services to get the OfflineMemoryDumpOsData value, retrieve the
DUMP_HEADER64, and validate that the header is good.

The following items are checked.
1. DUMP_HEADER64.Signature
2. DUMP_HEADER64.ValidDump

Arguments:

Context - Dmp_CONTEXT

IsHeaderValid - Informs the caller whether to trust the header or not.

Return Value:

NT status code.

--*/
{
    NTSTATUS      status = STATUS_UNSUCCESSFUL;

    if (0 == Context->DumpHeaderAddress.QuadPart) {
        LogLibInfoPrintf(L"DumpHeader64 adress is invalid!, address is zero.");
        goto Exit;
    }

    LogLibInfoPrintf(L"DumpHeader64 address is 0x%llx\r\n", Context->DumpHeaderAddress.QuadPart);

    //
    // Allocate a buffer to store the dump header. 
    //
    Context->DumpHeader64 = (PDUMP_HEADER64)malloc(sizeof(DUMP_HEADER64));
    if (nullptr == Context->DumpHeader64) {
        LogLibInfoPrintf(L"Failed to allocate memory for DUMP_HEADER64, requested size = 0x%llx\r\n",
                         sizeof(DUMP_HEADER64));
        status = STATUS_NO_MEMORY;
        goto Exit;
    }

    ZeroMemory(Context->DumpHeader64, sizeof(DUMP_HEADER64));
    if (FAILED(Context->hDisk.ReadAtOffset((PCHAR)Context->DumpHeader64, sizeof(DUMP_HEADER64), Context->DumpHeaderAddress, DEVICE_IO::READ_EXACT)))
    {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__, L"Failed to read pre-built Dump header, Error : 0x%llx\r\n", Context->hDisk.GetError());
        goto Exit;
    }

    if (Context->DumpHeader64->Signature != DUMP_SIGNATURE64){
        LogLibInfoPrintf(L"Invalid DUMP_HEADER64.Signature. Expected: 0x%llx Actual:0x%llx\r\n",
                         DUMP_SIGNATURE64,
                         Context->DumpHeader64->Signature);
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    if (Context->DumpHeader64->ValidDump != DUMP_VALID_DUMP64){
        LogLibInfoPrintf(L"Invalid DUMP_HEADER64.ValidDump. Expected: 0x%llx Actual:0x%llx\r\n",
                         DUMP_VALID_DUMP64,
                         Context->DumpHeader64->ValidDump);
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    // Copy the BugCheck Data, from the dumpHeader to context
    UPDATE_IF_ZERO(Context->DumpHeader64->BugCheckCode, (ULONG)Context->BugCheckCode);
    UPDATE_IF_ZERO(Context->DumpHeader64->BugCheckParameter1, (ULONG)Context->BugCheckParam1);
    UPDATE_IF_ZERO(Context->DumpHeader64->BugCheckParameter2, (ULONG)Context->BugCheckParam2);
    UPDATE_IF_ZERO(Context->DumpHeader64->BugCheckParameter3, (ULONG)Context->BugCheckParam3);
    UPDATE_IF_ZERO(Context->DumpHeader64->BugCheckParameter4, (ULONG)Context->BugCheckParam4);

    status = STATUS_SUCCESS;

Exit:

    return status;
}


NTSTATUS
ValidateDDRAgaisntPhysicalMemoryBlock64(
_Inout_ PDMP_CONTEXT Context
)
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
    UINT64                          endPD;
    UINT32                          indexDDR;
    UINT32                          indexPD;
    UINT64                          startPD;
    UINT32                          spanCount;
    NTSTATUS                        status = STATUS_UNSUCCESSFUL;
    BOOLEAN                         startInSection;
    BOOLEAN                         endInSection;

    PDDR_MEMORY_MAP                 ddrMemoryMap = Context->DDRMemoryMap;

    PPHYSICAL_MEMORY_DESCRIPTOR64   physDesc = (PPHYSICAL_MEMORY_DESCRIPTOR64)(&(Context->DumpHeader64->PhysicalMemoryBlock));

    UINT64                          memorySizeFromPD = physDesc->NumberOfPages * PAGE_SIZE;

    Context->SizeAccordingToMemoryDescriptors = memorySizeFromPD;

    if (memorySizeFromPD > Context->SectionStats.TotalDDRSizeInBytes) {
        LogLibInfoPrintf(L"Memory size from memory block (0x%p) is larger than DDR size (0x%p)\r\n",
            memorySizeFromPD,
            Context->SectionStats.TotalDDRSizeInBytes);
        status = STATUS_BAD_DATA;
        //goto Exit;
    }

    for (indexPD = 0; indexPD < physDesc->NumberOfRuns; indexPD++) {
        startPD = (UINT64)(physDesc->Run[indexPD].BasePage * PAGE_SIZE);
        endPD = startPD + (UINT64)(physDesc->Run[indexPD].PageCount * PAGE_SIZE) - 1;
        spanCount = 0;
        startInSection = FALSE;
        endInSection = FALSE;

        LogLibInfoPrintf(L"Looking at descriptor 0x%llx\r\n", indexPD);

        for (indexDDR = 0; indexDDR <Context->SectionStats.DDRSectionCount; indexDDR++) {
            if ((spanCount > 0) &&
                (!ddrMemoryMap[indexDDR].Contiguous))
            {
                //
                // We got a problem. We got a run that spans 
                // DDR sections but we are hitting a DDR section 
                // which don't have memory that's contiguous with 
                // previous. 
                //
                LogLibInfoPrintf(L"Error verifying memory run. Run is spanning discontiguous DDR sections.\r\n");
                LogLibInfoPrintf(L"Run Index: 0x%llx\r\n");
                LogLibInfoPrintf(L"DDR Index: 0x%llx\r\n", indexDDR);
                LogLibInfoPrintf(L"Previous DDR Index: 0x%llx\r\n", (indexDDR - 1));

                status = STATUS_BAD_DATA;
                goto Exit;
            }

            //
            // Is startPD in this DDR section?
            //
            if ((startPD >= ddrMemoryMap[indexDDR].Base) &&
                (startPD <= ddrMemoryMap[indexDDR].End))
            {
                startInSection = TRUE;
                //
                // Does endPD also fall into this section?
                //
                if ((endPD >= ddrMemoryMap[indexDDR].Base) &&
                    (endPD <= ddrMemoryMap[indexDDR].End))
                {
                    //
                    // Range is within this section. 
                    //
                    endInSection = TRUE;
                    break;
                }
                else
                {
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
        if (!startInSection)
        {
            LogLibInfoPrintf(L"The start (0x%I64x) of run 0x%llx does not fall into any DDR section.\r\n",
                (UINT64)(physDesc->Run[indexPD].BasePage * PAGE_SIZE),
                indexPD);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        if (!endInSection)
        {
            LogLibInfoPrintf(L"The end (0x%I64x) of run 0x%llx does not fall into any DDR section.\r\n",
                endPD,
                indexPD);
            status = STATUS_BAD_DATA;
            goto Exit;
        }

        LogLibInfoPrintf(L"Descriptor 0x%llx checked out.\r\n", indexPD);
    }//indexPD

    status = STATUS_SUCCESS;
Exit:
    return status;
}


NTSTATUS
WriteDumpHeader64(
_Inout_ PDMP_CONTEXT Context
)
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
    IO_STATUS_BLOCK         statusBlock = { 0 };
    LARGE_INTEGER           dedicatedDumpFileOffset = { 0 };

    Context->DedicatedDumpFileOffset.QuadPart = 0;
    
    //
    // Update the DUMP_HEADER64
    //
    LogLibInfoPrintf(L"Updating DUMP_HEADER64 before it is written to dump.\r\n");
    
    LogLibInfoPrintf(L"Changing value of \"Context->DumpHeader64->RequiredDumpSpace\" from 0x%llx to 0x%llx\n", Context->DumpHeader64->RequiredDumpSpace, Context->ActualDumpFileUsedInBytes);
    Context->DumpHeader64->RequiredDumpSpace = Context->ActualDumpFileUsedInBytes;

    if ((Context->SectionStats.SVSectionCount > 0) || (Context->SectionStats.CPUContextSectionCount > 0)) {
        LogLibInfoPrintf(L"Changing value of \"Context->DumpHeader64->SecondaryDataState\" from 0x%llx to 0x%llx\n", Context->DumpHeader64->SecondaryDataState, STATUS_SUCCESS);
        Context->DumpHeader64->SecondaryDataState = STATUS_SUCCESS;
    }

    //
    // Write the header to dump.
    //
    dataSize = sizeof(DUMP_HEADER64);
    dedicatedDumpFileOffset.QuadPart = Context->DedicatedDumpFileOffset.QuadPart;

    LogLibInfoPrintf(L"   dataSize=0x%x (%d)", dataSize, dataSize);
    LogLibInfoPrintf(L"   dedicatedDumpFileOffset=0x%x", dedicatedDumpFileOffset);
    LogLibInfoPrintf(L"   dedicatedDumpFileOffset.QuadPart=0x%x (%d)", dedicatedDumpFileOffset.QuadPart, dedicatedDumpFileOffset.QuadPart);

    status = NtWriteFile(Context->DedicatedDumpHandle,
        NULL,
        NULL,
        NULL,
        &statusBlock,
        Context->DumpHeader64,
        dataSize,
        &dedicatedDumpFileOffset,
        NULL);
    LogLibInfoPrintf(L"   NtWriteFile() returned status 0x%x (%d)", status, status);

    if (!NT_SUCCESS(status)) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error:  Failed to write DumpHeader64%d \n",
            GetLastError());
        goto Exit;
    }

    NtFlushBuffersFile(Context->DedicatedDumpHandle, &statusBlock);

    Context->DedicatedDumpFileOffset.QuadPart += dataSize;

    //
    // Save the starting offset for DDR for easier future access.
    //
    Context->DDRFileOffset = Context->DedicatedDumpFileOffset;

    status = STATUS_SUCCESS;

Exit:

    return status;
}

NTSTATUS
WriteDDR64(
_Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function writes DDR sections based on the DUMP_HEADER64's physical memory descriptor
to dump file.

Arguments:

Context - Pointer to the global context structure.

Return Value:

NT status code.

--*/
{
    LARGE_INTEGER   basePA = { 0 };
    ULONG64         PageRemain = 0;
    LARGE_INTEGER   bytesWritten = { 0 };
    UINT32          io = 0;
    UINT32          ioCountPerRun = 0;
    LARGE_INTEGER   runSize = { 0 };
    LARGE_INTEGER   startPA = { 0 };
    UINT32          ioSize = 0;
    ULONG           buffersize = DEFAULT_DMP_BUF_SZ;
    NTSTATUS        status = STATUS_UNSUCCESSFUL;
    IO_STATUS_BLOCK statusBlock = { 0 };
    LARGE_INTEGER   dedicatedDumpFileOffset = { 0 };


    //
    // Allocate the intermediate buffer to read memory from DDR section
    // to the dump file.
    //
    PVOID           tempBuffer = malloc(buffersize);
    if (tempBuffer == nullptr) {
        LogLibInfoPrintf(L"Unable to allocate 0x%llx bytes buffer for writing DDR memory to dump.\r\n", buffersize);
        status = STATUS_NO_MEMORY;
        goto Exit;
    }
    LogLibInfoPrintf(L"[INFO] Successfully allocated 0x%llx bytes of buffer to write DDR memory to dump.\r\n", buffersize);

    ZeroMemory(tempBuffer, buffersize);

    for (UINT32 index = 0; index < Context->DumpHeader64->PhysicalMemoryBlock.NumberOfRuns; index++)  {
        LogLibInfoPrintf(L"=== === === ===  Run %d  === === ===", index);

        basePA.QuadPart = (Context->DumpHeader64->PhysicalMemoryBlock.Run[index].BasePage * PAGE_SIZE);
        PageRemain = Context->DumpHeader64->PhysicalMemoryBlock.Run[index].PageCount;
        runSize.QuadPart = (Context->DumpHeader64->PhysicalMemoryBlock.Run[index].PageCount *PAGE_SIZE);
        ioCountPerRun = (UINT32)(runSize.QuadPart / buffersize);

        if ((runSize.QuadPart % buffersize) > 0) {
            ioCountPerRun++;
        }

        LogLibInfoPrintf(L"0x%x IOs will be required for this run.", ioCountPerRun);
        LogLibInfoPrintf(L"     basePA.QuadPart=0x%llx",  basePA.QuadPart );
        LogLibInfoPrintf(L"          PageRemain=0x%08x",  PageRemain );
        LogLibInfoPrintf(L"    runSize.QuadPart=0x%08x",  runSize.QuadPart );
        LogLibInfoPrintf(L"       ioCountPerRun=0x%08x",  ioCountPerRun );

        for (io = 0; io < ioCountPerRun; io++) {
            startPA.QuadPart = basePA.QuadPart + io * ioSize;

            ioSize = (UINT32)((PageRemain * PAGE_SIZE)< buffersize ? PageRemain * PAGE_SIZE : buffersize);
            LogLibInfoPrintf(L"  -- I/O 0x%08x: startPA.QuadPart=0x%016x, ioSize=0x%016x", startPA.QuadPart, ioSize );

            status = ReadFromDDRSectionByPhysicalAddress(Context,
                                                        startPA,
                                                        ioSize,
                                                        tempBuffer);
            if (!NT_SUCCESS(status)) {
                LogLibInfoPrintf(L"[0x%llx] Failed to read from DDR sections. Status: 0x%llx\r\n",
                    io,
                    status);
                goto Exit;
            }

            dedicatedDumpFileOffset.QuadPart = Context->DedicatedDumpFileOffset.QuadPart;
            status = NtWriteFile(Context->DedicatedDumpHandle,
                NULL,
                NULL,
                NULL,
                &statusBlock,
                tempBuffer,
                ioSize,
                &dedicatedDumpFileOffset,
                NULL);
            if (!NT_SUCCESS(status)) {
                LogLibInfoPrintf(L"Failed to Write to file! ");
                goto Exit;

            }
            NtFlushBuffersFile(Context->DedicatedDumpHandle, &statusBlock);
            bytesWritten.QuadPart += ioSize;
            Context->DedicatedDumpFileOffset.QuadPart += ioSize;
            PageRemain -= ioSize / PAGE_SIZE;
            LogLibInfoPrintf(L"                  bytesWritten=0x%llx, Context->DedicatedDumpFileOffset.QuadPart=0x%lld, 0x%llx Pages remain\n", bytesWritten, Context->DedicatedDumpFileOffset.QuadPart, PageRemain);
        }// for io

        LogLibInfoPrintf(L"**** Finished writing run 0x%llx. ****\r\n", index);
    }//for index

    if ((ULONG64)(bytesWritten.QuadPart / PAGE_SIZE) != Context->DumpHeader64->PhysicalMemoryBlock.NumberOfPages) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Wrote Page not match as expected\n");
        goto Exit;
    }
    status = STATUS_SUCCESS;

Exit:

    if (tempBuffer != nullptr) {
        free(tempBuffer);
        tempBuffer = nullptr;
    }

    return status;
}

NTSTATUS
GetKdDebuggerDataBlock64(
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
    NTSTATUS                    status = STATUS_UNSUCCESSFUL;

    if ((nullptr != Context) && (nullptr != Context->DumpHeader64))
    {
        UINT32                      dataSize = sizeof(KDDEBUGGER_DATA64);
        PKDDEBUGGER_DATA64          dbgDataBlock = nullptr;
        LARGE_INTEGER               dbgDataHeaderPA;
        LARGE_INTEGER               decodedBlockPA;
        ULONG64                     dbgDataHeaderVA = Context->DumpHeader64->KdDebuggerDataBlock;

        LogLibInfoPrintf(L"Checking DBGKD_DEBUG_DATA_HEADER64.Size\r\n");
        Context->KdDebuggerDataBlockPA.QuadPart = ADDRESS_NOT_PRESENT;

        //
        // Read DBGKD_DEBUG_DATA_HEADER64
        // 
        dbgDataBlock = (PKDDEBUGGER_DATA64)malloc(dataSize);
        if (nullptr == dbgDataBlock)
        {
            LogLibInfoPrintf(L"Failed to allocated 0x%llx bytes for KdDebuggerDataBlock\r\n", dataSize);
            status = STATUS_NO_MEMORY;
            goto Exit;
        }

        ZeroMemory(dbgDataBlock, dataSize);
        status = ReadFromDDRSectionByVirtualAddress64(Context,
                                                      dbgDataHeaderVA,
                                                      dataSize,
                                                      dbgDataBlock,
                                                      &dbgDataHeaderPA);
        if (!NT_SUCCESS(status))
        {
            LogLibInfoPrintf(L"Failed to read the address for CONTEXT. VA: 0x%x Status: 0x%x\r\n",
                             dbgDataHeaderVA,
                             status);
            goto Exit;
        }

        //
        // Validate the debugger data block
        //
        if (!ValidateKdDebuggerDataBlock(&(dbgDataBlock->Header)))
        {
            LogLibInfoPrintf(L"The KdDebuggerDataBlock appears to be encoded \r\n");

            //
            // As ARM platform DumperHeader size is PageSize so 
            // directly use PAGE_SIZE
            //
            decodedBlockPA.QuadPart = Context->DumpHeaderAddress.QuadPart + PAGE_SIZE;
            LogLibInfoPrintf(L"Decoded version of KdDebuggerDataBlock should be at 0x%I64x \r\n", decodedBlockPA.QuadPart);

            //directly read to buffer
            if (FAILED(Context->hDisk.ReadAtOffset((PCHAR)dbgDataBlock, dataSize, decodedBlockPA, DEVICE_IO::READ_EXACT)))
            {
                status = STATUS_UNSUCCESSFUL;
                LogLibInfoPrintf(L"Read Decoded Debug Block failed. ERROR:0x%x\r\n", Context->hDisk.GetError());
                goto Exit;
            }
        }
        else
        {
            LogLibInfoPrintf(L"DBGKD_DEBUG_DATA_HEADER64.Size is: 0x%x\r\n", dbgDataBlock->Header.Size);
            LogLibInfoPrintf(L"KdDebuggerDataBlock appears to be valid and decoded. Read it in to memory.\r\n");
        }

        Context->KdDebuggerDataBlock = dbgDataBlock;
        Context->KdDebuggerDataBlockPA.QuadPart = dbgDataHeaderPA.QuadPart;
        if (!ValidateKdDebuggerDataBlock(&(Context->KdDebuggerDataBlock->Header)))
        {
            status = STATUS_UNSUCCESSFUL;
            LogLibInfoPrintf(L"The KdDebuggerDataBlock is not valid.\r\n");
            goto Exit;
        }
        else
        {
            LogLibInfoPrintf(L"KdDebuggerDataBlock has been read into memory.\r\n");
        }

        LogLibInfoPrintf(L"KdDebuggerDataBlock - updating bugcheck parameters.\r\n");
        if (0 != Context->KdDebuggerDataBlock->KiBugcheckData)
        { // Update the KdDebuggerDataBlock with the Bugcheck data
            UINT64                      dbgBugcheckVA = (UINT64)Context->KdDebuggerDataBlock->KiBugcheckData;
            UINT                        dbgBugcheckDataBlock[BUGCHECK_ARRAY_SIZE];
            LARGE_INTEGER               dbgBugcheckPA = { 0 };

            status = ReadFromDDRSectionByVirtualAddress64(
                Context,
                dbgBugcheckVA,
                DBG_BUGCHECK_SIZE,
                dbgBugcheckDataBlock,
                &dbgBugcheckPA);
            if (!NT_SUCCESS(status))
            {
                LogLibInfoPrintf(L"Read KiBugcheckData failed. Status:0x%x\r\n", status);
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
                LogLibInfoPrintf(L"Failed to write the KiBugcheckData to dump. Status: 0x%x\r\n", status);
                goto Exit;
            }
            else
            {
                LogLibInfoPrintf(L"KiBugcheckData - Written successfully to dump.\r\n");
            }

        }

        status = WriteToDumpByPhysicalAddress(Context,
                                              Context->KdDebuggerDataBlockPA,
                                              dataSize,
                                              Context->KdDebuggerDataBlock);

        if (!NT_SUCCESS(status))
        {
            LogLibInfoPrintf(L"Failed to write the decoded copy of KdDebuggerData to dump. Status: 0x%x\r\n", status);
            goto Exit;
        }
        else
        {
            LogLibInfoPrintf(L"KdDebuggerDataBlock - Writing decoded copy to dump.\r\n");
        }

        status = STATUS_SUCCESS;
        LogLibInfoPrintf(L"KdDebuggerDataBlock Written to 0x%I64x Size:%d been read into memory.\r\n", Context->KdDebuggerDataBlockPA, dataSize);
    }

Exit:
    return status;
}

NTSTATUS
ReadFromDDRSectionByVirtualAddress64(
_In_ PDMP_CONTEXT Context,
_In_ UINT64 VirtualAddress,
_In_ UINT64 Length,
_Out_ PVOID Buffer,
_Out_opt_ PLARGE_INTEGER PhysicalAddress
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
    LARGE_INTEGER       physicalAddress;
    NTSTATUS            status = STATUS_UNSUCCESSFUL;

    //
    // Validate if we are crossing page boundary. If we are
    // return STATUS_INVALID_PARAMETER. 
    //
    nextPageVA = VirtualAddress + 0x1000;
    endVA = VirtualAddress + Length - 1;

    if (PhysicalAddress != NULL) {
        PhysicalAddress->QuadPart = ADDRESS_NOT_PRESENT;
    }

    if (endVA >= nextPageVA) {
        LogLibInfoPrintf(L"The read crosses page boundary. EndVA: 0x%llx\r\n",
            endVA);
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    //
    // Get the physical address.
    //
    status = VirtualToPhysical64(Context,
        VirtualAddress,
        &physicalAddress);

    if (!NT_SUCCESS(status)) {
        LogLibInfoPrintf(L"Failed to convert virtual address 0x%llx to physical address. Status: 0x%llx\r\n",
            VirtualAddress,
            status);

        goto Exit;
    }

    if (PhysicalAddress != NULL) {
        PhysicalAddress->QuadPart = physicalAddress.QuadPart;
    }

    //
    // Read this from DDR
    //
    status = ReadFromDDRSectionByPhysicalAddress(Context,
        physicalAddress,
        Length,
        Buffer);

    if (!NT_SUCCESS(status)) {
        LogLibInfoPrintf(L"Failed to read %u bytes from physical address 0x%I64x. Status: 0x%llx\r\n",
            Length,
            physicalAddress.QuadPart,
            status);

        goto Exit;
    }

    LogLibInfoPrintf(L"Read %u bytes from virtual address 0x%llx (PA: 0x%I64x).\r\n",
        Length,
        VirtualAddress,
        physicalAddress.QuadPart);
    status = STATUS_SUCCESS;

Exit:

    return status;
}

NTSTATUS
ExtractWindowsDumpFile64(PDMP_CONTEXT Context)
/*--
Extract Windows Dump file
it will
1. search in memory Dump header
2. validate DDR
3. Init Dump file
4. get debug block data
5. write dump header to dump file
6. write DDR to dump file
7. update dump file will cpu context
++*/
{
    NTSTATUS    status = STATUS_UNSUCCESSFUL;
    HRESULT     result = ERROR_SUCCESS;

    LogLibInfoPrintf(L"=========== Getting the pre-built DUMP_HEADER64. ===========\r\n");
    status = GetDumpHeader64(Context);
    if (FAILED(status)) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error: Failed to Get Dump Header\n");
        goto Exit;
    }
    
    //
    // Change Bugcheck Parameter 2 to 0xFFFFFFFF to indicate the dump have no AP_REG
    //
    if (Context->HasValidAP_REG != TRUE) {
        Context->DumpHeader64->BugCheckParameter2 = INVALID_AP_REG;
    }
    
    LogLibInfoPrintf(L"=========== Validating DUMP_HEADER64's memory descriptors against DDR sections. ===========\r\n");
    status = ValidateDDRAgaisntPhysicalMemoryBlock64(Context);

    if (FAILED(status)) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error: Failed ValidateDDRAgaisntPhysicalMemoryBlock64 \n");
        goto Exit;
    }

    LogLibInfoPrintf(L"=========== Init the Windows 64 dump file ===========\r\n");
    result = InitDumpFile(Context);
    if (result != ERROR_SUCCESS){
        LogLibErrorPrintf(
            result,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error: Failed to InitDumpFile : %d\n",
            GetLastError());
        goto Exit;
    }

    LogLibInfoPrintf(L"=========== Writing DUMP_HEADER 64 to the dump file. ===========\r\n");
    status = WriteDumpHeader64(Context);
    if (FAILED(status)) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error: Failed WriteDumpHeader64 \n");
        goto Exit;
    }

    LogLibInfoPrintf(L"=========== Writing DDR section to the dump file. ===========\r\n");
    status = WriteDDR64(Context);
    if (FAILED(status)) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error: Failed WriteDDR64 \n");
        goto Exit;
    }
  LogLibInfoPrintf(L"=========== Trying to load DbgEng.dll to understand virtual memory. ===========\r\n");
    
    if (!DbgClient::Initialize(Context->DedicatedDumpFilePath, L"cache*c:\\symbols;srv*http://symweb;")) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error: Failed to initialize Debug Client \n");
            goto Exit;
    }        
    
    LogLibInfoPrintf(L"=========== Try and determine if KdDebuggerDataBlock is encoded. ===========\r\n");
    status = GetKdDebuggerDataBlock64(Context);
    if (!NT_SUCCESS(status)) {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L"Error:GetKdDebuggerDataBlock64 \n");
   //     goto Exit;
    }

    if(Context->IsAPREGRequested != FALSE) {
        LogLibInfoPrintf(L"=========== Update CPU context ==============================================\r\n");
           switch (Context->DumpHeader64->MachineImageType) {
           case IMAGE_FILE_MACHINE_AMD64:
            if (Context->SectionStats.CPUContextSectionCount >0){
              status = UpdateContextAmd64(Context);
            }
           case IMAGE_FILE_MACHINE_ARM:
            case IMAGE_FILE_MACHINE_ARM64:
                if (Context->HasValidAP_REG == TRUE) {                
                    if(Context->isAPREG64 == TRUE && Context->IsAPREGRequested == TRUE) {                    
                          LogLibInfoPrintf(L" ====== Updating context with APREG64 . ===========\r\n");
                        result = UpdateContextWithAPReg64(Context);
                        if (FAILED(result)) {
                            LogLibErrorPrintf(
                                E_FAIL,
                                __LINE__,
                                WIDEN(__FUNCTION__),
                                __WFILE__,
                                L"Error: Failed UpdateContextWithAPReg 64 \n");
                            goto Exit;
                        }
                    }
                }
                break;
            default:
                LogLibErrorPrintf(
                        E_FAIL,
                        __LINE__,
                        WIDEN(__FUNCTION__),
                        __WFILE__,
                        L"Error:Unsupported CPU Context tyep \n");
        }
    }
    LogLibInfoPrintf(L"Write to Dump file successfully.\n");

Exit:
    return status;

}
#define MI_IS_NATIVE_PTE_VALID(PTE) ((PTE)->Valid == 1)

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
    
    // @@BEGIN_NOTPUBLICCODE
    LARGE_INTEGER temp;
    UINT64        pxe = 0;
    UINT64        pxeAddress = 0;
    UINT64        ppe = 0;
    UINT64        ppeAddress = 0;
    UINT64        pde = 0;
    UINT64        pdeAddress = 0;
    UINT64        pte = 0;
    UINT64        pteAddress = 0;
    UINT64        Addr;
    
    //
    // Get the Directory Table Base Address
    //
    UINT64 directoryTableBase = (UINT64)(Context->DumpHeader64->DirectoryTableBase & ARM64_VALID_PFN_MASK);
    if (directoryTableBase == 0) {
        LogLibInfoPrintf(L"Directory table base is NULL.\r\n");
        status = STATUS_BAD_DATA;
        goto Exit;
    }     

    // Init to all one (0xFFFFFFFFFFFFFFFF)
    PhysicalAddress->QuadPart = (ULONGLONG)-1;

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

    PhysicalAddress->QuadPart = (pte & ARM64_VALID_PFN_MASK) + (VirtualAddress & ARM64_OFFSET_WITHIN_PAGE_MASK);
    LogLibInfoPrintf(L"Virtual address= 0x%llx  decoded physical address= 0x%llx \r\n", VirtualAddress, PhysicalAddress->QuadPart);
    status = STATUS_SUCCESS;
Exit:

    // @@END_NOTPUBLICCODE
    
    return status;
}


NTSTATUS
UpdateContextAmd64(
    _Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function updates CONTEXT with contents in the Context Section.

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
    PAMD64_CONTEXT          amd64Context = nullptr;
    UINT64                  contextVA = 0;
    UINT32                  dataSize = sizeof(AMD64_CONTEXT);
    PKDDEBUGGER_DATA64      kdDebuggerDataBlock = nullptr;
    UINT64                  kiProcessorBlockVA = 0;
    UINT64                  offsetPrcbContext = 0;
    UINT64                  prcbAddressVA = 0;
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    UINT64                  tempVA = 0;
    IO_STATUS_BLOCK         statusBlock;
    LARGE_INTEGER           curoffset = { 0 };
    LARGE_INTEGER           amd64ContextAddress;

    //
    // Check prereqs...
    //
    LogLibInfoPrintf(L"Checking prerequisites...\r\n");

    if (nullptr == Context)
    { // Need a valid Context to proceed
        LogLibInfoPrintf(L"Invalid Context pointer.\r\n");
        goto Exit;
    }
    else if (nullptr == Context->KdDebuggerDataBlock)
    { // Need a valid KdDebuggerDataBlock to proceed
        LogLibInfoPrintf(L"KdDebuggerDataBlock not available.\r\n");
        goto Exit;
    }
    else
    { // All is well, capture/resolve the needed debug pointers and offsets
        kdDebuggerDataBlock = Context->KdDebuggerDataBlock;
        kiProcessorBlockVA = kdDebuggerDataBlock->KiProcessorBlock;
        offsetPrcbContext = kdDebuggerDataBlock->OffsetPrcbContext;
    }


    // Check for offsets to be valid (non-zero)
    if (0 == kiProcessorBlockVA)
    { // Within the KdDebuggerDataBlock, we require a valid KiProcessor pointer
        LogLibInfoPrintf(L"KiProcessorBlock not available.\r\n");
        goto Exit;
    }
    else if (0 == offsetPrcbContext)
    { // Within the KdDebuggerDataBlock, we require a valid OffsetPrcbContext pointer
        LogLibInfoPrintf(L"OffsetPrcbContext not available.\r\n");
        goto Exit;
    }

    if (!NT_SUCCESS(status = GetAmd64CPUContext(Context)))
    {
        LogLibInfoPrintf(L"Failed to to get x64 context from the rawdump Status: 0x%x\r\n",
                         status);
        goto Exit;
    }

    //
    // Allocate the space for context. 
    //
    LogLibInfoPrintf(L"Allocating %u bytes for context.\r\n", dataSize);
    amd64Context = (PAMD64_CONTEXT)malloc(dataSize);
    if (nullptr == amd64Context)
    {
        LogLibInfoPrintf(L"Failed to allocate memory for CONTEXT, requested size : %u bytes\r\n", dataSize);
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    LogLibInfoPrintf(L"KiProcessorBlock: 0x%I64x  OffsetPrcbContext: 0x%I64x  ProcessorCount: %u\r\n",
                     kiProcessorBlockVA,
                     offsetPrcbContext,
                     Context->DumpHeader64->NumberProcessors);

    //
    // Update the CONTEXT for each processor.
    //
    for (UINT32 indexProcessor = 0; indexProcessor < Context->DumpHeader64->NumberProcessors; indexProcessor++)
    {
        //
        // Calculate the virtual address of the location containing the PRCB virtual address.
        //
        tempVA = kiProcessorBlockVA + (sizeof(UINT32)* indexProcessor);

        LogLibInfoPrintf(L"[%u] VA of PRCB address: 0x%I64x\r\n",
                         indexProcessor,
                         tempVA);

        status = ReadFromDDRSectionByVirtualAddress(Context,
                                                    tempVA,
                                                    sizeof(prcbAddressVA),
                                                    &prcbAddressVA,
                                                    NULL);
        if (!NT_SUCCESS(status)) {
            LogLibInfoPrintf(L"Failed to read the address for PRCB. VA: 0x%I64x Status: 0x%I64x\r\n",
                             tempVA,
                             status);
            goto Exit;
        }

        if (prcbAddressVA == 0) {
            LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L"PRCB VA is 0! Abort processing\n");
            status = STATUS_DATA_ERROR;
            goto Exit;
        }
        //
        // Calculate the address of CONTEXT and read it.
        //
        tempVA = prcbAddressVA + offsetPrcbContext;

        LogLibInfoPrintf(L"[%u] VA Context address: 0x%I64x\r\n",
                         indexProcessor,
                         tempVA);

        status = ReadFromDDRSectionByVirtualAddress(Context,
                                                    tempVA,
                                                    sizeof(contextVA),
                                                    &contextVA,
                                                    NULL);
        if (!NT_SUCCESS(status)) {
            LogLibInfoPrintf(L"Failed to read the address for CONTEXT. VA: 0x%I64x Status: 0x%I64x\r\n",
                             tempVA,
                             status);
            goto Exit;
        }

        if (contextVA == 0) {
            LogLibErrorPrintf(
                E_FAIL,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L"Context VA is 0! Abort processing\n");
            status = STATUS_UNSUCCESSFUL;
            goto Exit;
        }

        ZeroMemory(amd64Context, dataSize);
        status = ReadFromDDRSectionByVirtualAddress(Context,
                                                    contextVA,
                                                    dataSize,
                                                    amd64Context,
                                                    &amd64ContextAddress);
        if (!NT_SUCCESS(status)) {
            LogLibInfoPrintf(L"Failed to read the address for CONTEXT. VA: 0x%I64x Status: 0x%I64x\r\n",
                             tempVA,
                             status);
            goto Exit;
        }

        LogLibInfoPrintf(L"[%u] Amd64ContextPA: 0x%I64x\r\n",
                         indexProcessor,
                         amd64ContextAddress.QuadPart);

        RtlZeroMemory(amd64Context, dataSize);
        curoffset.QuadPart = Context->CPUContextAddress.QuadPart + indexProcessor*dataSize;
        if (FAILED(Context->hDisk.ReadAtOffset((PCHAR)amd64Context, dataSize, curoffset, DEVICE_IO::READ_EXACT)))
        {
            status = STATUS_UNSUCCESSFUL;
            LogLibInfoPrintf(L"[%u] Failed toread dump's CONTEXT. Status: 0x%I64x\r\n", indexProcessor, status);
            goto Exit;
        }

        //
        // Write the CONTEXT to dump file.
        //
        status = WriteToDumpByPhysicalAddress(Context,
                                              amd64ContextAddress,
                                              dataSize,
                                              amd64Context);

        if (!NT_SUCCESS(status)) {
            LogLibInfoPrintf(L"[%u] Failed to update dump's CONTEXT. Status: 0x%I64x\r\n", indexProcessor, status);
            goto Exit;
        }

        status = NtFlushBuffersFile(Context->DedicatedDumpHandle, &statusBlock);
        if (NT_SUCCESS(status) && (status != STATUS_PENDING)) {
            status = statusBlock.Status;
            LogLibInfoPrintf(L"[%u] NtFlushBuffersFile FAILED, Status 0x%x \r\n", indexProcessor, status);
        }
        LogLibInfoPrintf(L"[%u] Written updated CONTEXT to dump.\r\n", indexProcessor);
    } //for indexProcessor



Exit:

    if (nullptr != amd64Context) {
        free(amd64Context);
    }

    if (Context->DedicatedDumpHandle != INVALID_HANDLE_VALUE){
        CloseHandle(Context->DedicatedDumpHandle);
        Context->DedicatedDumpHandle = INVALID_HANDLE_VALUE;
    }

    return status;
}

NTSTATUS
GetAmd64CPUContext(
_Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function writes CPU Context Specific sections based on the DUMP_HEADER64's physical memory descriptor
to dump file.

Arguments:

Context - Pointer to the global context structure.

Return Value:

NT status code.

--*/
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;

    for (UINT32 sectionIndex = 0; sectionIndex < SECTIONS_COUNT(&Context->RawDumpHeader); sectionIndex++)
    {

        if (RAW_DUMP_SECTION_TYPE_CPU_CONTEXT == SECTION_ENTRY_PTR(Context->pRawDumpSectionTable, sectionIndex)->Type)
        {
            Context->CPUContextAddress.QuadPart = SECTION_ENTRY_PTR(Context->pRawDumpSectionTable, sectionIndex)->Offset;
            Context->SectionStats.TotalCpuContextSizeInBytes = SECTION_ENTRY_PTR(Context->pRawDumpSectionTable, sectionIndex)->Size;
            status = STATUS_SUCCESS;
            break;
        }

    }

    return status;
}

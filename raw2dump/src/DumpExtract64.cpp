
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windef.h>
#include "dumputil.h"
#include "DumpExtract64.h"
#include "apreg64.h"

NTSTATUS
ValidateDDRAgainstPhysicalMemoryBlock64(
_Inout_ PDMP_CONTEXT Context
);

NTSTATUS
WriteDumpHeader64(
_Inout_ PDMP_CONTEXT Context
);


NTSTATUS
WriteDDR64(
_Inout_ PDMP_CONTEXT Context
);

NTSTATUS
GetKdDebuggerDataBlock64(
_Inout_ PDMP_CONTEXT Context
);

NTSTATUS
ExtractWindowsDumpFile64(PDMP_CONTEXT Context)
{
    NTSTATUS    status = STATUS_UNSUCCESSFUL;
    HRESULT     hr = S_OK;
    
    TraceInfo("Validating DUMP_HEADER's memory descriptors against DDR sections");
    hr = ValidateDDRAgainstPhysicalMemoryBlock64(Context);          
    if (FAILED(hr)) {
        TraceHRESULT("ValidateDDRAgainstPhysicalMemoryBlock64 failed", hr);
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
    hr = WriteDumpHeader64(Context);
    if (FAILED(hr)) {
        TraceHRESULT("WriteDumpHeader failed", hr);
        goto ExitHR;
    }

    TraceInfo("Writing DDR section to the dump file");
    hr = WriteDDR64(Context);
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

    if (!DbgClient::Initialize(Context->WindowsDumpFilePath, NULL))
    {
        TraceInfo("Error: Failed to initialize Debug Client");
        goto Exit;
    }

    TraceInfo("Try to determine if KdDebuggerDataBlock is encoded");
    status = GetKdDebuggerDataBlock(Context);
    if (FAILED(status)) {
        TraceNTSTATUS("GetKdDebuggerDataBlock failed", status);
        goto Exit;
    }

    TraceInfo("Update CPU context");
    switch (Context->DumpHeader64->MachineImageType){
    case IMAGE_FILE_MACHINE_I386:
        if (Context->CPUContextSectionCount >0){
            status = UpdateContextX86(Context);
        }
        break;

    case IMAGE_FILE_MACHINE_ARM:
    case IMAGE_FILE_MACHINE_ARM64:
        //
        // Get the AP_REG data from memory and do some validation.
        //
        TraceInfo("Try getting AP_REG");

        hr = GetAPReg64(Context);
        if (FAILED(hr)) {
            TraceHRESULT("GetAPReg64 failed", hr);
            goto ExitHR;
        }
        status = UpdateContextWithAPReg64(Context);
        if (FAILED(status)) {
            TraceHRESULT("UpdateContextWithAPReg64 failed", hr);
            goto Exit;
        }
        break;

    default:
        hr = E_FAIL;
        TraceHRESULT("Unsupported CPU Context type", hr);
        break;
    }

    if (Context->DumpHeaderStatus != DHS_VALID) {
        if (FAILED(hr = WriteFakeDumpHeader(Context))) {
            // not fatal
            TraceHRESULT("WriteFakeDumpHeader failed", hr);
        }
    }

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

NTSTATUS
ValidateDDRAgainstPhysicalMemoryBlock64(
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
    PDDR_MEMORY_MAP               ddrMemoryMap = NULL;
    UINT64                        endPD;
    UINT32                        indexDDR;
    UINT32                        indexPD;
    UINT64                        startPD;
    UINT32                        spanCount;
    UINT64                        memorySizeFromPD = 0;
    PPHYSICAL_MEMORY_DESCRIPTOR64 physDesc = NULL;
    NTSTATUS                      status = STATUS_UNSUCCESSFUL;
    BOOLEAN                       startInSection;
    BOOLEAN                       endInSection;

    ddrMemoryMap = Context->DDRMemoryMap;
    physDesc = &(Context->DumpHeader64->PhysicalMemoryBlock);

    memorySizeFromPD = physDesc->NumberOfPages * PAGE_SIZE;
    Context->MemoryDescriptors64 = physDesc;
    Context->SizeAccordingToMemoryDescriptors = memorySizeFromPD;

    if (memorySizeFromPD > Context->TotalDDRSizeInBytes) {
        LogLibInfoPrintf(L"Memory size from memory block (0x%p) is larger than DDR size (0x%p)\r\n",
            memorySizeFromPD,
            Context->TotalDDRSizeInBytes);
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

        for (indexDDR = 0; indexDDR < Context->DDRSectionCount; indexDDR++) {
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
    IO_STATUS_BLOCK         statusBlock;

    Context->WindowsDumpFileOffset.QuadPart = 0;
    
    //
    // Update the DUMP_HEADER64
    //
    TraceInfo("Updating DUMP_HEADER32 before it is written to dump");
    Context->DumpHeader64->RequiredDumpSpace = Context->ActualDumpFileUsedInBytes;
    Context->DumpHeader64->RequiredDumpSpace  = Context->ActualDumpFileUsedInBytes;
    Context->DumpHeader64->BugCheckCode       = Context->BugCheckCode;
    Context->DumpHeader64->BugCheckParameter1 = (ULONG)Context->BugCheckParam1;
    Context->DumpHeader64->BugCheckParameter2 = (ULONG)Context->BugCheckParam2;
    Context->DumpHeader64->BugCheckParameter3 = (ULONG)Context->BugCheckParam3;
    Context->DumpHeader64->BugCheckParameter4 = (ULONG)Context->BugCheckParam4;

    if ((Context->SVSectionCount > 0) || (Context->CPUContextSectionCount > 0)) {
        Context->DumpHeader64->SecondaryDataState = STATUS_SUCCESS;
    }

    //
    // Write the header to dump.
    //
    dataSize = sizeof(DUMP_HEADER64);
    status = NtWriteFile(Context->WindowsDumpHandle,
                nullptr,
                nullptr,
                nullptr,
                &statusBlock,
                Context->DumpHeader64,
                dataSize,
                &Context->WindowsDumpFileOffset,
                nullptr);

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
    LARGE_INTEGER   basePA;
    ULONG64         PageRemain = 0;
    LARGE_INTEGER   bytesWritten = { 0 };
    UINT32          io = 0;
    UINT32          index = 0;
    UINT32          ioCountPerRun = 0;
    LARGE_INTEGER   runSize;
    LARGE_INTEGER   startPA;
    UINT32          ioSize = 0;
    ULONG           buffersize = DEFAULT_DMP_BUF_SZ;
    NTSTATUS        status = STATUS_UNSUCCESSFUL;
    PVOID           tempBuffer = NULL;
    IO_STATUS_BLOCK statusBlock;
    
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

    //ZeroMemory(tempBuffer, buffersize);

    for (index = 0; index < Context->DumpHeader64->PhysicalMemoryBlock.NumberOfRuns; index++)  {

        basePA.QuadPart = (Context->DumpHeader64->PhysicalMemoryBlock.Run[index].BasePage * PAGE_SIZE);
        PageRemain = Context->DumpHeader64->PhysicalMemoryBlock.Run[index].PageCount;
        runSize.QuadPart = (Context->DumpHeader64->PhysicalMemoryBlock.Run[index].PageCount *PAGE_SIZE);
        ioCountPerRun = (UINT32)(runSize.QuadPart / buffersize);

        if ((runSize.QuadPart % buffersize) > 0) {
            ioCountPerRun++;
        }

        LogLibInfoPrintf(L"0x%llx IOs will be required for this run.\r\n", ioCountPerRun);

        for (io = 0; io < ioCountPerRun; io++) {
            startPA.QuadPart = basePA.QuadPart + io * ioSize;

            ioSize = (UINT32)((PageRemain * PAGE_SIZE)< buffersize ? PageRemain * PAGE_SIZE : buffersize);

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

            status = NtWriteFile(Context->WindowsDumpHandle,
                nullptr,
                nullptr,
                nullptr,
                &statusBlock,
                tempBuffer,
                ioSize,
                &Context->WindowsDumpFileOffset,
                nullptr);
            if (!NT_SUCCESS(status)) {
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

    if ((ULONG64)(bytesWritten.QuadPart / PAGE_SIZE) != Context->DumpHeader64->PhysicalMemoryBlock.NumberOfPages) {
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
    UINT32                      dataSize;
    PKDDEBUGGER_DATA64          dbgDataBlock;
    LARGE_INTEGER               dbgDataHeaderPA;
    LARGE_INTEGER               decodedBlockPA;
    ULONG64                     dbgDataHeaderVA;
    NTSTATUS                    status = STATUS_UNSUCCESSFUL;

    LogLibInfoPrintf(L"Checking DBGKD_DEBUG_DATA_HEADER64.Size\r\n");
    Context->KdDebuggerDataBlockPA.QuadPart = ADDRESS_NOT_PRESENT;

    //
    // Read DBGKD_DEBUG_DATA_HEADER64
    // 
    dbgDataHeaderVA = Context->DumpHeader64->KdDebuggerDataBlock;
    dataSize = sizeof(KDDEBUGGER_DATA64);
    dbgDataBlock = (PKDDEBUGGER_DATA64)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dataSize);
    if (dbgDataBlock == NULL) {
        LogLibInfoPrintf(L"Failed to allocated 0x%llx bytes for KdDebuggerDataBlock\r\n", dataSize);
        status = STATUS_NO_MEMORY;
        goto Exit;
    }

    status = ReadFromDDRSectionByVirtualAddress(Context,
                dbgDataHeaderVA,
                dataSize,
                dbgDataBlock,
                &dbgDataHeaderPA);    
      
    if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Failed to read nt!KdDebuggerDataBlock", status);
            goto Exit;
    }
    
    //
    // Check the size.
    //
    LogLibInfoPrintf(L"DBGKD_DEBUG_DATA_HEADER64.Size is: 0x%llx\r\n", dbgDataBlock->Header.Size);

   if(dbgDataBlock->Header.Size > MAX_KDDEBUGGER_BLOCK_SIZE  || !ValidateKdDebuggerDataBlock(&(dbgDataBlock->Header))) {    
    
        LogLibInfoPrintf(L"The KdDebuggerDataBlock appears to be encoded \r\n");

        //
        // As ARM platform DumperHeader size is PageSize so 
        // directly use PAGE_SIZE
        //
        decodedBlockPA.QuadPart = Context->DumpHeaderPA.QuadPart + PAGE_SIZE;
        LogLibInfoPrintf(L"Decoded version of KdDebuggerDataBlock should be at 0x%I64x \r\n", decodedBlockPA.QuadPart);

        //directly read to buffer
        status = ReadFromDDRSectionByPhysicalAddress(Context,
                        decodedBlockPA,
                        dataSize,
                        dbgDataBlock);
        if (!NT_SUCCESS(status)) {

            LogLibInfoPrintf(L"Read Decoded Debug Block failed. Status:0x%x\r\n", status);
            goto Exit;
        }
    }

     else {
           LogLibInfoPrintf(L"DBGKD_DEBUG_DATA_HEADER64.Size is: 0x%x\r\n", dbgDataBlock->Header.Size);
           LogLibInfoPrintf(L"KdDebuggerDataBlock appears to be valid and decoded. Read it in to memory.\r\n");
    }
    
    Context->KdDebuggerDataBlock = dbgDataBlock;   
    Context->KdDebuggerDataBlockPA.QuadPart = dbgDataHeaderPA.QuadPart;
    if (!ValidateKdDebuggerDataBlock(&(Context->KdDebuggerDataBlock->Header))) {
        status = STATUS_UNSUCCESSFUL;
        LogLibInfoPrintf(L"The KdDebuggerDataBlock is not valid.\r\n");
        goto Exit;
    }

    LogLibInfoPrintf(L"KdDebuggerDataBlock has been read into memory.\r\n");
    status = WriteToDumpByPhysicalAddress(Context,
                Context->KdDebuggerDataBlockPA,
                dataSize,
                Context->KdDebuggerDataBlock);
   
    if (!NT_SUCCESS(status)) {
        LogLibInfoPrintf(L"Failed write the decoded copy of KdDebuggerData to dump. Status: 0x%x\r\n", status);
        goto Exit;
    }
     status = STATUS_SUCCESS;
    LogLibInfoPrintf(L"KdDebuggerDataBlock Writen to 0x%I64x Size:%d been read into memory.\r\n", Context->KdDebuggerDataBlockPA, dataSize);
Exit:
    return status;
}
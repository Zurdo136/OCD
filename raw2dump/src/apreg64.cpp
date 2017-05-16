// 
// This ffile implements the APREG 64 bit format. 
//

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
#include "dumputil.h"
#include "apreg64.h"

#define PROCESSOR_INDEX_MASK 0x0F
#define APREG_MSM_DUMP_TABLE_VERSION_ARM   0x00000001
#define APREG_MSM_DUMP_TABLE_VERSION_ARM64 0x00200000

BOOL g_Shared_IMEM;

//
// Global variable to get contain the list of Contexts.
//
CPUContext64List  gAPRegCPUConextList;

HRESULT 
parseDumpTable(
_Inout_     PDMP_CONTEXT Context,
LARGE_INTEGER   address
);

HRESULT
parseDumpData(
_Inout_  PDMP_CONTEXT   Context,
LARGE_INTEGER           address,
PSDICPUCtxtType        pCPUContext
);

VOID 
dumpAPREGDumpTable(
PAP_REG_MSM_DUMP_TABLE dump_table
);

VOID 
dumpAPREGDumpData(
PAP_AP_REG_MSM_DUMP_DATA dump_data
);

VOID 
dumpARM64Context(
PSDI_CPU64_CTXT_REGS_TYPE arm64Context
);

VOID 
dumpARM32Context(
PSDI_CPU32_CTXT_REGS_TYPE arm32Context
);

VOID
dumpAllARMContexts(
VOID
);

HRESULT
ConvertAPREGContextToARMContext(
    _In_ PSDI_CPU32_CTXT_REGS_TYPE apregContext,
    _Out_ PARM_CONTEXT             ntctx
);

HRESULT
ConvertAPREGContextToARM64Context(
    _In_ PSDI_CPU64_CTXT_REGS_TYPE apregContext,
    _Out_ PARM64_CONTEXT             ntctx
);

HRESULT
GetAPREG64AddrFromOCIMEMSection(
    _Inout_ PDMP_CONTEXT    Context
)
/*++

Routine Description:

This provides the APREG Address from OCMEM SV section.
The APREG table address is in SHARED_IMEM + 0x10
SHARED_IMEM is 0x0 in 8916 (older devices)
and 0x7F000 in 8994+ (also holds for ARM64)

Arguments:

Context - PDMP_CONTEXT

sharedIMem - Indicates if the Address is located in .

Return Value:

HRESULT
--*/
{
    HRESULT         hr = E_FAIL;
    UINT32          index;
    CHAR            name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
    UINT32          sectionCount = Context->RawDumpHeader.SectionsCount;
    size_t          bytesProcessed = 0;
    LARGE_INTEGER   offset;
    
    //
    // Initialize the global list of contexts
    //
    gAPRegCPUConextList.listHead = nullptr;
    gAPRegCPUConextList.listEnd  = nullptr;
    gAPRegCPUConextList.numProcressors = 0;
    
    for (index = 0; index < sectionCount; index++)
    {
        memset(name, 0, RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
        memcpy(name, (void*) &(Context->RawDumpSectionTable[index].Name),
               RAW_DUMP_SECTION_HEADER_NAME_LENGTH);       
        if(strcmp(name, "OCIMEM.BIN") == 0) {
            TraceInfo("Found OCIMEM section");
            hr = E_UNEXPECTED;
            break;
        }
    }

    if(g_Shared_IMEM == TRUE) {
        TraceInfo("Shared IMEM  is set");
        offset.QuadPart = Context->RawDumpSectionTable[index].Offset + 0x7F000 + 0x10;
    }
    else {
        TraceInfo("Shared IMEM is not set, IMEM_BASE = SharedIMEM");
        offset.QuadPart = Context->RawDumpSectionTable[index].Offset + 0x10;
    }

    TraceInfo1("Reading from ", "Offset", offset.QuadPart);
    if ( FAILED(Context->hRawFile.SetPos(offset))
         || FAILED(Context->hRawFile.Read((PCHAR)&(Context->APRegAddress.QuadPart), sizeof(UINT32), &bytesProcessed))
         || (0 == bytesProcessed)
       )
    {
        TraceInfo("Failed to read DDR section from device.");
        hr = E_FAIL;
        goto Exit;
    }

    TraceInfo1("APREG ADDRESS", "Address", Context->APRegAddress.QuadPart);
    hr = S_OK;

Exit: 
    return hr;
}


HRESULT
GetAPReg64(
_Inout_ PDMP_CONTEXT    Context
)
// Find the location where the APREG could be
// read the File for AP_REG_MSM_DUMP_TABLE number of bytes.
// and recursively read till you find the location of a dump data 
// entry. 
// parse the data which the 

/*++

Routine Description:

This routine finds the register context information

Arguments:

Context - PDMP_CONTEXT

IsApRegValid - Informs the caller when a valid AP_REG was found.

Return Value:

HRESULT
--*/

{
    HRESULT hr = E_FAIL;
    // Assuming the APREG address will be abtained by now. 
    //LARGE_INTEGER   physicalAddress = Context->APREG64_IMEM_ADDR;    
    hr = parseDumpTable(Context, Context->APRegAddress);
    if(FAILED(hr)) {
        TraceInfo("Error: APREG address seems to be invalid");
        goto Exit; 
    }
    dumpAllARMContexts();
 Exit:
    return hr;
}

HRESULT 
parseDumpTable(
_Inout_  PDMP_CONTEXT   Context,
LARGE_INTEGER           address
)
// Read the address passed to it. 
// Find out how many entries the table has. 
// Go to each addresses in those entries and call either 
// this same function or parse data 
{
    HRESULT                 hr = S_OK;
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    AP_REG_MSM_DUMP_TABLE   apreg_dump_table;
    PAP_REG_MSM_DUMP_TABLE  papreg_dump_table = nullptr;
    UINT32                  index;
    LARGE_INTEGER           elementAddress;
    UINT32                  tableSize;
    PCPUContext64           currentContext = nullptr;
    PCPUContext64           prevContext = nullptr;

    status = ReadFromDDRSectionByPhysicalAddress(Context, 
        address,
        sizeof(AP_REG_MSM_DUMP_TABLE),
        &apreg_dump_table);
    
    if (!NT_SUCCESS(status)) {
        hr = E_FAIL;    
        TraceNTSTATUS("Failed to read PDE.", status);
        goto Exit;

    }    
        
    if ( (apreg_dump_table.Version != APREG_MSM_DUMP_TABLE_VERSION_ARM)
         && (apreg_dump_table.Version != APREG_MSM_DUMP_TABLE_VERSION_ARM64)
       )
    {
        hr = E_FAIL;
        TraceInfo("parseDumpTable: Unexpected ARM Dump Table version");
        goto Exit;
    }

    // if there are more than one entries we will have to read again. 
    if(apreg_dump_table.Num_Entries > 1){
        //
        // allocate a memory for this guy.
        //
        tableSize = sizeof(AP_REG_MSM_DUMP_TABLE) +
                    (sizeof(AP_REG_MSM_DUMP_ENTRY)*(apreg_dump_table.Num_Entries-1));
        
        TraceInfo1("ParseDumpTable", "tableSize", tableSize);
        papreg_dump_table = (PAP_REG_MSM_DUMP_TABLE) HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, tableSize);

        ReadFromDDRSectionByPhysicalAddress(Context, 
                                            address,
                                            tableSize,
                                            papreg_dump_table
                                            );
    }
    else {
        papreg_dump_table = (PAP_REG_MSM_DUMP_TABLE) HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(AP_REG_MSM_DUMP_TABLE));
        memcpy(papreg_dump_table, &apreg_dump_table, sizeof(AP_REG_MSM_DUMP_TABLE));
    }
    
    dumpAPREGDumpTable(papreg_dump_table); 
    
    //
    // perform some checks here.
    //
    for(index = 0; index < papreg_dump_table->Num_Entries; index++) {
        // 
        // Do some sanity checks.
        //
        elementAddress.QuadPart = papreg_dump_table->Entries[index].Address;
        switch(papreg_dump_table->Entries[index].type){
        
        case MSM_DUMP_TYPE_TABLE: 
            // 
            // We found a table again. 
            //
            parseDumpTable(Context, elementAddress);
            break;
            
        case MSM_DUMP_TYPE_DATA:
             //
             // The LSB indicates the the processor number. The 2nd LSB indicates what kind 
             // of data is it. We are only interested in Context type.
             //
             if (papreg_dump_table->Entries[index].id >> 4 == MSM_DUMP_DATA_CPU_CTX) {
                TraceInfo("FOUND A CONTEXT TYPE DATA ENTRY \n");
                
                //
                // add to the list of contexts 
                //
                currentContext = (PCPUContext64) HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(CPUContext64));
                if(gAPRegCPUConextList.numProcressors == 0) {
                    gAPRegCPUConextList.listHead = currentContext;
                    prevContext = currentContext;
                }
                //
                // now parse it and fill the entry.
                //
                prevContext->Flink = currentContext;
                parseDumpData(Context, elementAddress, &(currentContext->cpuContext));
                       
                prevContext = currentContext;
                gAPRegCPUConextList.numProcressors++;
             }
             
            break;
            
        }
    }    
    
    
 Exit:
    if(papreg_dump_table != nullptr) {
        HeapFree(GetProcessHeap(), NULL, papreg_dump_table);
    }        
    return hr;
}

HRESULT
parseDumpData(
_Inout_  PDMP_CONTEXT   Context,
LARGE_INTEGER           address,
PSDICPUCtxtType        pCPUContext
)
{
    HRESULT                     hr = E_FAIL;
    AP_REG_MSM_DUMP_DATA        apregDumpData;
    LARGE_INTEGER               contextAddress;
    UINT32                      cpuStatus = 0;
    
    ReadFromDDRSectionByPhysicalAddress(Context, 
           address,
           sizeof(AP_REG_MSM_DUMP_DATA),
           &apregDumpData);
           
    //dumpAPREGDumpData(&apregDumpData);
    
    // 
    // TODO: Do some sanity checks here.
    //
    
    
    
    //
    // Read the Context.
    //
    contextAddress.QuadPart = apregDumpData.address;
    
    ReadFromDDRSectionByPhysicalAddress(Context, 
           contextAddress,
           sizeof(SDICPUCtxtType), /*apregDumpData.len,*/
           pCPUContext);    
         
    //
    // Analysing CPU Status word 0 
    //
    cpuStatus = pCPUContext->Status[0] & CPU_STATUS_MASK;
    switch(cpuStatus) {
        case CPU_STATUS_A53: 
            TraceInfo("CPU is A53");
            break;
            
        case CPU_STATUS_A57:
            TraceInfo("CPU is A57");
            break;
            
        case CPU_STATUS_HYDRA:
            TraceInfo("CPU is HYDRA");
            break;
            
        default:
            TraceInfo("INVALID CPU");
    }
    
    //
    // Analysing CPU Status word 1 
    //
    
    if(pCPUContext->Status[1] & NS_WDT_FROM_WARM_BOOT) {
        TraceInfo("NS WDT bite woke CPU up from warm boot or NS WDT bite happened during TZ execution in warm boot");
    }    
    else if(pCPUContext->Status[1] & NS_WDT_FROM_TZ) {
        TraceInfo("SGI from TZ prompted dump. This happens after the first core handles the NS WDT bite interrupt.");
    }
    else if(pCPUContext->Status[1] & NS_WDT_BITE_PROMTPED_DUMP) {
        TraceInfo("NS WDT bite interrupt prompted dump.");
    }
    else if(pCPUContext->Status[1] & NS_WDT_BITE_PROMTPED_NS_EXECUTION) {
        TraceInfo("NS WDT bite interrupted NS execution.");
    }
    else {
        TraceInfo("Secure WDT bite.");
    }
    
    //
    // Currently defaulting to 32bit CPU context
    //

    if(Context->Is64Bit) {
        dumpARM64Context(&(pCPUContext->Cpu_regs.Cpu64_ctxt));
    }
    else {
       dumpARM32Context(&(pCPUContext->Cpu_regs.Cpu32_ctxt));
    }
    
    
    return hr;    
}


HRESULT
UpdateContextWithAPReg64(
    _Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function updates CONTEXT with AP_REG.

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
    ARM_CONTEXT             armContext;
    ARM64_CONTEXT           arm64Context;
    UINT64                  contextVA = 0;
    UINT32                  dataSize = 0;
    UINT32                  indexProcessor = 0;
    PKDDEBUGGER_DATA64      kdDebuggerDataBlock;
    UINT64                  kiProcessorBlockVA = 0;
    UINT64                  offsetPrcbContext = 0;
    UINT64                  prcbAddressVA = 0;
    UINT32                  processorCount = 0;
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    HRESULT                 hr = E_FAIL;
    UINT64                  tempVA = 0;
    IO_STATUS_BLOCK         statusBlock;
    UINT32                  indexProcessorOffset = 0;
    LARGE_INTEGER           physicalAddress;
    
    //
    // Check prereqs...
    //
    TraceInfo("Checking prerequisites...");

    if (Context->KdDebuggerDataBlock == NULL) {
        TraceInfo("Unable to update APREG. KdDebuggerDataBlock not available.");
        hr = E_FAIL;
        goto Exit;
    }

    if (Context->DumpHeader32 == NULL && Context->DumpHeader64 == NULL) {
        TraceInfo("Unable to update APREG. DumpHeader not available.");
        hr = E_FAIL;
        goto Exit;
    }
    
    //
    // Get the addresses of various variables we will need.
    //
    kdDebuggerDataBlock = Context->KdDebuggerDataBlock;
    kiProcessorBlockVA = (UINT64)(kdDebuggerDataBlock->KiProcessorBlock);
    offsetPrcbContext = (UINT64)(kdDebuggerDataBlock->OffsetPrcbContext);

    processorCount = Context->Is64Bit ? Context->DumpHeader64->NumberProcessors : Context->DumpHeader32->NumberProcessors;

    TraceInfo3("KdDebuggerDataBlock info", 
                "KiProcessorBlock", kiProcessorBlockVA,
                "OffsetPrcbContext", offsetPrcbContext,
                "processorCount", processorCount);
  
    //
    // Update the CONTEXT for each processor.
    //
    PCPUContext64 APREG64ContextNode = gAPRegCPUConextList.listHead;
    indexProcessorOffset = Context->Is64Bit ? sizeof(UINT64) : sizeof(UINT32);

    for (indexProcessor = 0; indexProcessor < processorCount; indexProcessor++) {
        //
        // Calculate the virtual address of the location containing the PRCB virtual address.
        //
        
        TraceInfo1("Updating context", "CPU", indexProcessor);
        tempVA = kiProcessorBlockVA + (indexProcessorOffset* indexProcessor);

        TraceInfo2("VA of PRCB address:", "indexProcessor", indexProcessor, "tempVA", tempVA);
        status = ReadFromDDRSectionByVirtualAddress(Context,
            (UINT32)tempVA,
            sizeof(prcbAddressVA),
            &prcbAddressVA,
            &physicalAddress);
        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Failed to read the address for PRCB", status);
            goto Exit;
        }
       

        TraceInfo1("PRCB Virtual Address "," VA ", prcbAddressVA);
        
        if (prcbAddressVA == 0) {
           TraceInfo("Context VA is 0!\n"); 
           continue;
        }
        
        //
        // Calculate the address of CONTEXT and read it.
        //
        tempVA = prcbAddressVA + offsetPrcbContext;

        TraceInfo2("Context address", "CPU", indexProcessor, "VA", tempVA);

        status = ReadFromDDRSectionByVirtualAddress(Context,
            (UINT32)tempVA,
            sizeof(contextVA),
            &contextVA,
            &physicalAddress);
        if (!NT_SUCCESS(status)) {
            TraceNTSTATUS("Failed to read the address for CONTEXT", status);
            goto Exit;
        }

        if (contextVA == 0) {
            TraceInfo("Context VA is 0!");
           // TODO: I am getting such scenarios in which contextVA is 0. 
           continue;
        }

        //
        // I wish the structs were C++.
        //
        if (Context->Is64Bit) {
            //
            // This is 64 bit architecture. 
            //
            
            dataSize = sizeof(ARM64_CONTEXT);
            status = ReadFromDDRSectionByVirtualAddress(Context,
                (UINT32)contextVA,
                dataSize,
                &arm64Context,
                &Context->ArmContextPA);
           
            if (!NTSTATUS(status)) {
                TraceNTSTATUS("Failed to read the address for CONTEXT.", status);
                    goto Exit;
            }
            
            //
            // Update CONTEXT
            //
            //TraceInfo2("Updating context.", "CPU ", indexProcessor,
            //    "PA", &(APREG64ContextNode->cpuContext.Cpu_regs.Cpu64_ctxt));
            
            status = ConvertAPREGContextToARM64Context(
                    &(APREG64ContextNode->cpuContext.Cpu_regs.Cpu64_ctxt), 
                    &arm64Context);
           
           if (!NT_SUCCESS(status)) {
                TraceInfo("Update CONTEXT with AP Reg 64 Failed");
                continue;
            }
            
            //
            // Write the CONTEXT to dump file.
            //        
            status = WriteToDumpByPhysicalAddress(Context,
                Context->ArmContextPA,
                dataSize,
                &arm64Context);
            if (!NT_SUCCESS(status)) {
                TraceNTSTATUS("Failed to update dump's CONTEXT", hr);
                 goto Exit;
            }                                
        }
        else {
            //
            // Allocate the space for context. 
            //
            dataSize = sizeof(ARM_CONTEXT);
            
            //
            // Read the CONTEXT
            //
            status = ReadFromDDRSectionByVirtualAddress(Context,
                (UINT32)contextVA,
                dataSize,
                &armContext,
                &Context->ArmContextPA);
       
            if (!NT_SUCCESS(status)) {
                TraceNTSTATUS("Failed to read the address for CONTEXT", status);                
                goto Exit;
            }
            
            //
            // Update CONTEXT
            //
            //TraceInfo2("Updating context with APREG.", 
            //    "CPU", indexProcessor,
            //    "PA", &(APREG64ContextNode->cpuContext.Cpu_regs.Cpu32_ctxt));
        
            status = ConvertAPREGContextToARMContext(
                &(APREG64ContextNode->cpuContext.Cpu_regs.Cpu32_ctxt), 
                &armContext);

            if (!NT_SUCCESS(status)) {
                TraceInfo("Update CONTEXT with AP Reg 64 Failed");
                continue;
            }
            
            //
            // Write the CONTEXT to dump file.
            //        
            status = WriteToDumpByPhysicalAddress(Context,
                        Context->ArmContextPA,
                        dataSize,
                        &armContext);

            if (!NT_SUCCESS(status)) {
                TraceNTSTATUS("Failed to update dump's CONTEXT", status);
                goto Exit;
            }
        }

        NtFlushBuffersFile(Context->WindowsDumpHandle, &statusBlock);
        TraceInfo1("Wrote updated CONTEXT to dump", "CPU", indexProcessor);
        APREG64ContextNode = APREG64ContextNode->Flink;
        
    } //for indexProcessor

    goto Exit;

Exit:
    status = NtFlushBuffersFile(Context->WindowsDumpHandle, &statusBlock);
    if (NT_SUCCESS(status) && (status != STATUS_PENDING))  {
        //hr = HRESULT_FROM_NTSTATUS(statusBlock.Status);
        hr = S_OK;
    }

    if (Context->WindowsDumpHandle != INVALID_HANDLE_VALUE){
        CloseHandle(Context->WindowsDumpHandle);
        Context->WindowsDumpHandle = INVALID_HANDLE_VALUE;
    }
    
    //
    //  Cleanup the context linked list.
    //
    APREG64ContextNode = gAPRegCPUConextList.listHead;
    PCPUContext64 prevAPREG64ContextNode = gAPRegCPUConextList.listHead;
    for (indexProcessor = 0; indexProcessor < gAPRegCPUConextList.numProcressors; indexProcessor++) {
          APREG64ContextNode = APREG64ContextNode->Flink;
          HeapFree(GetProcessHeap(), NULL, prevAPREG64ContextNode);
          prevAPREG64ContextNode = APREG64ContextNode;
    }

    return hr;
}

HRESULT
ConvertAPREGContextToARM64Context(
    _In_ PSDI_CPU64_CTXT_REGS_TYPE apregContext,
    _Out_ PARM64_CONTEXT             ntctx
)
{
    HRESULT     hr = S_OK;;
    memset(ntctx, 0, sizeof *ntctx);
    
    ntctx->Cpsr = (ULONG)apregContext->spsr_el3;
    
    ntctx->X[0] = apregContext->x0;
    ntctx->X[1] = apregContext->x1;
    ntctx->X[2] = apregContext->x2;
    ntctx->X[3] = apregContext->x3;
    ntctx->X[4] = apregContext->x4;
    ntctx->X[5] = apregContext->x5;
    ntctx->X[6] = apregContext->x6;
    ntctx->X[7] = apregContext->x7;
    ntctx->X[8] = apregContext->x8;
    ntctx->X[9] = apregContext->x9;
    ntctx->X[10] = apregContext->x10;
    ntctx->X[11] = apregContext->x11;
    ntctx->X[12] = apregContext->x12;
    ntctx->X[13] = apregContext->x13;
    ntctx->X[14] = apregContext->x14;
    ntctx->X[15] = apregContext->x15;
    ntctx->X[16] = apregContext->x16;
    ntctx->X[17] = apregContext->x17;
    ntctx->X[18] = apregContext->x18;
    ntctx->X[19] = apregContext->x19;
    ntctx->X[20] = apregContext->x20;
    ntctx->X[21] = apregContext->x21;
    ntctx->X[22] = apregContext->x22;
    ntctx->X[23] = apregContext->x23;
    ntctx->X[24] = apregContext->x24;
    ntctx->X[25] = apregContext->x25;
    ntctx->X[26] = apregContext->x26;
    ntctx->X[27] = apregContext->x27;
    ntctx->X[28] = apregContext->x28;
       
    ntctx->Fp = apregContext->x29;
    ntctx->Lr = apregContext->x30;
    ntctx->Pc = apregContext->pc;
    
    ntctx->Sp = apregContext->sp_el3;
    
    return hr;
}

HRESULT
ConvertAPREGContextToARMContext(
    _In_ PSDI_CPU32_CTXT_REGS_TYPE apregContext,
    _Out_ PARM_CONTEXT             ntctx
)
{
    ULONG       mode;
    HRESULT     hr = S_OK;;

    memset(ntctx, 0, sizeof *ntctx);
    
    

    //
    // The context is captured in monitor mode, so look at
    // mon_lr to see the value of pc at the time of the interrupt
    // and mon_spsr to see the value of cpsr
    //

    ntctx->ContextFlags = ARM_CONTEXT_INTEGER | ARM_CONTEXT_CONTROL;
    ntctx->R0 = (ULONG)apregContext->r0;
    ntctx->R1 = (ULONG)apregContext->r1;
    ntctx->R2 = (ULONG)apregContext->r2;
    ntctx->R3 = (ULONG)apregContext->r3;
    ntctx->R4 = (ULONG)apregContext->r4;
    ntctx->R5 = (ULONG)apregContext->r5;
    ntctx->R6 = (ULONG)apregContext->r6;
    ntctx->R7 = (ULONG)apregContext->r7;
    ntctx->R8 = (ULONG)apregContext->r8;
    ntctx->R9 = (ULONG)apregContext->r9;
    ntctx->R10 = (ULONG)apregContext->r10;
    ntctx->R11 = (ULONG)apregContext->r11;
    ntctx->R12 = (ULONG)apregContext->r12;
    ntctx->Pc = (ULONG)apregContext->pc;
    ntctx->Cpsr = (ULONG)apregContext->cpsr;

    //
    // Read sp and lr from the banked registers of whichever mode we were
    // in (plus r8-r12 if it was fiq mode)
    //
    mode = apregContext->cpsr & 0x1f;

    switch (mode)
    {
    case usr_mode:
        ntctx->Sp = (ULONG)apregContext->r13_usr;
        ntctx->Lr = (ULONG)apregContext->r14_usr;
        break;

    case fiq_mode:
        ntctx->R8 = (ULONG)apregContext->r8_fiq;
        ntctx->R9 = (ULONG)apregContext->r9_fiq;
        ntctx->R10 = (ULONG)apregContext->r10_fiq;
        ntctx->R11 = (ULONG)apregContext->r11_fiq;
        ntctx->R12 = (ULONG)apregContext->r12_fiq;
        ntctx->Sp = (ULONG)apregContext->r13_fiq;
        ntctx->Lr = (ULONG)apregContext->r14_fiq;
        break;

    case irq_mode:
        ntctx->Sp = (ULONG)apregContext->r13_irq;
        ntctx->Lr = (ULONG)apregContext->r14_irq;
        break;

    case svc_mode:
        ntctx->Sp = (ULONG)apregContext->r13_svc;
        ntctx->Lr = (ULONG)apregContext->r14_svc;
        break;

    case abt_mode:
        ntctx->Sp = (ULONG)apregContext->r13_abt;
        ntctx->Lr = (ULONG)apregContext->r14_abt;
        break;

    case und_mode:
        ntctx->Sp = (ULONG)apregContext->r13_und;
        ntctx->Lr = (ULONG)apregContext->r14_und;
        break;

    default:
        TraceInfo1("Unrecognised processor mode in CPU context.", "Mode", mode);
        // TODO: There is an known issue where CPSR value is not retained accross
        // resets. Currently not bailing out because of this. 
        // Setting the default values to user mode. 
        //hr = E_UNEXPECTED;
        break;
    }
    
    return hr;
}

//
// ---------------------- helper functions ---------------------------
//
VOID 
dumpAPREGDumpTable(
PAP_REG_MSM_DUMP_TABLE dump_table
)
{
    UINT32 index = 0;
    
    wprintf(L"=======DUMPING MSM_DUMP_TABLE==========\n\r");
    wprintf(L"Version: %d\n", dump_table->Version);
    wprintf(L"Num_Entries: %d\n", dump_table->Num_Entries);
    
    for(index = 0; index < dump_table->Num_Entries; index++) {
        wprintf(L"Entry %d\n", index);
        wprintf(L"id: 0x%x\n", dump_table->Entries[index].id);
        wprintf(L"Name (addr): 0x%p\n", &(dump_table->Entries[index].Name));
        wprintf(L"type %d\n", dump_table->Entries[index].type);
        wprintf(L"Address 0x%I64x \n", dump_table->Entries[index].Address);
        wprintf(L"\n");
    } 
    wprintf(L"========================================\n\n\r");
}


VOID 
dumpAPREGDumpData(
PAP_AP_REG_MSM_DUMP_DATA dump_data
)
{
    wprintf(L"=======DUMPING MSM_DUMP_DATA==========\n\r");
    wprintf(L"version: 0x%x\n", dump_data->version);
    wprintf(L"Magic: 0x%x\n", dump_data->Magic);
    wprintf(L"name: %s\n", dump_data->name);
    wprintf(L"address: 0x%I64x\n", dump_data->address);
    wprintf(L"len: 0x%I64x\n", dump_data->len);
    wprintf(L"reserved: 0x%I64x\n", dump_data->reserved);
    wprintf(L"========================================\n\n\r");
}

VOID 
dumpARM64Context(
PSDI_CPU64_CTXT_REGS_TYPE arm64Context
)
{

  wprintf(L"DUMPING the CPU CONTEXT, SIZE : %d\n", sizeof(*arm64Context));
  wprintf(L"x0=0x%I64x\n", arm64Context->x0);
  wprintf(L"x1=0x%I64x\n", arm64Context->x1);
  wprintf(L"x2=0x%I64x\n", arm64Context->x2);
  wprintf(L"x3=0x%I64x\n", arm64Context->x3);
  wprintf(L"x4=0x%I64x\n", arm64Context->x4);
  wprintf(L"x5=0x%I64x\n", arm64Context->x5);
  wprintf(L"x6=0x%I64x\n", arm64Context->x6);
  wprintf(L"x7=0x%I64x\n", arm64Context->x7);
  wprintf(L"x8=0x%I64x\n", arm64Context->x8);
  wprintf(L"x9=0x%I64x\n", arm64Context->x9);
  wprintf(L"x10=0x%I64x\n", arm64Context->x10);
  wprintf(L"x11=0x%I64x\n", arm64Context->x11);
  wprintf(L"x12=0x%I64x\n", arm64Context->x12);
  wprintf(L"x13=0x%I64x\n", arm64Context->x13);
  wprintf(L"x14=0x%I64x\n", arm64Context->x14);
  wprintf(L"x15=0x%I64x\n", arm64Context->x15);
  wprintf(L"x16=0x%I64x\n", arm64Context->x16);
  wprintf(L"x17=0x%I64x\n", arm64Context->x17);
  wprintf(L"x18=0x%I64x\n", arm64Context->x18);
  wprintf(L"x19=0x%I64x\n", arm64Context->x19);
  wprintf(L"x20=0x%I64x\n", arm64Context->x20);
  wprintf(L"x21=0x%I64x\n", arm64Context->x21);
  wprintf(L"x22=0x%I64x\n", arm64Context->x22);
  wprintf(L"x23=0x%I64x\n", arm64Context->x23);
  wprintf(L"x24=0x%I64x\n", arm64Context->x24);
  wprintf(L"x25=0x%I64x\n", arm64Context->x25);
  wprintf(L"x26=0x%I64x\n", arm64Context->x26);
  wprintf(L"x27=0x%I64x\n", arm64Context->x27);
  wprintf(L"x28=0x%I64x\n", arm64Context->x28);
  wprintf(L"x29=0x%I64x\n", arm64Context->x29);
  wprintf(L"x30=0x%I64x\n", arm64Context->x30);
  wprintf(L"pc=0x%I64x\n", arm64Context->pc);
  wprintf(L"currentEL=0x%I64x\n", arm64Context->currentEL);
  wprintf(L"sp_el3=0x%I64x\n", arm64Context->sp_el3);
  wprintf(L"elr_el3=0x%I64x\n", arm64Context->elr_el3);
  wprintf(L"spsr_el3=0x%I64x\n", arm64Context->spsr_el3);
  wprintf(L"sp_el2=0x%I64x\n", arm64Context->sp_el2);
  wprintf(L"elr_el2=0x%I64x\n", arm64Context->elr_el2);
  wprintf(L"spsr_el2=0x%I64x\n", arm64Context->spsr_el2);
  wprintf(L"sp_el1=0x%I64x\n", arm64Context->sp_el1);
  wprintf(L"elr_el1=0x%I64x\n", arm64Context->elr_el1);
  wprintf(L"spsr_el1=0x%I64x\n", arm64Context->spsr_el1);
  wprintf(L"sp_el0=0x%I64x\n", arm64Context->sp_el0);
  wprintf(L"__reserved1=0x%I64x\n", arm64Context->__reserved1);
  wprintf(L"__reserved2=0x%I64x\n", arm64Context->__reserved2);
  wprintf(L"__reserved3=0x%I64x\n", arm64Context->__reserved3);
  wprintf(L"__reserved4=0x%I64x\n\n", arm64Context->__reserved4);
}


VOID 
dumpARM32Context(
PSDI_CPU32_CTXT_REGS_TYPE arm32Context
)
{

  wprintf(L"DUMPING the CPU CONTEXT, SIZE : %d\n", sizeof(*arm32Context));
  wprintf(L"r0=0x%I64x\n", arm32Context->r0);
  wprintf(L"r1=0x%I64x\n", arm32Context->r1);
  wprintf(L"r2=0x%I64x\n", arm32Context->r2);
  wprintf(L"r3=0x%I64x\n", arm32Context->r3);
  wprintf(L"r4=0x%I64x\n", arm32Context->r4);
  wprintf(L"r5=0x%I64x\n", arm32Context->r5);
  wprintf(L"r6=0x%I64x\n", arm32Context->r6);
  wprintf(L"r7=0x%I64x\n", arm32Context->r7);
  wprintf(L"r8=0x%I64x\n", arm32Context->r8);
  wprintf(L"r9=0x%I64x\n", arm32Context->r9);
  wprintf(L"r10=0x%I64x\n", arm32Context->r10);
  wprintf(L"r11=0x%I64x\n", arm32Context->r11);
  wprintf(L"r12=0x%I64x\n", arm32Context->r12);  
  wprintf(L"r13_usr=0x%I64x\n", arm32Context->r13_usr);
  wprintf(L"r14_usr=0x%I64x\n", arm32Context->r14_usr);  
  wprintf(L"r13_hyp=0x%I64x\n", arm32Context->r13_hyp);  
  wprintf(L"r13_irq=0x%I64x\n", arm32Context->r13_irq);
  wprintf(L"r14_irq=0x%I64x\n", arm32Context->r14_irq);  
  wprintf(L"r13_svc=0x%I64x\n", arm32Context->r13_svc);
  wprintf(L"r14_svc=0x%I64x\n", arm32Context->r14_svc);  
  wprintf(L"r13_abt=0x%I64x\n", arm32Context->r13_abt);
  wprintf(L"r14_abt=0x%I64x\n", arm32Context->r14_abt);  
  wprintf(L"r13_und=0x%I64x\n", arm32Context->r13_und);
  wprintf(L"r14_und=0x%I64x\n", arm32Context->r14_und);  
  wprintf(L"r9_fiq=0x%I64x\n", arm32Context->r9_fiq);
  wprintf(L"r10_fiq=0x%I64x\n", arm32Context->r10_fiq);
  wprintf(L"r11_fiq=0x%I64x\n", arm32Context->r11_fiq);
  wprintf(L"r12_fiq=0x%I64x\n", arm32Context->r12_fiq);
  wprintf(L"r13_fiq=0x%I64x\n", arm32Context->r13_fiq);
  wprintf(L"r14_fiq=0x%I64x\n", arm32Context->r14_fiq);  
  wprintf(L"pc=0x%I64x\n", arm32Context->pc);  
  wprintf(L"cpsr=0x%I64x\n", arm32Context->cpsr);  
  wprintf(L"r13_mon=0x%I64x\n", arm32Context->r13_mon);
  wprintf(L"r14_mon=0x%I64x\n", arm32Context->r14_mon);
  wprintf(L"r14_hyp=0x%I64x\n", arm32Context->r14_hyp);  
  wprintf(L"_reserved=0x%I64x\n", arm32Context->_reserved);
  wprintf(L"__reserved1=0x%I64x\n", arm32Context->__reserved1);
  wprintf(L"__reserved2=0x%I64x\n", arm32Context->__reserved2);
  wprintf(L"__reserved3=0x%I64x\n", arm32Context->__reserved3);
  wprintf(L"__reserved4=0x%I64x\n\n", arm32Context->__reserved4);  
}


VOID
dumpAllARMContexts(
VOID
)
{
    UINT32        index;
    PCPUContext64 currentProcContext;
    //traverse through the list.
    
    wprintf(L"DUMPING PROCESSOR CONTEXTS FOR %d PROCERROS ",
        gAPRegCPUConextList.numProcressors);
    
    currentProcContext = gAPRegCPUConextList.listHead;
    for(index = 0; index < gAPRegCPUConextList.numProcressors; index++) {
        wprintf(L"PROCESSOR %d :-\n", index);
        dumpARM32Context(&(currentProcContext->cpuContext.Cpu_regs.Cpu32_ctxt));
        currentProcContext = currentProcContext->Flink;
    }  

}

/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:
    apreg.cpp

Environment:
    User Mode

Note:
  AP_REG is a Qualcomm-specific structure

--*/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include "dumputil.h"


HRESULT GetAPRegLegacy(_Inout_ PDMP_CONTEXT Context)
/*++

    Routine Description:

    This function retrieves the AP_REG header from the raw dump file at the location
    that was specified in the XML.

    The region is valid if:

    1. Magic value match.
    2. Version match.
    3. CPU Count is non-zero.

    Arguments:
        Context - PDMP_CONTEXT

    Updates:
        Context->ApReg
        Context->TotalCpuContextSizeInBytes
        Context->DumpHeaderStatus

    Return Value:
        NT status code.

--*/
{
    AP_REG_B_FAMILY_HEADER  apRegHeader;
    UINT32                  dataSize;
    NTSTATUS                status = STATUS_UNSUCCESSFUL;

    if (0 == Context->APRegAddress.QuadPart) {
        TraceNTSTATUS("AP_REG address is invalid", status);
        goto Exit;
    }

    TraceInfo1("AP_REG", "Address", Context->APRegAddress.QuadPart);

    status = ReadFromDDRSectionByPhysicalAddress(
                 Context,
                 Context->APRegAddress,
                 sizeof(AP_REG_B_FAMILY_HEADER),
                 &apRegHeader
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Read AP_REG failed", status);
        goto Exit;
    }

    //
    // Check signature.
    //
    if (apRegHeader.Magic != AP_REG_STRUCTURE_MAGIC_VALUE) {
        TraceExpectedActual("Unexpected AP_REG magic value", AP_REG_STRUCTURE_MAGIC_VALUE, apRegHeader.Magic);
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    //
    // Check version.
    //
    switch (apRegHeader.Version) {
    case AP_REG_STRUCTURE_V2:
    case AP_REG_STRUCTURE_V3:
    case AP_REG_STRUCTURE_V4:
        break;

    default:
        TraceInfo1("Unexpected AP_REG version", "Version", apRegHeader.Version);
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    //
    // Check CPU count.
    //
    if ((apRegHeader.CPU_Count == 0) || (apRegHeader.CPU_Count > AP_REG_MAX_CPUS)) {
        TraceInfo1("Unexpected AP_REG CPU count", "Actual", apRegHeader.CPU_Count);
        status = STATUS_BAD_DATA;
        goto Exit;
    }

    //
    // Calculate and retrieve the rest of the content. 
    // Includes an APREG header and a secure context, plus 3 different structs for each CPU
    //
    dataSize = sizeof(AP_REG_B_FAMILY_HEADER)+ sizeof(SECURE_CPU_CONTEXT) +
               (apRegHeader.CPU_Count * (sizeof(CPU_STATUS)+sizeof(NON_SECURE_CPU_CONTEXT)+sizeof(WDOG_STATUS)));

    //
    // Allocate the buffer...
    //
    ASSERT(Context->ApReg == nullptr);
    Context->ApReg = (PAP_REG_B_FAMILY_HEADER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dataSize);

    if (Context->ApReg == nullptr) {
        status = STATUS_NO_MEMORY;
        TraceNTSTATUS("Failed to allocate memory for AP_REG", status);
        goto Exit;
    }

    status = ReadFromDDRSectionByPhysicalAddress(
                 Context,
                 Context->APRegAddress,
                 dataSize,
                 Context->ApReg
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Read AP_REG failed", status);
        goto Exit;
    }

    Context->TotalCpuContextSizeInBytes = dataSize;

    status = STATUS_SUCCESS;

Exit:
    if (FAILED(status)) {
        Context->DumpHeaderStatus = DHS_NO_SVINFO;
    }
    return HRESULT_FROM_NT(status);
}


NTSTATUS
TZBSPContextToNTContext(
    _In_ PSECURE_CPU_CONTEXT tzctx,
    _Out_ PARM_CONTEXT ntctx
    )
{
    ULONG       mode;
    NTSTATUS    status = STATUS_SUCCESS;

    memset(ntctx, 0, sizeof *ntctx);

    //
    // The context is captured in monitor mode, so look at
    // mon_lr to see the value of pc at the time of the interrupt
    // and mon_spsr to see the value of cpsr
    //

    ntctx->ContextFlags = ARM_CONTEXT_INTEGER | ARM_CONTEXT_CONTROL;
    ntctx->R0 = tzctx->Usr_R0;
    ntctx->R1 = tzctx->Usr_R1;
    ntctx->R2 = tzctx->Usr_R2;
    ntctx->R3 = tzctx->Usr_R3;
    ntctx->R4 = tzctx->Usr_R4;
    ntctx->R5 = tzctx->Usr_R5;
    ntctx->R6 = tzctx->Usr_R6;
    ntctx->R7 = tzctx->Usr_R7;
    ntctx->R8 = tzctx->Usr_R8;
    ntctx->R9 = tzctx->Usr_R9;
    ntctx->R10 = tzctx->Usr_R10;
    ntctx->R11 = tzctx->Usr_R11;
    ntctx->R12 = tzctx->Usr_R12;
    ntctx->Pc = tzctx->Mon_Lr;
    ntctx->Cpsr = tzctx->Mon_Spsr;

    //
    // Read sp and lr from the banked registers of whichever mode we were
    // in (plus r8-r12 if it was fiq mode)
    //
    mode = tzctx->Mon_Spsr & 0x1f;

    switch (mode) {
    case usr_mode:
        ntctx->Sp = tzctx->Usr_R13;
        ntctx->Lr = tzctx->Usr_R14;
        break;

    case fiq_mode:
        ntctx->R8 = tzctx->Fiq_R8;
        ntctx->R9 = tzctx->Fiq_R9;
        ntctx->R10 = tzctx->Fiq_R10;
        ntctx->R11 = tzctx->Fiq_R11;
        ntctx->R12 = tzctx->Fiq_R12;
        ntctx->Sp = tzctx->Fiq_R13;
        ntctx->Lr = tzctx->Fiq_R14;
        break;

    case irq_mode:
        ntctx->Sp = tzctx->Irq_R13;
        ntctx->Lr = tzctx->Irq_R14;
        break;

    case svc_mode:
        ntctx->Sp = tzctx->Svc_R13;
        ntctx->Lr = tzctx->Svc_R14;
        break;

    case abt_mode:
        ntctx->Sp = tzctx->Abt_R13;
        ntctx->Lr = tzctx->Abt_R14;
        break;

    case und_mode:
        ntctx->Sp = tzctx->Und_R13;
        ntctx->Lr = tzctx->Und_R14;
        break;

    default:
        status = STATUS_UNSUCCESSFUL;
        TraceNTSTATUS("Unrecognized processor mode in CPU context", status);
        break;
    }

    return status;
}


NTSTATUS UpdateContextWithAPRegLegacy(_Inout_ PDMP_CONTEXT Context)
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
    PARM_CONTEXT        armContext = nullptr;
    UINT32              contextVA = 0;
    UINT32              dataSize = 0;
    UINT32              indexProcessor = 0;
    PKDDEBUGGER_DATA64  kdDebuggerDataBlock;
    UINT32              kiProcessorBlockVA = 0;
    UINT32              offsetPrcbContext = 0;
    UINT32              prcbAddressVA = 0;
    UINT32              processorCount = 0;
    PSECURE_CPU_CONTEXT secureContext = nullptr;
    NTSTATUS            status = STATUS_UNSUCCESSFUL;
    UINT32              tempVA = 0;
    PCPU_STATUS         cpustatus;
    LARGE_INTEGER       ArmContextPA;
    LARGE_INTEGER       physicalAddress;
    LARGE_INTEGER       ContextPAOffset;
    PLARGE_INTEGER      ContextPA = nullptr;

    //
    // Check prereqs...
    //
    TraceInfo("Checking prerequisites");

    if (Context->KdDebuggerDataBlock == nullptr) {
        status = STATUS_UNSUCCESSFUL;
        TraceNTSTATUS("Unable to update APREG. KdDebuggerDataBlock not available", status);
        goto Exit;
    }

    //
    // Allocate the space for context. 
    //
    dataSize = sizeof(ARM_CONTEXT);
    armContext = (ARM_CONTEXT*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, dataSize);
    if (armContext == nullptr) {
        status = STATUS_NO_MEMORY;
        TraceNTSTATUS("Failed to allocate memory for CONTEXT", status);
        goto Exit;
    }

    //
    // Get the addresses of various variables we will need.
    //
    kdDebuggerDataBlock = Context->KdDebuggerDataBlock;
    kiProcessorBlockVA = (UINT32)(kdDebuggerDataBlock->KiProcessorBlock);
    offsetPrcbContext = (UINT32)(kdDebuggerDataBlock->OffsetPrcbContext);
    processorCount = Context->DumpHeader32->NumberProcessors;

    TraceInfo3("Context", "KiProcessorBlock", kiProcessorBlockVA, "OffsetPrcbContext", offsetPrcbContext,
                          "ProcessorCount", processorCount);
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
            RtlZeroMemory(armContext, dataSize);

            // Read the Physical address where you would get the 
            status = ReadFromDDRSectionByPhysicalAddress(Context,
                ContextPA[indexProcessor],
                sizeof(ARM_CONTEXT),
                armContext);

            if (!NT_SUCCESS(status)) {
                TraceNTSTATUS("Failed to read the address for CONTEXT", status);
                goto Exit;
            }

            ArmContextPA = ContextPA[indexProcessor];
        }
        else {
            //
            // Calculate the virtual address of the location containing the PRCB virtual address.
            //
            tempVA = kiProcessorBlockVA + (sizeof(UINT32)* indexProcessor);

            TraceInfo2("Address of PRCB", "CPU", indexProcessor, "VA", tempVA);

            status = ReadFromDDRSectionByVirtualAddress32(
                Context,
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

            TraceInfo2("Context address", "CPU", indexProcessor, "VA", tempVA);

            status = ReadFromDDRSectionByVirtualAddress32(
                Context,
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

            //
            // Read the CONTEXT
            //
            status = ReadFromDDRSectionByVirtualAddress32(
                Context,
                contextVA,
                dataSize,
                armContext,
                &ArmContextPA);
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to read the address for CONTEXT", status);
                goto Exit;
            }
        }

        TraceInfo2("ArmContext", "CPU", indexProcessor, "PA", ArmContextPA.QuadPart);

        cpustatus = (PCPU_STATUS)Add2Ptr(Context->ApReg, sizeof(AP_REG_B_FAMILY_HEADER)+indexProcessor * sizeof(CPU_STATUS));

        // If SC_STATUS_WARM_BOOT is set, then AP_REG is not valid as it was in power collapse.
        if (!(
               ((cpustatus->Status & SC_STATUS_WARM_BOOT|SC_STATUS_SGI) == (SC_STATUS_WARM_BOOT|SC_STATUS_SGI))
              || cpustatus->Status & SC_STATUS_DBI_RSVD
              || ((cpustatus->Status & SC_STATUS_NS|SC_STATUS_WDT|SC_STATUS_SGI|SC_STATUS_WARM_BOOT) == 0)
           ))
        {

            if (Context->ApReg->Version == AP_REG_STRUCTURE_V4){
                TraceInfo("Starting version 4 AP_REG validation");

                //
                // For version 4 or higher, APREG is collected only if SC_STATUS_DBI_RSVD or SC_STATUS_CPU_CONTEXT 
                // is set. 
                //
                if (((cpustatus->Status & SC_STATUS_DBI_RSVD) == 0) &&
                    ((cpustatus->Status & SC_STATUS_CPU_CONTEXT) == 0)) {
                    TraceInfo1("Skipping AP_REG update. Neither SC_STATUS_DBI_RSVD nor SC_STATUS_CPU_CONTEXT are set",
                        "CPU", indexProcessor);
                    status = STATUS_UNSUCCESSFUL;
                    continue;
                }

            }

            //
            // Update CONTEXT
            //
            secureContext = (PSECURE_CPU_CONTEXT)Add2Ptr(Context->ApReg,
                            (sizeof(AP_REG_B_FAMILY_HEADER)+
                            (Context->ApReg->CPU_Count * sizeof(CPU_STATUS)) +
                            (indexProcessor * sizeof(NON_SECURE_CPU_CONTEXT))));

            TraceInfo2("Updating context", "CPU", indexProcessor, "SecureContext", (PVOID)secureContext);

            status = TZBSPContextToNTContext(secureContext, armContext);
            if (FAILED(status)) {
                TraceNTSTATUS("Update CONTEXT with AP Reg Failed", status);
                status = STATUS_SUCCESS; // Allow continuation
                continue;
            }
        }

        //
        // Write the CONTEXT to dump file.
        //
        status = WriteToDumpByPhysicalAddress(Context,
            ArmContextPA,
            dataSize,
            armContext);

        if (FAILED(status)) {
            TraceNTSTATUS("Failed to update dump's CONTEXT", status);
            goto Exit;
        }

        TraceInfo1("Wrote updated CONTEXT to dump", "CPU", indexProcessor);
    } //for indexProcessor

Exit:

    if (armContext != nullptr) {
        HeapFree(GetProcessHeap(), NULL, armContext);
    }

    if (ContextPA != nullptr) {
        HeapFree(GetProcessHeap(), NULL, ContextPA);
    }


    return status;
}



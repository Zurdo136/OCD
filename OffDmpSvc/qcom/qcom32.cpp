/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name: 
    qcom32.cpp

Environment:
    User Mode

--*/

#include "qcom32.h"
#include "wpcrdmpsentinel.h"
#include <bugcodes.h>
#define NO_INTERFACE_DECL
#include <ntefi.h>
#undef NO_INTERFACE_DECL
#include "offdmpistream.h"
#include "buildparams.h"

//
// Converting all GUID to human readable form.
//
SV_SPECIFIC_NAME_TO_GUID GUIDToName[] = {
    { 0xAB3A051F, 0xEF0B, 0x4A5F, { 0xA7, 0x9A, 0x80, 0xC2, 0x43, 0xBA, 0x08, 0x48 }, "AP_REG" },
    { 0xD0A267A1, 0x9CA5, 0x471D, { 0x8E, 0x9C, 0x79, 0xC9, 0x86, 0xBE, 0x77, 0x77 }, "OCIMEM.BIN" },
    { 0x100B990B, 0x0F9B, 0x40B3, { 0x82, 0xEF, 0x06, 0x61, 0x4F, 0x53, 0x05, 0xFE }, "CODERAM.BIN" },
    { 0x82233308, 0xCE47, 0x4D52, { 0x92, 0x11, 0xF4, 0x2E, 0x89, 0x61, 0x8A, 0xF4 }, "DATARAM.BIN" },
    { 0x91A8C35C, 0xA340, 0x4F2E, { 0xB7, 0x27, 0x65, 0x39, 0x47, 0xDB, 0x9C, 0x76 }, "MSGRAM.BIN" },
    { 0x877F61E0, 0xA870, 0x4635, { 0x9F, 0x41, 0x33, 0x00, 0x53, 0x20, 0x26, 0x05 }, "LPM.BIN" },
    { 0x10D25EDD, 0x1558, 0x4B88, { 0xAB, 0x5C, 0xE8, 0x1E, 0x7F, 0x47, 0xDA, 0xD9 }, "PMIC_PON.BIN" },
    { 0xD0352E48, 0xE359, 0x459E, { 0x9B, 0xBF, 0x2E, 0x16, 0xE6, 0x28, 0xAC, 0xFB }, "RST_STAT.BIN" },
    { 0x066A56C8, 0xCE2A, 0x4686, { 0xB6, 0x10, 0x5B, 0xFC, 0x22, 0xD0, 0xC7, 0xAB }, "load.cmm" },
    { 0x0df632e9, 0x5c48, 0x43aa, { 0xb8, 0xbd, 0x5f, 0xf6, 0x18, 0x05, 0x02, 0x5f }, "rawdump.bin" },
    { 0x62fb2678, 0x933f, 0x4177, { 0x86, 0x29, 0xff, 0x3f, 0x70, 0x55, 0x02, 0xe3 }, "DDR_DATA.BIN" },
    { 0x6901D825, 0x0E25, 0x4D6C, { 0x8C, 0x11, 0xE0, 0xAB, 0x2E, 0x98, 0xCA, 0xEF }, "UNKNOWN" }
};


QCom32::QCom32(
    _In_ PDMP_CONTEXT Context) :
        m_Context(Context),
        m_ApReg(nullptr),
        m_AddressOfDummyFunction(0),
        m_InMemDiagBuffer(nullptr)
{
    m_APRegAddress.QuadPart = 0;
}


HRESULT 
QCom32::ProcessSVSpecific(VOID)
{
    HRESULT result = E_FAIL;
    bool    ValidateResult = FALSE;

    TraceInfo("=========== Finished getting dump instance. Get SV section specific info. ===========");
    result = GetSVSpecificInfo();
    if (!SUCCEEDED(result)) {
        TraceHRESULT(
            "ERROR: GetSVSpecificInfo Failed.", result);
        goto Exit;
    }

    //
    // Get the AP_REG data from memory and do some validation.
    //
    TraceInfo("=========== Finished getting SV specific data. Get AP_REG. ===========");
    ValidateResult = FALSE;

    result = GetAPReg(&ValidateResult);
    if (ValidateResult == TRUE) {
        m_Context->SBLDumpProgress.IsAPRegPresent = CHKLIST_TRUE;
    }
    else {
        //
        // APREG invalid is not a fatal error. Log the error and move on.  
        //
        TraceInfo("APREG is not valid \n\r");
    }

    if (!SUCCEEDED(result)) {
        TraceHRESULT("ERROR: Failed to get the AP_REG.", result);
        goto Exit;
    }
    

    TraceInfo("=========== AP_REG is well and good. Getting Diag Buffer now===========");
    result = BuildDiagBuffer();
    if (!SUCCEEDED(result)) {
        TraceHRESULT("ERROR: Failed to Build Diag buffer", result);
    }

    result = S_OK;

Exit:
    return result;

}


HRESULT
QCom32::GetSVSpecificInfo(VOID)
{
    const UINT32             guidToNameTableSize = (GetGUIDTableSize() / sizeof(GUIDToName[0]));
    PVOID                    tempBuffer = nullptr;
    HRESULT                  result = E_FAIL;

    tempBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (UINT32)m_Context->LargestSVSpecificSectionSize);
    if (tempBuffer == nullptr) {
        TraceInfo1("Failed to allocate intermediate buffer for writing SV specific sections.", "Size",
            m_Context->LargestSVSpecificSectionSize);
        goto Exit;
    }

    TraceInfo1("m_Context->LargestSVSpecificSectionSize", "SVSectionSize", m_Context->LargestSVSpecificSectionSize);

    //
    // Loop through the sections to find the matching guid in order to get more information 
    // about the reset reason.
    // 
   
    for (UINT32 sectionIndex = 0; sectionIndex < m_Context->RawDumpHeader->SectionsCount; sectionIndex++)
    {
        WCHAR                    sectionName[RAW_DUMP_SECTION_HEADER_NAME_LENGTH + 1] = {0};
        CHAR                     name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH] = {0};

        PRAW_DUMP_SECTION_HEADER section = 
            (PRAW_DUMP_SECTION_HEADER )&(m_Context->RawDumpHeader->SectionTable[sectionIndex]);

        if (section->Type == RAW_DUMP_SECTION_TYPE_SV_SPECIFIC)
        {
            BOOLEAN     foundName = FALSE;
            size_t      bytesRead = 0;
            size_t      namelength;
            UINT32      guidToNameIndex = 0;
            ULONGLONG   offset = section->Offset;

            RtlZeroMemory(tempBuffer, (UINT32)m_Context->LargestSVSpecificSectionSize);
            if ( FAILED(result = m_Context->hDisk.SetPos(offset)) || 
                 FAILED(result = m_Context->hDisk.Read((PCHAR)tempBuffer, (size_t)section->Size, &bytesRead))
               )
            {
                TraceHRESULT("Failed to read SV section from device.", result);
                goto Exit;
            }

            memcpy(name, (void*)&(section->Name), RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
            namelength = strlen(name);
            mbstowcs(sectionName, name, namelength);
            TraceInfo2("Processing SV section to secondary data", "sectionIndex", sectionIndex, "sectionName", sectionName);
            //
            // Find the matching GUID for the section name.
            //
            for (guidToNameIndex = 0; guidToNameIndex < guidToNameTableSize; guidToNameIndex++) {
                if (RtlEqualMemory(section->Name,
                        GUIDToName[guidToNameIndex].Name,
                        RAW_DUMP_SECTION_HEADER_NAME_LENGTH)) {
                    foundName = TRUE;
                    break;
                }
            }//for guidToNameIndex

            if (FALSE == foundName)    {
                //use UNKNOWN_GUID
                guidToNameIndex = guidToNameTableSize - 1;
            }

            //
            // Make a copy of PMIC_PON and RST_STAT
            //
            if (RtlEqualMemory(&GUIDToName[guidToNameIndex].Guid,
                    &PMIC_PON_GUID,
                    sizeof(GUID))) {
                TraceInfo("Making a copy of PMIC_PON...");

                RtlCopyMemory(&m_PmicPonBin[0],
                    tempBuffer,
                    QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH);

                DumpPmicPon(&m_PmicPonBin[0]);
                m_Context->SBLDumpProgress.IsPmicPonBinCopied = CHKLIST_TRUE;
            }
            else if (RtlEqualMemory(&GUIDToName[guidToNameIndex].Guid,
                &RST_STAT_GUID,
                sizeof(GUID)))     {
                TraceInfo("Making a copy of RST_STAT...");

                RtlCopyMemory(&m_RstStatBin[0],
                    tempBuffer,
                    QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH);

                DumpRstStat(&m_RstStatBin[0]);
                m_Context->SBLDumpProgress.IsRstStatBinCopied = CHKLIST_TRUE;
            }
        } // RAW_DUMP_SECTION_TYPE_SV_SPECIFIC
    }//for sectionindex
    result = S_OK;

Exit:

    if (tempBuffer != nullptr) {
        HeapFree(GetProcessHeap(), 0, tempBuffer);
    }

    return result;
}


HRESULT
QCom32::GetAPReg(
_Out_ bool* IsApRegValid
)
{
    AP_REG_B_FAMILY_HEADER  apRegHeader = {0};
    ULONG                   dataSize = sizeof(m_APRegAddress);
    NTSTATUS                status = STATUS_UNSUCCESSFUL;
    HRESULT                 result = E_FAIL;
    UNICODE_STRING          apRegAddressVarName;
    GUID                    guid = EFI_OFFLINE_CRASHDUMP_VARIABLES_GUID;

    // make 
    m_APRegAddress.QuadPart = 0;
    m_ApReg = nullptr;

    //
    // Get the physical address of AP_REG.
    //

    RtlInitUnicodeString(&apRegAddressVarName, OFFLINE_MEMORY_DUMP_AP_REG_ADDRESS_VAR_NAME);
    status = NtQuerySystemEnvironmentValueEx(&apRegAddressVarName,
        &guid,
        &m_APRegAddress.QuadPart,
        &dataSize,
        nullptr);

    if (!NT_SUCCESS(status)) {
        TraceNTSTATUS("NtQuerySystemEnvironmentValueEx failed. ", status);
        result = HRESULT_FROM_NT(status);
        goto Exit;
    }


    if (m_APRegAddress.QuadPart == 0) {
        TraceInfo("Error: AP_REG address is invalid!\n");
        result = E_UNEXPECTED;
        goto Exit;
    }
    
    TraceInfo1("AP_REG address is ", " Physical address", m_APRegAddress.QuadPart);

    result = ReadFromDDRSectionByPhysicalAddress(m_Context,
        m_APRegAddress,
        sizeof(AP_REG_B_FAMILY_HEADER),
        &apRegHeader);    

    if (!SUCCEEDED(result)) {
        TraceNTSTATUS("Read AP_REG failed", status);
        goto Exit;
    }
    
    //
    // Check signature. If it has a magic string, it is a Legacy type of 
    // APREG. Else it is the new 64 bit compatible structure.
    // We dont need any more validation here. Those can be done at the back end.
    //
    if (apRegHeader.Magic != AP_REG_STRUCTURE_MAGIC_VALUE) {
        
        //
        // Assuming the this is a 64 bit APREG structure.
        //
        m_Context->IsAPREG64Bit = TRUE;
    }

    *IsApRegValid = TRUE;
    result = S_OK;

Exit:

    return result;
}


HRESULT
QCom32::GetInMemDataInfo(VOID)
{
    ULONG                   dataSize = sizeof(IN_MEM_DATA_INFO);
    IN_MEM_DATA_INFO        dummy;
    UNICODE_STRING          inMemDataInfoVarName;
    NTSTATUS                status = STATUS_SUCCESS;
    HRESULT                 result = E_FAIL;
    GUID                    guid = WPDMP_SETTINGS_VENDOR_GUID;

    TraceInfo("Looking up IN_MEM_DATA_INFO.\r\n");

    m_Context->SBLDumpProgress.IsInMemDataInfoFound = CHKLIST_FALSE;
    RtlZeroMemory(&m_InMemDataInfo, dataSize);

    RtlInitUnicodeString(&inMemDataInfoVarName, IN_MEM_DATA_INFO_CACHED);
    status = NtQuerySystemEnvironmentValueEx(&inMemDataInfoVarName,
                &guid,
                &m_InMemDataInfo,
                &dataSize,
                nullptr);

    if (!NT_SUCCESS(status))
    {
        TraceNTSTATUS("NtQuerySystemEnvironmentValueEx Failed to query", status);
        result = HRESULT_FROM_NT(status);
        goto Exit;
    }

    if (dataSize != sizeof(IN_MEM_DATA_INFO))
    {
        TraceExpectedActual("Unexpected size returned for IN_MEM_DATA_INFO.",           
            dataSize,
            sizeof(IN_MEM_DATA_INFO));
        result = E_BAD_DATA;
        goto Exit;
    }

    //
    // Reset the UEFI variable to prevent possible build-to-build and backwards compat issues.
    //
    RtlZeroMemory(&dummy, dataSize);

#ifndef DISABLE_RESETTING_UEFI_VARS

    status = NtSetSystemEnvironmentValueEx(&inMemDataInfoVarName,
                &guid,
                &dummy,
                dataSize,
                (VARIABLE_ATTRIBUTE_NON_VOLATILE |
                VARIABLE_ATTRIBUTE_RUNTIME_ACCESS |
                VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS));

#endif

    if (!NT_SUCCESS(status))
    {
        TraceNTSTATUS("Failed to set WPDMP_IN_MEM_DATA_INFO UEFI variable.", status);
        result = HRESULT_FROM_NT(status);
        goto Exit;
    }

    TraceInfo3("m_InMemDataInfo", 
        "VA", m_InMemDataInfo.DataVA, 
        "PA", m_InMemDataInfo.DataPA.QuadPart,
        "Size", m_InMemDataInfo.Size
        );

    if (m_InMemDataInfo.Size == 0)
    {
        TraceInfo("Failed to validate m_InMemDataInfo. m_InMemDataInfo.Size is zero.");
        result = E_BAD_DATA;
        goto Exit;
    }

    if (m_InMemDataInfo.DataPA.QuadPart == 0)
    {
        TraceInfo("Failed to validate m_InMemDataInfo. m_InMemDataInfo.DataPA.QuadPart is zero.");
        result = E_BAD_DATA;
        goto Exit;
    }

    if (m_InMemDataInfo.DataVA == 0)
    {
        TraceInfo("Failed to validate m_InMemDataInfo. m_InMemDataInfo.DataVA is zero.");
        result = E_BAD_DATA;
        goto Exit;
    }

    m_Context->SBLDumpProgress.IsInMemDataInfoFound = CHKLIST_TRUE;
    result = S_OK;

Exit:
    return result;
}


HRESULT
QCom32::BuildDiagBuffer(VOID)
{
    HRESULT                     result = E_FAIL;
    PINMEM_DIAG_BUFFER          buffer = nullptr;
    UINT32                      bytesToRead = 0;
    UINT32                      cpuCount = 0;
    PCPU_STATUS                 cpuStatus = nullptr;
    PHYSICAL_ADDRESS            temp = { 0 };
    UCHAR                       tempSig[] = ONEFOURC_DIAG_BUFFER_SIGNATURE;

    TraceInfo("Building diag buffer.\r\n");
    m_Context->SBLDumpProgress.IsDiagBufferBuilt = CHKLIST_FALSE;

    //
    // Get the in mem data.
    //
    result = GetInMemDataInfo();
    if (!SUCCEEDED(result))
    {
        TraceHRESULT("Failed to get InMemData.", result);
        goto Exit;
    }

    if (m_InMemDataInfo.Size < sizeof(INMEM_DIAG_BUFFER))
    {
        TraceExpectedActual("The pre-allocated InMemData is not large enough.",
            m_InMemDataInfo.Size,
            sizeof(INMEM_DIAG_BUFFER));
        result = E_UNEXPECTED;
        goto Exit;
    }

    TraceInfo("Allocating memory for INMEM_DIAG_BUFFER.");

    buffer = (PINMEM_DIAG_BUFFER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(INMEM_DIAG_BUFFER));
    if (buffer == nullptr)
    {
        TraceInfo("Failed to allocate OneFourCBuffer.");
        goto Exit;
    }

    buffer->OneFourCBuffer.Version = ONEFOURC_DIAG_BUFFER_VERSION;
    //#if defined(_M_ARM)
    // TODO: find an alternative to _MoveFromCoprocessor. Causing illegal exception as of now.
    //buffer->OneFourCBuffer.PlatformID = _MoveFromCoprocessor(CP15_MIDR);
    //#else
    //
    // Should use cpuid here if this ever runs on PCAT.
    //
    buffer->OneFourCBuffer.PlatformID = (UINT32)-1;
    //#endif
    buffer->OneFourCBuffer.AdditionalData = Add2Ptr(m_InMemDataInfo.DataVA,
        FIELD_OFFSET(INMEM_DIAG_BUFFER, QCData));

    TraceInfo3("14CBuf-",
        "Version", buffer->OneFourCBuffer.Version,
        "PlatformID", buffer->OneFourCBuffer.PlatformID, 
        "AdditionalData", buffer->OneFourCBuffer.AdditionalData);

    //
    // First is to validate signature.
    //
    TraceInfo("Validating in mem data buffer signature...");
    temp.QuadPart = m_InMemDataInfo.DataPA.QuadPart;
    bytesToRead = ONEFOURC_DIAG_BUFFER_SIGNATURE_LENGTH;
    result = ReadFromDDRSectionByPhysicalAddress(m_Context,
                temp,
                bytesToRead,
                &buffer->OneFourCBuffer.Signature[0]);

    if (!SUCCEEDED(result))
    {
        TraceHRESULT("Failed to read signature from in memory structure.", result);
        goto Exit;
    }

    //
    // Validate signature.
    //
    if (!RtlEqualMemory(&buffer->OneFourCBuffer.Signature[0],
            &tempSig[0],
            ONEFOURC_DIAG_BUFFER_SIGNATURE_LENGTH)) {
        TraceInfo("Failed to validate in mem data signature.");
        result = E_BAD_DATA;
        goto Exit;
    }

    //
    // Now get the bugcheck data located after the Sentinel structure.
    //
    TraceInfo("Retrieve bugcheck data.");
    temp.QuadPart = m_InMemDataInfo.DataPA.QuadPart +
        sizeof(SENTINEL)+
        FIELD_OFFSET(DUMP_HEADER32, BugCheckCode);

    bytesToRead = sizeof(buffer->OneFourCBuffer.BugCheckData[0]) *
        ONEFOURC_DIAG_BUFFER_BUGCHECK_DATA_COUNT;

    result = ReadFromDDRSectionByPhysicalAddress(m_Context,
        temp,
        bytesToRead,
        &buffer->OneFourCBuffer.BugCheckData[0]);

    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to read bugcheck data from in memory structure.", result);
        goto Exit;
    }

    TraceInfo("Bugcheck data from in memory data");
    TraceInfo1("Code ", "Value", buffer->OneFourCBuffer.BugCheckData[0]);
    TraceInfo1("Param1", "Value", buffer->OneFourCBuffer.BugCheckData[1]);
    TraceInfo1("Param2", "Value", buffer->OneFourCBuffer.BugCheckData[2]);
    TraceInfo1("Param3", "Value", buffer->OneFourCBuffer.BugCheckData[3]);
    TraceInfo1("Param4", "Value", buffer->OneFourCBuffer.BugCheckData[4]);

    //
    // Get the dummy function address from DUMP_HEADER.Comments. 
    // This gets used when we update AP m_Context and when we aren't sure
    // if PC is valid.
    //
    TraceInfo("Retrieve bugcheck data.");
    temp.QuadPart = m_InMemDataInfo.DataPA.QuadPart +
        sizeof(SENTINEL)+
        FIELD_OFFSET(DUMP_HEADER32, Comment);

    bytesToRead = sizeof(m_AddressOfDummyFunction);

    result = ReadFromDDRSectionByPhysicalAddress(m_Context,
                temp,
                bytesToRead,
                &m_AddressOfDummyFunction);

    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to read dummy function address from in memory structure.",
            result);
    }
    else
    {
        TraceInfo1("Address of dummy function ", "Address", m_AddressOfDummyFunction);
    }

    //
    // Fill in additional data...
    //
    buffer->QCData.Version = QC_ADDITIONAL_DATA_VERSION;

    if (m_Context->SBLDumpProgress.IsAPRegPresent == CHKLIST_TRUE)
    {
        TraceInfo("Adding m_ApReg data...");

        cpuStatus = (PCPU_STATUS)Add2Ptr(m_ApReg, sizeof(AP_REG_B_FAMILY_HEADER));
        cpuCount = m_ApReg->CPU_Count;

        RtlCopyMemory(&buffer->QCData.ApRegScStatus[0],
            &cpuStatus[0],
            (cpuCount * sizeof(CPU_STATUS)));
    }

    TraceInfo("Adding reset status...");

    RtlCopyMemory(&buffer->QCData.RstStatBin[0],
        &m_RstStatBin[0],
        QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH);

    TraceInfo("Adding PMIC pon status...");

    RtlCopyMemory(&buffer->QCData.PmicPonBin[0],
        &m_PmicPonBin[0],
        QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH);

    m_InMemDiagBuffer = buffer;
    m_Context->SBLDumpProgress.IsDiagBufferBuilt = CHKLIST_TRUE;

    TraceInfo("Diag buffer built.");

Exit:
    if (m_Context->SBLDumpProgress.IsDiagBufferBuilt != CHKLIST_TRUE)
    {
        // Free the heap memory on failure
        if (buffer != nullptr)
        {
            HeapFree(GetProcessHeap(), 0, buffer);
            buffer = nullptr;
        }
    }
    return result;
}


VOID
QCom32::BuildBugCheckParams(VOID)
{
    UINT32                      cpuCount = 0;
    PCPU_STATUS                 cpuStatus = nullptr;
    UINT32                      index = 0;

    TraceInfo("Building bugcheck data...");

    //
    // Fill in the default values.
    //
    m_Context->DumpHeader->BugCheckCode = FATAL_ABNORMAL_RESET_ERROR;
    m_Context->DumpHeader->BugCheckParameter1 = ONEFOURC_VERSION1_PARAM1_DEFAULT;
    m_Context->DumpHeader->BugCheckParameter2 = ONEFOURC_VERSION1_PARAM2_DEFAULT;
    m_Context->DumpHeader->BugCheckParameter3 = ONEFOURC_VERSION1_PARAM3_DEFAULT;
    m_Context->DumpHeader->BugCheckParameter4 = (m_Context->SBLDumpProgress.IsDiagBufferBuilt == CHKLIST_TRUE) ?
        (UINT32)m_InMemDataInfo.DataVA :
        ONEFOURC_VERSION1_PARAM4_DEFAULT;

    //
    // Below is Qualcomm specific. Be sure to check ONEFOURC_DIAG_BUFFER.PlatformID 
    // if we start supporting multiple SVs.
    //

    if (m_Context->SBLDumpProgress.IsRstStatBinCopied == CHKLIST_FALSE)
    {
        TraceInfo("GCC_RESET_STATUS is not available. Unable to determine reset trigger.");
        goto CheckForFallback;
    }

    //
    // The code below tries to avoid any dependency on the DiagBuffer in case
    // it was not built for whatever reason.
    //

    //
    // Check if this is caused by thermal.
    //
    TraceInfo("Checking for thermal reset...");

    if (m_RstStatBin[0] & 0x8) {
        TraceInfo("Trigger is thermal reset.");

        m_Context->DumpHeader->BugCheckParameter1 = ONEFOURC_PARAM1_THERMAL;
        goto Exit;
    }
    else {
        TraceInfo("Not thermal.");
    }

    //
    // Check RST_STAT. Since extended power button hold 
    // is wired via MSM, RST_STAT will be zero.
    // So check if RST_STAT is zero. If RST_STAT is zero
    // and PON_WARM_RESET_REASON1 is 0x2, then
    // this is a extended power button hold.
    //
    TraceInfo("Checking for EPBH...");
    if (m_RstStatBin[0] == 0) {
        //
        // PON_WARM_RESET_REASON1 is the third byte.
        //
        if (m_Context->SBLDumpProgress.IsPmicPonBinCopied == CHKLIST_TRUE)
        {
            if (m_PmicPonBin[2] == 0x2)
            {
                TraceInfo("GCC_RESET_STATUS is zero and PS_HOLD bit is set. EPBH confirmed.\r\n");
                m_Context->DumpHeader->BugCheckParameter1 = ONEFOURC_PARAM1_USER_INITIATED;

                //
                // It is possible that the fallback is caused by an user who thought the 
                // device is hung and did a EPBH even though the device was just 
                // doing a bug check so check for that.
                //
                goto CheckForFallback;
            }
            else {
                TraceInfo("PS_HOLD not set in PON_WARM_RESET_REASON1. Not EPBH.\r\n");
            }
        }
        else {
            TraceInfo("Failed to determine if trigger is EPBH. PMIC_PON data not available.\r\n");
        }
    }
    else {
        TraceInfo("RST_STAT is not zero.\r\n");
    }

    //
    // Check if this is a watchdog bite
    //
    TraceInfo("Checking if watchdog bite...\r\n");
    if (m_RstStatBin[0] & 0x2) {
        TraceInfo("Watchdog bite. Check if secure or non-secure.\r\n");

        m_Context->DumpHeader->BugCheckParameter1 = ONEFOURC_PARAM1_WATCHDOG;
        m_Context->DumpHeader->BugCheckParameter2 = ONEFOURC_PARAM2_SECURE_WDOG;

        //
        // GCC_RESET_STATUS does not provide data to distinguish between a 
        // secure and non-secure bite. Loop through m_ApReg.sc_status and
        // consider this a secure bite if any core's sc_status.WDT bit is set.
        //
        if (m_Context->SBLDumpProgress.IsAPRegPresent == CHKLIST_TRUE)
        {
            if(!m_Context->IsAPREG64Bit)
            {            
                cpuStatus = (PCPU_STATUS)Add2Ptr(m_ApReg, sizeof(AP_REG_B_FAMILY_HEADER));
                cpuCount = m_ApReg->CPU_Count;

                for (index = 0; index < cpuCount; index++)
                {
                    //
                    // Non-secure bite if at least one core has WDT bit set.
                    //
                    if (cpuStatus[index].Status & SC_STATUS_WDT)
                    {
                        TraceInfo("Non-secure watchdog.");

                        m_Context->DumpHeader->BugCheckParameter2 = ONEFOURC_PARAM2_NONSECURE_WDOG;;

                        //
                        // Since the bug check fallback code uses the non-secure wdog bite to 
                        // trigger an offline dump, should go ahead and check if this is really a 
                        // fallback.
                        //
                        goto CheckForFallback;
                    }
                }
            }
            
            //
            // Not going to check for fallback for a secure bite since that indicates a TZ issue and should
            // be bucketed as such and not a fallback.
            //
            goto Exit;

            //
            // NOTE: We are not processing APREG64 on device side as of now since this would increase
            // computations to be performed on the device side. 
            //
        }
        else {
            TraceInfo("AP_REG is not valid. Unable to determine wdog trigger from AP_REG.");
        }
    }
    else {
        TraceInfo("Not a watchdog bite. RST_STAT.Watchdog is not set.");
    }

CheckForFallback:

    if (m_Context->SBLDumpProgress.IsDiagBufferBuilt == CHKLIST_TRUE)
    {
        //
        // Figure out if there are non-14c data in m_InMemDiagBuffer.
        // If there is, then that indicates that we got an offline dump
        // because we failed creating an online dump.
        //
        TraceInfo("Checking for failed bugcheck...");

        if ((m_InMemDiagBuffer->OneFourCBuffer.BugCheckData[0] !=
            FATAL_ABNORMAL_RESET_ERROR) &&
            (m_InMemDiagBuffer->OneFourCBuffer.BugCheckData[0] != 0))
        {
            TraceInfo1("Failed bugcheck.", "Original bugcheck code:",
                m_InMemDiagBuffer->OneFourCBuffer.BugCheckData[0]);
            //
            // This is a fallback scenario. Save the bugcheck code in param2 and the original
            // param1 value to param3 so that we can easily tell whether the fallback was due to 
            // someone mistaken a device in bugcheck with a hung device.
            //
            m_Context->DumpHeader->BugCheckParameter3 = m_Context->DumpHeader->BugCheckParameter1;
            m_Context->DumpHeader->BugCheckParameter2 = m_InMemDiagBuffer->OneFourCBuffer.BugCheckData[0];
            m_Context->DumpHeader->BugCheckParameter1 = ONEFOURC_PARAM1_FALLBACK;
            goto Exit;
        }
        else
        {
            TraceInfo("Not failed bugcheck.");
        }
    }
    else
    {
        TraceInfo("Diag buffer is not built. Unable to check bugcheck data to see if this is a bugcheck fallback.");
    }

Exit:
    TraceInfo("Final bugcheck code and params");
    TraceInfo1("Code ", "Value",  m_Context->DumpHeader->BugCheckCode);
    TraceInfo1("Param1", "Value", m_Context->DumpHeader->BugCheckParameter1);
    TraceInfo1("Param2", "Value", m_Context->DumpHeader->BugCheckParameter2);
    TraceInfo1("Param3", "Value", m_Context->DumpHeader->BugCheckParameter3);
    TraceInfo1("Param4", "Value", m_Context->DumpHeader->BugCheckParameter4);
}


HRESULT
QCom32::BuildInfoFile(VOID)
{
    HRESULT              hr;
    OffDmpIStream        StreamFromFile;
    CComPtr<IXmlWriter>  pWriter;    

    if (FAILED(hr = StreamFromFile.OpenFile(m_Context->RawDumpInfoPath))) {
        TraceHRESULT("Error creating file writer", hr);
        goto Exit;
    }

    if (FAILED(hr = CreateXmlWriter(__uuidof(IXmlWriter), (void**)&pWriter, NULL)))  {
        TraceHRESULT("Error creating xml writer", hr);
        goto Exit;
    }

    if (FAILED(hr = pWriter->SetOutput(static_cast<IUnknown *>(&StreamFromFile)))) {
        TraceHRESULT("Error setting output for writer", hr);
        goto Exit;
    }

    if (FAILED(hr = pWriter->SetProperty(XmlWriterProperty_Indent, TRUE)))    {
        TraceHRESULT("Error, Method: SetProperty XmlWriterProperty_Indent", hr);
        goto Exit;
    }
    
    if (FAILED(hr = pWriter->WriteStartDocument(XmlStandalone_Omit))){
        goto Exit;
    }

    //
    // Write to the File 
    // <DeviceSpecificInfo>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"DeviceSpecificInfo", NULL))) {
        goto Exit;
    }

    //
    // <DumpHeader> 
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"DumpHeader", NULL))) {
        goto Exit;
    }

    //
    // Write the Dump header Instance.
    // 
    // Instance="m_Context->DumpInstance.QuadPart"
    //  
    if (FAILED(hr = WriteUINT64AttrString(pWriter,
                        m_Context->DumpInstance.QuadPart,
                        L"Instance"))) {    
        goto Exit;
    }

    //
    // </DumpHeader>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        TraceHRESULT("Error, Method: WriteFullEndElement: DumpHeader", hr);
        goto Exit;
    }
    
    //
    // <SVSpecific>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"SVSpecific", NULL))) {
        goto Exit;
    }

    //
    // <Qualcomm>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"Qualcomm", NULL))) {
        goto Exit;
    }

    //
    // <APRegAddress>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"APRegAddress", NULL))) {
        goto Exit;
    }

    //
    // PA = "m_APRegAddress.QuadPart"
    //
    if (FAILED(hr = WriteUINT64AttrString(pWriter,
                        m_APRegAddress.QuadPart,
                        L"PA"))) {    
        goto Exit;
    }
    
    //
    // </APRegAddress>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    //
    // <InMemDiagBuffer>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"InMemDiagBuffer", NULL))) {
        goto Exit;
    }

    //
    //  VA="m_InMemDataInfo.DataVA" 
    //     
    if (FAILED(hr = WriteUINT64AttrString(pWriter,
                        (ULONG)m_InMemDataInfo.DataVA,
                        L"VA"))) {    
        goto Exit;
    }

    //
    // PA="m_InMemDataInfo.DataPA.QuadPart"
    //   
    if (FAILED(hr = WriteUINT64AttrString(pWriter,
                        m_InMemDataInfo.DataPA.QuadPart,
                        L"PA"))) {    
        goto Exit;
    }
    //
    // Size="m_InMemDataInfo.Size"
    // 
    if (FAILED(hr = WriteUINT64AttrString(pWriter,
                        m_InMemDataInfo.Size,
                        L"Size"))) {
        goto Exit;
    }

    //
    // </InMemDiagBuffer>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    //
    // </Qualcomm>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    //
    // </SVSpecific>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    //
    // <BugCheckParameters>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"BugCheckParameters", NULL))) {
        goto Exit;
    }
    
    //
    // BugcheckCode="m_Context->DumpHeader->BugCheckCode"
    //   
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
                        m_Context->DumpHeader->BugCheckCode,
                        L"BugCheckCode"))) {
        goto Exit;
    }

    //
    // Param1="m_Context->DumpHeader->BugCheckParameter1"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
                        m_Context->DumpHeader->BugCheckParameter1,
                        L"Param1"))) {
        goto Exit;
    }
    //
    // Param2="m_Context->DumpHeader->BugCheckParameter2"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
                        m_Context->DumpHeader->BugCheckParameter2,
                        L"Param2"))) {
        goto Exit;
    }

    //
    // Param3="m_Context->DumpHeader->BugCheckParameter3"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
                        m_Context->DumpHeader->BugCheckParameter3,
                        L"Param3"))) {
        goto Exit;
    }

    //
    // Param4="m_Context->DumpHeader->BugCheckParameter4"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
                        m_Context->DumpHeader->BugCheckParameter4,
                        L"Param4"))) {
        goto Exit;
    }

    //
    // </BugCheckParameters> 
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }
    
    //
    // </DeviceSpecificInfo> 
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    if (FAILED(hr = pWriter->WriteEndDocument())) {
        goto Exit;
    }
    hr = S_OK;
Exit:
    return hr;
}


HRESULT 
QCom32::BuildInfoBuffer(
    _Out_ PDEVICE_SPECIFIC_INFO DeviceSpecificInfo
)
{
    HRESULT hr = S_OK;

    if (nullptr == DeviceSpecificInfo)
    { // Fail if buffer points to null...
        hr = E_FAIL;
    }
    else
    {
        DeviceSpecificInfo->Type = ( (m_Context->Is64Bit) ? PROCESSOR_ARCHITECTURE_ARM64 : PROCESSOR_ARCHITECTURE_ARM );
        DeviceSpecificInfo->DumpHeaderInstanceID = m_Context->DumpInstance.QuadPart;
        DeviceSpecificInfo->APRegPA = m_APRegAddress.QuadPart;
        DeviceSpecificInfo->VA = (ULONGLONG)m_InMemDataInfo.DataVA;
        DeviceSpecificInfo->PA = m_InMemDataInfo.DataPA.QuadPart;
        DeviceSpecificInfo->Size = m_InMemDataInfo.Size;
        DeviceSpecificInfo->BugCheckCode = m_Context->DumpHeader->BugCheckCode;
        DeviceSpecificInfo->BugCheckParam1 = m_Context->DumpHeader->BugCheckParameter1;
        DeviceSpecificInfo->BugCheckParam2 = m_Context->DumpHeader->BugCheckParameter2;
        DeviceSpecificInfo->BugCheckParam3 = m_Context->DumpHeader->BugCheckParameter3;
        DeviceSpecificInfo->BugCheckParam4 = m_Context->DumpHeader->BugCheckParameter4;
    }

    return hr;
}

//
//----------------- Helper function -----------------------------
//
HRESULT
QCom32::WriteUINT64AttrString(
    _In_ CComPtr<IXmlWriter> pWriter,
    _In_ ULONGLONG           LongLongIntegerValue,
    _In_ LPCWSTR             AttributeName
)
{
    HRESULT hr;
    WCHAR   pszDest[30];
    if (FAILED(hr = StringCchPrintfW(pszDest, 
                        ARRAYSIZE(pszDest), 
                        L"0x%I64x", 
                        LongLongIntegerValue))){
        goto Exit;
    }
    if (FAILED(hr = pWriter->WriteAttributeString(NULL, AttributeName,
                                NULL, pszDest))) {
        goto Exit;
    }
Exit:
    return hr;
}


HRESULT
QCom32::WriteUINT32AttrString(
    _In_ CComPtr<IXmlWriter> pWriter,
    _In_ UINT32              IntegerValue,
    _In_ LPCWSTR             AttributeName
)
{
    HRESULT hr;
    WCHAR   pszDest[30];
    if (FAILED(hr = StringCchPrintfW(pszDest, 
                        ARRAYSIZE(pszDest), 
                        L"0x%x", 
                        IntegerValue))){
        goto Exit;
    }
    if (FAILED(hr = pWriter->WriteAttributeString(NULL, AttributeName,
                                NULL, pszDest))) {
        goto Exit;
    }
Exit:
    return hr;
}


UINT32
QCom32::GetGUIDTableSize(VOID)
{
    return sizeof(GUIDToName);
}


VOID
QCom32::DumpPmicPon(
    _In_reads_(QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH) PUCHAR PmicPonBin
)
{
    UINT32 index = 0;

    TraceInfo("PMICPON");
    for (index = 0; index < QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH; index++) {
        TraceInfo2("PMICPON", "Index", index, "Value", PmicPonBin[index]);
    }
}


VOID
QCom32::DumpRstStat(
    _In_reads_(QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH) PUCHAR RstStatBin
)
{
    UINT32 index = 0;

    TraceInfo("RST_STAT");
    for (index = 0; index < QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH; index++){
        TraceInfo2("RST_STAT", "Index", index, "Value", RstStatBin[index]);
    }
}


QCom32::~QCom32()
{
    //
    // Free the m_ApReg
    //
    if (m_ApReg != nullptr)
    {
        HeapFree(GetProcessHeap(), 0, m_ApReg);
        m_ApReg = nullptr;
    }

    //
    // Free the m_InMemDiagBuffer
    //
    if (m_InMemDiagBuffer != nullptr)
    {
        HeapFree(GetProcessHeap(), 0, m_InMemDiagBuffer);
        m_InMemDiagBuffer = nullptr;
    }
}

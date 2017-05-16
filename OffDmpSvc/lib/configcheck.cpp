/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
    configcheck.cpp

Environment:
    User Mode

--*/


#include "configcheck.h"
#include "buildparams.h"
#define NO_INTERFACE_DECL
#include <ntefi.h>
#include <ntefi.h>
#include <string>
#undef NO_INTERFACE_DECL

BOOL DirectoryNameFromFilePath(
    _Inout_  LPWSTR filePath
    );


/*++

Routine Description:

    This function an NVRAM variable wich is used to turns off the WpDmp.EFI driver.
    In case the UEFI driver is loaded, this flag will cause the driver to 
    immediately exit and not perform any work.

Arguments:

    none.

Return Value:

    NT status code.

--*/
NTSTATUS
SetWpDmpFlag(
    _In_    UEFI_SETTING    setVal
)
{
    NTSTATUS        status = STATUS_UNSUCCESSFUL;
    GUID            guid = EFI_OFFLINE_CRASHDUMP_VARIABLES_GUID;
    UNICODE_STRING  uefiVarName;
    UINT            uefiState;
    ULONG           uefiStateLength = sizeof(uefiState);
    BOOL            readFailed = FALSE;

    // DISABLE_KERNEL_MODE_OCD   L"WpDisableKernelModeOCD"
    RtlInitUnicodeString(&uefiVarName, DISABLE_KERNEL_MODE_OCD);

    status = NtQuerySystemEnvironmentValueEx(
                    &uefiVarName,
                    &guid,
                    &uefiState,
                    &uefiStateLength,
                    NULL);

    if (!NT_SUCCESS(status))
    {
        TraceNTSTATUS("NtQuerySystemEnvironmentValueEx() failed to read UEFI control flag", status);
        readFailed=TRUE;
    }
    
    if( (readFailed==TRUE) || (uefiState != (UINT)setVal) )
    {
        uefiState = (UINT)setVal;
        status = NtSetSystemEnvironmentValueEx(
                    &uefiVarName,
                    &guid,
                    &uefiState,
                    sizeof(uefiState),
                    ( VARIABLE_ATTRIBUTE_NON_VOLATILE | 
                      VARIABLE_ATTRIBUTE_RUNTIME_ACCESS | 
                      VARIABLE_ATTRIBUTE_BOOTSERVICE_ACCESS
                    )
                );

        if (!NT_SUCCESS(status))
        {
            TraceNTSTATUS("ZwSetSystemEnvironmentValueEx() failed to set UEFI control flag", status);
        }
        else
        {
            if ( setVal==TRUE )
            {
                TraceInfo("UEFI control flag was set: TRUE");
            }
            else
            {
                TraceInfo("UEFI control flag was set: FALSE");
            }
        }
    }

    return status;
}


NTSTATUS
IsOffDumpReady(
_Inout_     PDMP_CONTEXT Context
)
/*++

Routine Description:
This function does the following
(1) Determines offline crashdump verison
(2) Checks if OfflineDumpState is enabled
(3) Determine if the dump is expected

Arguments:
    Context

Return Value:
    NTSTATUS

--*/
{
    bool        isGood = FALSE;
    NTSTATUS    result = S_OK; // Compiler flagged this when uninitialized
    
    if( Context == nullptr ) {        
        TraceWIN32("Invalid Context parameter passed", GetLastError());
        goto Exit;
    }

    //
    // Get system privilege
    //
    result = EnablePrivilege(SE_SYSTEM_ENVIRONMENT_PRIVILEGE);
    if (!SUCCEEDED(result)) {
        TraceWIN32("Enable Privilege Failed", GetLastError());
        goto Exit;
    }

    TraceInfo("Privilege accquired");


    // This section of code will run every time this driver is run.
    // It checks the EFI driver flag and sets it to DISABLE so as to ensure
    // that legacy EFI driver does not run, if it is installed.
    result = SetWpDmpFlag( DISABLE_UEFI );
    if (!SUCCEEDED(result)) {
        // Failure is not an error, but need to log a trace message if it does.
        TraceHRESULT("Failed to disable the UEFI disable flag in NVRAM", result);
    }


    //
    // Determine whether SBL (SBL capture memory to disk) or 
    // UEFI (wpdmp.efi capturing the memory) dump is supported by the firmware.
    // Note: UEFI version of memory capture is not supported.
    //
    result = DetermineOfflineDumpVersion(&(Context->ConfigTable));
    if (!SUCCEEDED(result)) {
        TraceHRESULT("DetermineOfflineDumpVersion failed", result);
        goto Exit;
    }

    RtlZeroMemory(&Context->SBLDumpProgress, sizeof(Context->SBLDumpProgress));
    TraceInfo("Checking if offline dump is enabled");    
	result = CheckOfflineCrashdumpEnabled(Context, &isGood);
    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to get the OfflineDumpState from UEFI variables", result);
        goto Exit;
    }

    Context->SBLDumpProgress.IsDumpEnabled = (isGood) ? CHKLIST_TRUE : CHKLIST_FALSE;

    if (!isGood) {
        result = E_UNEXPECTED;
        TraceHRESULT("Offline dump is not enabled or initialized", result);
        goto Exit;
    }

    TraceInfo("Proceeding with SBL offline dump.");

    //
    // Check if we expect a dump.
    //
    TraceInfo("Checking if a raw dump is expected from firmware");   
    isGood = FALSE;
    result = IsSBLDumpExpected(Context, &isGood);
    if (!SUCCEEDED(result)) {    
        TraceHRESULT("Failed to determine if a dump is expected", result);
        goto Exit;
    }

    Context->SBLDumpProgress.IsDumpExpected = (isGood) ? CHKLIST_TRUE : CHKLIST_FALSE;
    if (!isGood){
        result = E_UNEXPECTED;
        TraceHRESULT("Not expecting a raw dump", result);
        goto Exit;
    }
    TraceInfo("Raw dump is Expected.");

Exit:
    return result;
}


HRESULT
EnablePrivilege(
    DWORD     dwPrivilege
)
/*++

Routine Description:
    This function changes the previleges of the program and elvates it to SE Privileges.
    This is required to access UEFI variables.

Arguments:

    Context - Pointer to the global contet.
    IsDumpExp - the value that indicates the whether raw dump is expected.

Return Value:
    NT status code.

--*/
{
    HRESULT result;
    HANDLE hTokenHandle;
    TOKEN_PRIVILEGES NewState;
    DWORD dwReturnLength;

    result = S_OK;
    hTokenHandle = nullptr;
    dwReturnLength = 0;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hTokenHandle)) {
        //
        // Can't open the process token
        //
        result = HRESULT_FROM_WIN32(GetLastError());
        TraceHRESULT("Failed to Open Process Token", result);
        goto error;
    }
    else {
        //
        //  OpenProcessToken succeeded. Check if the token in null 
        //
        if (hTokenHandle == nullptr) {
            result = HRESULT_FROM_WIN32(GetLastError());
            TraceHRESULT("hTokenHandle == NULL", result);
            goto error;
        }
    }

    NewState.PrivilegeCount = 1;
    NewState.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    NewState.Privileges[0].Luid = RtlConvertLongToLuid(dwPrivilege);

    if (!AdjustTokenPrivileges(hTokenHandle,
                                FALSE,
                                &NewState,
                                sizeof(TOKEN_PRIVILEGES),
                                NULL,
                                &dwReturnLength)) {
        result = HRESULT_FROM_WIN32(GetLastError());
        goto error;
    }
    
error:
    if (hTokenHandle != nullptr) {
        CloseHandle(hTokenHandle);
    }

    return result;
}


HRESULT
DetermineOfflineDumpVersion(
_Inout_  POFFLINE_CRASH_DUMP_CONFIGURATION_TABLE ConfigTable
)
/*++

Routine Description:

This routine will attempt to figure out which dump version is supported.

The logic checks for version B first. Will use version B if below are
validated.
1. Check if config table is present.
2. Check config table version.
3. Check OfflineMemoryDumpCapable

Code will move on to look for version A if the following checks out:
1. "legacy_flag_address" present.
2. "memory_capture_mode_address" present.

Arguments:

None

Return Value:

NT status code.

--*/
{
	NTSTATUS    status;
    HRESULT     result = E_FAIL;
    ULONG       returnedLength = 0;

    TraceInfo("Checking for SBL offline dump support.");

    //
    // Check for config table. If it is not present. Check for UEFI dump.
    //
	RtlZeroMemory( ConfigTable, sizeof(OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE) );
    status = NtQuerySystemInformation(SystemOfflineDumpConfigInformation,
        ConfigTable,
		sizeof(OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE),
        &returnedLength);

    if (!NT_SUCCESS(status))
    {
        TraceNTSTATUS("NTQuerySystemInformation failed to retrieve config table information", status);
        result = HRESULT_FROM_NT(status);
        goto Exit;
    }

    TraceInfo("Retrieved config table information.");

    //
    // Check for config table version. Check for UEFI dump if the version doesn't match.
    //
    TraceInfo("Checking config table version.");

    
    if (!ConfigTable->Version ||
        ConfigTable->Version > EFI_OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE_VERSION)
    {
        TraceExpectedActual("Configuration table version mismatch", EFI_OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE_VERSION, ConfigTable->Version);
        result = E_NOTIMPL;
        goto Exit;
    }

    //
    // Check OfflineMemoryDumpCapable
    // Only check for dedicated partition because that's what Qualcomm will support.
    // Check for UEFI dump if capability bit is not set.
    //
    TraceInfo("Checking reported dump capability.");

    if ((ConfigTable->OfflineMemoryDumpCapable &
        RAW_DUMP_SUPPORTED_VIA_DEDICATED_PARTITION) !=
        RAW_DUMP_SUPPORTED_VIA_DEDICATED_PARTITION)
    {
        TraceInfo1("Support offline dump via dedicated partition not reported", "OfflineMemoryDumpCapable",ConfigTable->OfflineMemoryDumpCapable);
        result = E_UNEXPECTED;
        goto Exit;
    }

    TraceInfo("Using SBL dump");
    result = S_OK;


Exit:    
    return result;
}

HRESULT
CheckOfflineCrashdumpEnabled(
  _Inout_     PDMP_CONTEXT Context,
  _Out_       bool* IsOfflineDumpEnabled
)
/*++
Routine Description:

    This function is responsible for reading three main registry values in 
    HKLM\System\CurrentControlSet\Control\CrashControl to decide if the offline 
    crashdump is enabled or not.

Arguments:
     IsOfflineDumpEnable - TRUE if offline dump is enabled and initialized.

Return Value:
    NTSTATUS

--*/
{
    UNREFERENCED_PARAMETER(Context);
    
    HRESULT hr = E_FAIL;
    HKEY    hk;
    ULONG   CrashDumpEnabled = 0;
    ULONG   DisablePrebootCrashDump = 0;
    ULONG   EnableOfflineDumps = 0;
    DWORD   regValSize = sizeof(ULONG);
    ULONG   result;
    
    
    *IsOfflineDumpEnabled = FALSE;
    
    result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      TEXT(DUMP_REGKEY),
                      0,
                      KEY_QUERY_VALUE,
                      &hk);
                            
    if(result != ERROR_SUCCESS) {
        TraceNTSTATUS("Failed to get DumpInstance.Use default.", result);
        goto Exit;
    }
    
    //
    // This is an existing CrashControl registry key entry which specifies if crash 
    // dump feature is enabled and the type of the crash dump.
    //
    result = RegQueryValueExW(hk,
                             TEXT(L"CrashDumpEnabled"),
                             NULL,
                             NULL,
                             (LPBYTE) &CrashDumpEnabled,
                             &regValSize);
                             
    if(result != ERROR_SUCCESS) {
        TraceInfo("Error: Cant read the regkey CrashDumpEnabled");
        hr = E_NOTIMPL;
        goto Exit;
    }
    
    TraceInfo1("Registry key read successful", "CrashDumpEnabled", CrashDumpEnabled);

    regValSize = sizeof(ULONG);
    
    //
    // This key disables offline crashdump when set to a non-zero value.
    //
    result = RegQueryValueExW(hk,
                              TEXT(L"DisablePrebootCrashDump"),
                              NULL,
                              NULL,
                              (LPBYTE) &DisablePrebootCrashDump,
                              &regValSize);
                             
    if(result != ERROR_SUCCESS) {
        TraceInfo("Error: Cant read the regkey DisablePrebootCrashDump");
        //
        // WpDisablePrebootCrashDump is an optional parameter. 
        // So dont bail out as of yet.
    }
    
    TraceInfo1("Registry key read successful", "DisablePrebootCrashDump", DisablePrebootCrashDump);

    regValSize = sizeof(ULONG);  

    //
    // EnableOfflineDumps reg value is used by the kernel to determine whether 
    // it will enable SBL offline dump or not. The value must be present and set to 1 
    // in order for the kernel to set the OfflineMemoryDumpUseCapability UEFI variable.
    //
    result = RegQueryValueExW(hk,
                              TEXT(L"EnableOfflineDumps"),
                              NULL,
                              NULL,
                              (LPBYTE) &EnableOfflineDumps,
                              &regValSize);
                             
    if(result != ERROR_SUCCESS) {
        TraceInfo("Error: Cant read the regkey EnableOfflineDumps");
    }
    
    TraceInfo1("Registry key read successful", "EnableOfflineDumps", EnableOfflineDumps);
    
    if(!BOOLIFY(DisablePrebootCrashDump) && BOOLIFY(EnableOfflineDumps)) {
        if(CrashDumpEnabled == DUMP_TYPE_FULL || CrashDumpEnabled == DUMP_TYPE_SUMMARY) {
            TraceInfo("Offline Crashdump is enabled from Registry");
            *IsOfflineDumpEnabled = TRUE;
            hr = S_OK;          
        }
    }

/*
** Using the regsitry key "DedicatedDumpFile" is no longer necessary, for the user mode OCD (Offline Crash Dump) feature.
** OCD file(s) created are temporary and will be erased by WER so as not toe linger on the device, consuming disk space 
** unnecessarily.
** The raw dump file (and others created by this DLL) are temporary and should be in the new location (C:\Data\CrashDump\)
** on newer devices. If the CrashDump partition is present, OCD will use that partition preferentially.
** The MainOS partition should not be used for OCD (temporary) files as they are large and can cause device and crash dump
** capture failures. Failed captures could also cause  the device into a low disk space situation.
** Devices with no CrashDump partition should use the Data partition to avoid filling the MainOS partition. The Data partition
** absorbs the additional space freed when the CrashDump partition is eliminated.
** In the interest of disk space, a check for the CrashDump partition will be performed. When the partition is not found, OCD
** will use the Data partition for temporary files (C:\Data\CrashDump\). If the CrashDump partition exists, on a device, OCD
** will use it as (C:\CrashDUmp\) where it is always mounted.
*/

    RegCloseKey(hk);    

 Exit:
    return hr; 
}

HRESULT
IsSBLDumpExpected(
    _Inout_         PDMP_CONTEXT Context,
    _Out_           bool* IsDumpExpected
)
/*++

Routine Description:

    This function determines if a raw dump is expected. A raw dump is expected if
    the following conditions are met:

    1. A defined bit in the Offline Crash Dump Configuration Table's
    AbnormalResetOccurred variable is set.
    2. A defined bit in OfflineMemoryDumpUseCapability UEFI variable is set.

Arguments:

Context - Pointer to the global contet.
    IsDumpExp - the value that indicates the whether raw dump is expected.

Return Value:
    NT status code.

--*/
{
    HRESULT result = S_OK; // Compiler flagged this when uninitialized
    
    RtlZeroMemory(&Context->IsDumpExpectedChkList, sizeof(Context->IsDumpExpectedChkList));
    if (Context->ConfigTable.AbnormalResetOccurred == 0)
    {
        TraceInfo("AbnormalResetOccurred is zero.");
        goto Exit;
    }
    TraceInfo("ConfigTable->AbnormalResetOccurred  is set");

    Context->IsDumpExpectedChkList.IsAbnormalResetNonZero = CHKLIST_TRUE;
    result = GetOfflineMemoryDumpUseCapability(&Context->OfflineMemoryDumpUseCapability);

    if (!SUCCEEDED(result))    {
        result = E_UNEXPECTED;
        TraceHRESULT("WpDmppGetOfflineMemoryDumpUseCapability failed.", result);
        goto Exit;
    }

    Context->IsDumpExpectedChkList.IsUseCapabilityPresent = CHKLIST_TRUE;

    if (Context->OfflineMemoryDumpUseCapability == 0) {
        result = E_UNEXPECTED;
        TraceHRESULT("OfflineMemoryDumpUseCapability is zero", result);
        goto Exit;
    }

    Context->IsDumpExpectedChkList.IsUseCapabilitySet = CHKLIST_TRUE;
    *IsDumpExpected = TRUE;    
Exit:

#ifdef ENABLE_REPLAY_MODE
    TraceInfo("ENABLE_REPLAY_MODE: Pretending that AbnormalResetOccurred is set.");
    *IsDumpExpected = TRUE;
#endif

    return result;
}



HRESULT 
GetOfflineMemoryDumpUseCapability(
    _Out_  PUINT32  OfflineMemoryDumpUseCapability
)
/*++

Routine Description:

    This function uses variable services to get the OfflineMemoryDumpUseCapability value.

Arguments:

    OfflineMemoryDumpUseCapability - Pointer to store the OfflineMemoryDumpUseCapability value.

Return Value:

    NT status code.

--*/
{
    NTSTATUS        status;
    GUID            guid = EFI_OFFLINE_CRASHDUMP_VARIABLES_GUID;
    UNICODE_STRING  offlineMemoryDumpUseCapabilityVarName;


#ifndef WINDOWS_BLUE_307356_IS_FIXED
    UINT64 useCapability = 0;
#else
    UINT8 useCapability = 0;
#endif

    ULONG offlineMemoryDumpUseCapabilityLength = sizeof(useCapability);

    RtlInitUnicodeString(&offlineMemoryDumpUseCapabilityVarName,
        OFFLINE_MEMORY_DUMP_USE_CAPABILITY_VAR_NAME);

    status = NtQuerySystemEnvironmentValueEx(&offlineMemoryDumpUseCapabilityVarName,
        &guid,
        &useCapability,
        &offlineMemoryDumpUseCapabilityLength,
        NULL);

    if (!NT_SUCCESS(status))
    {
        TraceNTSTATUS("NtQuerySystemEnvironmentValueEx failed.", status);
        goto Exit;
    }
    
    *OfflineMemoryDumpUseCapability = (UINT32)useCapability;

Exit:
    return HRESULT_FROM_NT(status);
}

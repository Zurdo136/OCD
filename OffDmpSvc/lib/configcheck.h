/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
    configcheck.h

Environment:
    User Mode
    
--*/


#pragma once
#include "offdmpsvc.h"

#define DMP_OFFLINE_DUMP_STATE      L"OfflineDumpState"
#define DISABLE_KERNEL_MODE_OCD     L"WpDisableKernelModeOCD"

NTSTATUS
IsOffDumpReady(
    _Inout_     PDMP_CONTEXT Context
);

HRESULT
EnablePrivilege(
    DWORD         dwPrivilege
);

HRESULT
DetermineOfflineDumpVersion(
_Inout_  POFFLINE_CRASH_DUMP_CONFIGURATION_TABLE ConfigTable
);


HRESULT
CheckOfflineCrashdumpEnabled(
  _Inout_     PDMP_CONTEXT Context,
  _Out_       bool* IsOfflineDumpEnabled
);

HRESULT
IsSBLDumpExpected(
    _Inout_     PDMP_CONTEXT Context,
    _Out_       bool* IsDumpExpected
);

HRESULT
GetOfflineMemoryDumpUseCapability(
    _Out_         PUINT32 OfflineMemoryDumpUseCapability
);

// // //
PSTR 
GetParent( _In_z_ PCHAR dirName);

BOOL
MkDir(_In_z_ PCHAR dirName);

BOOL
IsDir(PCHAR _In_z_ dirName );

HRESULT
CreateFullPath(_In_z_ PCHAR dirName);

NTSTATUS
SetWpDmpFlag(_In_ UEFI_SETTING setVal);

/*++

Copyright (c) January 2016 Microsoft Corporation, All Rights Reserved

Module Name: 
    offdmpsvcTestWrappers.h

Environment:
    User Mode

--*/


#pragma once

#include "offdmpsvc.h"


#ifndef DLLEXPORT
#define DLLEXPORT __declspec(dllexport)
#endif

#ifndef DLLIMPORT
#define DLLIMPORT __declspec(dllimport)
#endif

// // // // // // // // // // // // // // // // // // // // // //
// These functions are from the DLL, called directly
extern 
HRESULT  
CheckAndSubmitOfflineCrash(VOID);

extern
HRESULT
IsOffDumpReady( 
    _Inout_     PDMP_CONTEXT Context
);

extern
HRESULT
SubmitOfflineCrashDump(
    _Inout_ PDMP_CONTEXT Context
);

// // // // // // // // // // // // // // // // // // // // // //
// These functions are from the DLL, not called directly
extern
HRESULT
DetermineOfflineDumpVersion(
_Inout_  POFFLINE_CRASH_DUMP_CONFIGURATION_TABLE ConfigTable
//_Inout_      PDMP_CONTEXT    Context
);

extern
HRESULT 
GetOfflineMemoryDumpUseCapability(
    _Out_         PUINT32 OfflineMemoryDumpUseCapability
);

extern
HRESULT
IsSBLDumpExpected(
    _Inout_     PDMP_CONTEXT Context,
    _Out_       bool* IsDumpExpected
);

extern
HRESULT
LocateAndCopyRawDump(
    _Inout_ PDMP_CONTEXT Context
);

extern
HRESULT
VerifyRawDumpHeader(
  _Inout_     PDMP_CONTEXT Context,
  _Inout_     bool* IsRawDumpHeaderValid
);


extern
HRESULT
CollateSDRawDumps(
_Inout_ PDMP_CONTEXT
);

HRESULT
VerifyRawDumpSectionTable(
    _Inout_ PDMP_CONTEXT Context,
    _Inout_ bool* IsRawDumpSectionTableValid
);

HRESULT
BuildDDRMemoryMap(
    _Inout_  PDMP_CONTEXT Context
);

HRESULT
GetDumpInstance(
    _Out_ PULARGE_INTEGER DumpInstance
);

HRESULT
FindDumpPartition(
    _Inout_ PDMP_CONTEXT Context
);

extern
HRESULT
CheckOfflineCrashdumpEnabled(
  _Inout_     PDMP_CONTEXT Context,
  _Out_       bool* IsOfflineDumpEnabled
);

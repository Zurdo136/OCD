/*++

Copyright (c) January 2016 Microsoft Corporation, All Rights Reserved

Module Name:
    offdmpsvcTestWrappers.cpp 


Environment:
    User Mode

Author:


--*/

#include "offdmpsvcTestWrappers.h"

// // // // // Test only context, is global but only exists here...
DMP_CONTEXT Context = {0};
    
    
int
DetermineOfflineDumpVersionWrapper(int val)
{
    HRESULT result = E_FAIL;
    OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE ConfigTable = {0};

    result = DetermineOfflineDumpVersion(&ConfigTable);
    if( !SUCCEEDED(result) )
    {
        return -1;
    }

    switch( val )
    {
        case 1:
            return ConfigTable.Version;
        case 2:
            return ConfigTable.AbnormalResetOccurred;
        case 3:
            return ConfigTable.OfflineMemoryDumpCapable;
        default:
            return -2;
    }
}


int
GetOfflineMemoryDumpUseCapabilityWrapper()
{
    HRESULT result = E_FAIL;
    UINT    Capability = 0;

    result = GetOfflineMemoryDumpUseCapability(&Capability);
    if( !SUCCEEDED(result) )
    {
        return -1;
    }
    return Capability;
}


int
IsSBLDumpExpectedWrapper( int i )
{
    HRESULT result = E_FAIL;
    bool bGood = false;

    result = IsSBLDumpExpected(&Context, &bGood);
    if( !SUCCEEDED(result) )
    {
        return -1;
    }
    
    switch( i )
    {
        case 1:
            return bGood;
        case 2:
            return Context.IsDumpExpectedChkList.IsAbnormalResetNonZero;
        case 3:
            return Context.IsDumpExpectedChkList.IsUseCapabilityPresent;
        case 4:
            return Context.IsDumpExpectedChkList.IsUseCapabilitySet;
        default:
            return -2;
    }

}

int
IsOffDumpReadyWrapper(int val)
{
    NTSTATUS result = E_FAIL;

    result = IsOffDumpReady(&Context);
    if( result == E_FAIL )
    {
        return -1;
    }
    
    switch(val)
    {
        case 1:  //Context->SBLDumpProgress.IsDumpEnabled
            return Context.SBLDumpProgress.IsDumpEnabled;
        case 2: //Context->SBLDumpProgress.IsDumpExpected
            return Context.SBLDumpProgress.IsDumpExpected;
        default:
            return -2;
    }
}

int
CheckOfflineCrashdumpEnabledWrapper()
{
    HRESULT result = E_FAIL;

    if( !SUCCEEDED(result) )
    {
            return -1;
    }
    
    if(  Context.SBLDumpProgress.IsDumpExpected )
        return 1;
    else
        return 0;
}


int
CheckAndSubmitOfflineCrashWrapper()
{
    HRESULT result = E_FAIL;

    result = CheckAndSubmitOfflineCrash();

    return result;
}

int
LocateAndCopyRawDumpWrapper()
{
    HRESULT status;

    status = LocateAndCopyRawDump(&Context);
    if( !SUCCEEDED(status) )
        return -1;

    if( DEVICE_IO::IO_OK != Context.hDisk.GetError())
        return 1;
    else if( Context.diskoffset.QuadPart == 0 )
        return 2;
    else if( Context.RawDumpPartitionLength.QuadPart == 0 )
        return 3;
    else
        return 0;
}

int
GetDumpLocation()
{
  return Context.SBLDumpLocation;
}

int
GetDumpFormat()
{
  return Context.SBLDumpFormat;
}

int 
VerifyRawDumpHeaderWrapper()
{
    bool IsRawDumpHeaderValid = FALSE;

    HRESULT status = VerifyRawDumpHeader( &Context, &IsRawDumpHeaderValid );
    if( !SUCCEEDED(status) )
        return -1;

    if( IsRawDumpHeaderValid==TRUE )
    {
        return 1;
    }
    else
    {
        return 2;
    }
}

int
CollateSDRawDumpsWrapper()
{
    if (Context.SBLDumpLocation != SBL_DUMP_LOCATION_SD)
        return 1;  // No Need!
    
    HRESULT result = CollateSDRawDumps(&Context);
    if (!SUCCEEDED(result))
    {
        return -1;
    }
    
    return 0;
}

int
VerifyRawDumpSectionTableWrapper()
{
    bool ValidateResult = FALSE;

    HRESULT result = VerifyRawDumpSectionTable(&Context, &ValidateResult);
    if(!SUCCEEDED(result))
    {
        return -1;
    }
    
    if( ValidateResult == TRUE )
    {
        return 1;
    }
    else
    {
        return 2;
    }
}

int
BuildDDRMemoryMapWrapper()
{
    HRESULT result = BuildDDRMemoryMap(&Context);
    if (!SUCCEEDED(result)) {
         return -1;
    }
    
    return 0;
}


int
GetDumpInstanceWrapper()
{
    HRESULT result = GetDumpInstance(&(Context.DumpInstance));
    if (!SUCCEEDED(result))
    {
        return -1;
    }   

    return 0;   // Context.DumpInstance;    
}

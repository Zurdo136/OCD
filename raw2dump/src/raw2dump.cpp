//
// raw2dump.cpp : Defines the exported functions for the DLL.
//
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>

#include "raw2dump.h"
#include "dumputil.h"
#include "dbgclient.h"

//
// ------------------------- Function Definitions -------------------------------------------------------------
//
void CleanupDmpContext(PDMP_CONTEXT Context)
{
    if (Context->ApReg) {
        HeapFree(GetProcessHeap(), NULL, Context->ApReg);
        Context->ApReg = nullptr;
    }

    if (Context->DDRMemoryMap) {
        HeapFree(GetProcessHeap(), NULL, Context->DDRMemoryMap);
        Context->DDRMemoryMap = nullptr;
    }

    if (Context->DumpHeader32) {
        HeapFree(GetProcessHeap(), NULL, Context->DumpHeader32);
        Context->DumpHeader32 = nullptr;
    }

    if (Context->DumpHeader64) {
        HeapFree(GetProcessHeap(), NULL, Context->DumpHeader64);
        Context->DumpHeader64 = nullptr;
    }

    if (Context->IoBuffer) {
        HeapFree(GetProcessHeap(), NULL, Context->IoBuffer);
        Context->IoBuffer = nullptr;
    }

    if (Context->RawDumpSectionTable) {
        HeapFree(GetProcessHeap(), NULL, Context->RawDumpSectionTable);
        Context->RawDumpSectionTable = nullptr;
    }
    
     if (Context->KdDebuggerDataBlock) {
        HeapFree(GetProcessHeap(), NULL, Context->KdDebuggerDataBlock);
        Context->KdDebuggerDataBlock = nullptr;
    }

    if (Context->WindowsDumpHandle != INVALID_HANDLE_VALUE)
    {
       CloseHandle(Context->WindowsDumpHandle);
       Context->WindowsDumpHandle = INVALID_HANDLE_VALUE;
    }

    if (nullptr != Context->RawDumpSectionTable)
    {
       HeapFree(GetProcessHeap(), NULL, Context->RawDumpSectionTable);
       Context->RawDumpSectionTable = nullptr;
    }

}


// This function takes the path to rawdump and rawdump info file and outputs a windows dump file.
bool
ConvertRawToDump(
    _In_ LPWSTR rawDumpPath,
    _In_ LPWSTR rawInfoFile,
    _In_ LPWSTR logFile,
    _In_ LPWSTR windowsDumpFile
    )
{    
    HRESULT hr = S_OK;
    DMP_CONTEXT context = { 0 };    // declare and init the context

    // Check if the inputs are correct.
    if (rawDumpPath &&
        windowsDumpFile) {
        context.WindowsDumpFilePath = windowsDumpFile;
    } else {
        hr = E_INVALIDARG;
        TraceHRESULT("Invalid Path", hr);
        goto Error;
    }

    if (!rawInfoFile || !logFile) {
        TraceInfo("rawdumpInfo file is not provided. Expecting device specific info to be present \
                                    in the rawdump. NOTE: Log file will not be updated for this conversion.");
        //
        // Creating log file.
        //
        logFile = L"raw2dump.log";
        context.IsDeviceInfoInRawDump = TRUE;
    }
    else {
        context.IsDeviceInfoInRawDump = FALSE;
    }

    hr = OpenLogFile(logFile);
    if (!SUCCEEDED(hr)) {
        TraceHRESULT("OpenLogFile Failed", hr);
    }

    context.rawdumpInfoFilePath = rawInfoFile;

    hr = ExtractRawDumpFile(&context, rawDumpPath);
    if (FAILED(hr)) {
        TraceHRESULT("ExtractRawDumpFile failed", hr);
    }

    CloseLogFile();

Error:
    CleanupDmpContext(&context);
    return !SUCCEEDED(hr);

}


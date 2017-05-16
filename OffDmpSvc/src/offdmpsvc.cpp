/*++

Copyright (c) April 2014 Microsoft Corporation, All Rights Reserved

Module Name:
    offdmpsvc.cpp 


Environment:
    User Mode

Author:


--*/

#include "offdmpsvc.h"
#include "configcheck.h"
#include "buildparams.h"


//"6D463093-0696-4F48-A39C-F65DF5B49F71"
static const GUID s_OffdmpsvcTraceLoggingProvider =
{ 0x6D463093, 0x0696, 0x4F48, { 0xA3, 0x9C, 0xF6, 0x5D, 0xF5, 0xB4, 0x9F, 0x71 } };

TRACELOGGING_DEFINE_PROVIDER(g_hOffdmpsvcTraceLoggingProvider,
    "OfflineCrashDumpTraceLoggingProvider",
    (0x6D463093, 0x0696, 0x4F48, 0xA3, 0x9C, 0xF6, 0x5D, 0xF5, 0xB4, 0x9F, 0x71),
    TraceLoggingOptionMicrosoftTelemetry());


/*++

Routine Description:    
    This is the DLL entry. The function checks if an offline crash dump is available or 
    not and return the success code accordingly.

Arguments:
    Context

Return Value
    NT status code.

--*/

HRESULT
CheckAndSubmitOfflineCrash(VOID)
{
    HRESULT result;

    DMP_CONTEXT Context = {0};

    TraceLoggingRegister(g_hOffdmpsvcTraceLoggingProvider);
    TraceLoggingThreadActivity<g_hOffdmpsvcTraceLoggingProvider, MICROSOFT_KEYWORD_TELEMETRY> CheckAndSubmitOfflineCrashActivity;
    TraceLoggingWriteStart(CheckAndSubmitOfflineCrashActivity,
            "CheckAndSubmitOfflineCrash",
            TraceLoggingValue("CheckAndSubmitOfflineCrash", "Start")
            );

    TraceMetric("BEGIN:CheckAndSubmitOfflineCrash");

    result = OpenLogFile(LOG_FILE_PATH);
    if (!SUCCEEDED(result)) {
        TraceHRESULT("OpenLogFile Failed.", result);
		TraceString("Log file path", LOG_FILE_PATH);
    }

    Context.LogFilePath = LOG_FILE_PATH;
    result = IsOffDumpReady(&Context);
    if (SUCCEEDED(result)) {
        TraceMetric("Raw dump is Expected");
        result = SubmitOfflineCrashDump(&Context);
    }
    else
    {
        TraceMetric("Raw dump not Expected");
    }

    TraceMetric("DONE:CheckAndSubmitOfflineCrash");

    TraceLoggingWriteStop(CheckAndSubmitOfflineCrashActivity,"CheckAndSubmitOfflineCrash");
    TraceLoggingUnregister(g_hOffdmpsvcTraceLoggingProvider);
        
    return result;
}

/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
    buildparams.h

Environment:
User Mode

--*/


#pragma once
#include "offdmpsvc.h"
#include "svspecific.h"

#include <TraceLoggingProvider.h>
#include <telemetry\MicrosoftTelemetry.h>
#include <TraceLoggingActivity.h>

TRACELOGGING_DECLARE_PROVIDER(g_hOffdmpsvcTraceLoggingProvider);


//
// TraceError: Log an error
// DescriptionText  - high level explanation of the error
// ErrorClass       - one of "HRESULT", "NTSTATUS" or "WIN32"
// ErrorCode        - actual error code
//

#define TraceError(DescriptionText, ErrorClass, ErrorCode)         \
    TraceLoggingWrite(                                             \
    g_hOffdmpsvcTraceLoggingProvider,                              \
    "OffdmpsvcError",                                              \
    TraceLoggingValue(DescriptionText, "Description"),             \
    TraceLoggingValue(ErrorCode, ErrorClass),                      \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));             \
    DmpLog("%s %s 0x%x\n", DescriptionText, ErrorClass, ErrorCode)

//
// TraceError1-N: Log an error with 1-N info pairs
// DescriptionText          - high level explanation of the error
// ErrorClass               - one of "HRESULT", "NTSTATUS" or "WIN32"
// ErrorCode                - actual error code
// ValueName, Value pair    - additional info related to the error
//

#define TraceError1(DescriptionText, ValueName1, Value1, ErrorClass, ErrorCode)   \
    TraceLoggingWrite(                                             \
    g_hOffdmpsvcTraceLoggingProvider,                              \
    "OffdmpsvcError",                                              \
    TraceLoggingValue(DescriptionText, "Description"),             \
    TraceLoggingValue(Value1, ValueName1),                         \
    TraceLoggingValue(ErrorCode, ErrorClass),                      \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));             \
    DmpLog("%s %s %llx %s 0x%x\n", DescriptionText, ValueName1, (UINT64)Value1, ErrorClass, ErrorCode)

#define TraceError2(DescriptionText, ValueName1, Value1, ValueName2, Value2, ErrorClass, ErrorCode)    \
    TraceLoggingWrite(                                             \
    g_hOffdmpsvcTraceLoggingProvider,                              \
    "OffdmpsvcError",                                              \
    TraceLoggingValue(DescriptionText, "Description"),             \
    TraceLoggingValue(Value1, ValueName1),                         \
    TraceLoggingValue(Value2, ValueName2),                         \
    TraceLoggingValue(ErrorCode, ErrorClass),                      \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));             \
    DmpLog("%s %s %llx %s %llx %s 0x%x\n", DescriptionText, ValueName1, (UINT64)Value1, ValueName2, (UINT64)Value2, ErrorClass, ErrorCode)

//
// Some helper macros to wrap the error macros for specific purposes
//

#define TraceHRESULT(DescriptionText, ErrorCode)  TraceError(DescriptionText, "HRESULT", ErrorCode)
#define TraceHRESULT1(DescriptionText, ValueName1, Value1, ErrorCode) TraceError1(DescriptionText, ValueName1, Value1, "HRESULT", ErrorCode)
#define TraceHRESULT2(DescriptionText, ValueName1, Value1, ValueName2, Value2, ErrorCode) TraceError2(DescriptionText, ValueName1, Value1, ValueName2, Value2, "HRESULT", ErrorCode)
#define TraceNTSTATUS(DescriptionText, ErrorCode) TraceError(DescriptionText, "NTSTATUS", ErrorCode)
#define TraceWIN32(DescriptionText, ErrorCode) TraceError(DescriptionText, "WIN32", ErrorCode)

//
// TraceWarn: Log a warning.
// DescriptionText  - high level explanation of the warning
//

#define TraceWarn(DescriptionText)                                \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcWarning",                                           \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s\r\n", DescriptionText)

//
// TraceWarn1-N: Log a warning with 1-N info pairs
// DescriptionText          - high level explanation of the warning
// ValueName, Value pair    - additional information related to the warning
//

#define TraceWarn1(DescriptionText, ValueName1, Value1)           \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcWarning",                                           \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingValue(Value1, ValueName1),                        \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s %s 0x%llx\r\n", DescriptionText, ValueName1, (UINT64)Value1)

#define TraceWarn2(DescriptionText, ValueName1, Value1, ValueName2, Value2)  \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcWarning",                                           \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingValue(Value1, ValueName1),                        \
    TraceLoggingValue(Value2, ValueName2),                        \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s %s 0x%llx %s 0x%llx\r\n", DescriptionText, ValueName1, (UINT64)Value1, ValueName2, (UINT64)Value2)

#define TraceWarn3(DescriptionText, ValueName1, Value1, ValueName2, Value2, ValueName3, Value3)  \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcWarning",                                           \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingValue(Value1, ValueName1),                        \
    TraceLoggingValue(Value2, ValueName2),                        \
    TraceLoggingValue(Value3, ValueName3),                        \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s %s 0x%llx %s 0x%llx %s 0x%llx\r\n", DescriptionText, ValueName1, (UINT64)Value1, ValueName2, (UINT64)Value2, ValueName3, (UINT64)Value3)


//
// TraceInfoString: logs a single string with a preamble
//
#define TraceString(StringInfo, StringPayload)                    \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcInfo",                                              \
    TraceLoggingValue(StringInfo),                                \
    TraceLoggingValue(StringPayload),                             \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s %s\r\n", StringInfo, StringPayload)

//
// TraceCritical: Log a crital telemtry message
//

#define TraceMetric(MetricText)                                   \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcInfo",                                              \
    TraceLoggingValue(MetricText, "Metric"),                      \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY | MICROSOFT_KEYWORD_CRITICAL_DATA)); \
    DmpLog("%s \r\n", MetricText)

//
// TraceInfo: Log a message
// DescriptionText  - log message
//

#define TraceInfo(DescriptionText)                                \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcInfo",                                              \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s \r\n", DescriptionText)

//
// TraceInfo1-N: Log a message with 1-N info pairs
// DescriptionText          - log message
// ValueName, Value pair    - additional information related to the message
//

#define TraceInfo1(DescriptionText, ValueName1, Value1)           \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcInfo",                                              \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingValue(Value1, ValueName1),                        \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s %s 0x%llx\r\n", DescriptionText, ValueName1, (UINT64)Value1)

#define TraceInfo2(DescriptionText, ValueName1, Value1, ValueName2, Value2)  \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcInfo",                                              \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingValue(Value1, ValueName1),                        \
    TraceLoggingValue(Value2, ValueName2),                        \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s %s 0x%llx %s 0x%llx\r\n", DescriptionText, ValueName1, (UINT64)Value1, ValueName2, (UINT64)Value2)

#define TraceInfo3(DescriptionText, ValueName1, Value1, ValueName2, Value2, ValueName3, Value3)  \
    TraceLoggingWrite(                                            \
    g_hOffdmpsvcTraceLoggingProvider,                             \
    "OffdmpsvcInfo",                                              \
    TraceLoggingValue(DescriptionText, "Description"),            \
    TraceLoggingValue(Value1, ValueName1),                        \
    TraceLoggingValue(Value2, ValueName2),                        \
    TraceLoggingValue(Value3, ValueName3),                        \
    TraceLoggingKeyword(MICROSOFT_KEYWORD_TELEMETRY));            \
    DmpLog("%s %s 0x%llx %s 0x%llx %s 0x%llx\r\n", DescriptionText, ValueName1, (UINT64)Value1, ValueName2, (UINT64)Value2, ValueName3, (UINT64)Value3)

//
// TraceExpectedActual: Helper to log expected vs actual value when a mismatch is encountered
//
#define TraceExpectedActual(DescriptionText, ExpectedValue, ActualValue) \
    TraceInfo2(DescriptionText, "Expected", ExpectedValue, "Actual", ActualValue)

//
//
//  Miscelaneous, Defined constants here
//
    //  This is the current number of partitions in a raw dump file.
#define PARTITION_INFORMATION_SECTION_COUNT     16

//
// Function Prototypes
//
HRESULT
SubmitOfflineCrashDump(
    _Inout_ PDMP_CONTEXT Context
);

HRESULT
LocateAndCopyRawDump(
    _Inout_ PDMP_CONTEXT Context
);

HRESULT
FindDumpPartition(
    _Inout_ PDMP_CONTEXT Context
);


HRESULT
FindRawDumpDotBin(
    _Inout_ PDMP_CONTEXT Context
);

HRESULT
FindNewestRawDumpDotBinPath(
    _Inout_ PDMP_CONTEXT Context
);

HRESULT
CreateRawDumpDotBin(
    _Inout_ PDMP_CONTEXT Context
);

BOOL
GetSectionNameFromGUIDTable(
_In_ GUID *pguid,
_Inout_ PUCHAR fileName
);

HRESULT
CollateSDRawDumps(
_Inout_ PDMP_CONTEXT
);

HRESULT
AppendFile(
    _In_ DEVICE_IO      *destinationFile,
    _In_ DEVICE_IO      *sourceFile,
    _Inout_ PULONGLONG  bytesAppended
);

HRESULT
CheckIfDumpHasError(
_In_ PWCHAR rawdumpFolderPath,
_Inout_ PBOOL hasError
);

HRESULT
CheckIfDumpAlreadyProcessed(
_In_ PWCHAR rawdumpFolderPath,
_Inout_ PBOOL alreadyProcessed
);

HRESULT
MarkDumpProcessed(
_In_ PWCHAR rawdumpFolderPath
);

HRESULT
VerifyRawDumpHeader(
    _Inout_ PDMP_CONTEXT Context,
    _Inout_ bool* IsRawDumpHeaderValid
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
GetAPReg(
    _Inout_ PDMP_CONTEXT Context,
    _Out_   bool* IsApRegValid
);

HRESULT 
ReadRawDumpPartitionToFile(
    _Inout_ PDMP_CONTEXT Context,
    _In_    LPCWSTR FilePath
);

HRESULT
AppendDeviceSpecificInfoToRawDump(
    _Inout_ PDMP_CONTEXT Context,
    _In_ SvSpecific* SVData
);

HRESULT 
SubmitReportToWER(
    _Inout_ PDMP_CONTEXT Context
);

VOID 
CleanupContext(
    _In_ PDMP_CONTEXT Context
);


//
// Helper functions
// 
HRESULT
ReadFromDDRSectionByPhysicalAddress(
   _In_  PDMP_CONTEXT Context,
   _In_  LARGE_INTEGER PhysicalAddress,
   _In_  UINT64 Length,
   _Out_ PVOID Buffer
);

BOOLEAN
ValidateRawDumpHeaderFlags(
    _In_ UINT32 Flags
);

VOID
DumpRawDumpHeader(
_In_ const PRAW_DUMP_HEADER Header
);

//
// TODO: Dont hard code, read the registry and populate this parameter.
//
#define WINDOWSDUMP_FILE_PATH        L"C:\\data\\test\\bin\\DMP0000.dmp"
 
//
// CrashControl registry key path and values
//
#define CRASHCONTROL_PATH               L"SYSTEM\\CurrentControlSet\\Control\\CrashControl"
#define CRASHCONTROL_RAW2DUMP_ENABLED   L"Raw2DumpEnabled"

//
// For multi-sbl-dump scenarios, wpdmp.efi will write a 
// "done" file.
//
#define MULTI_DUMP_DONE_FILE    L"wpdone.txt"

// Temporary files and locations
#define RAWDUMP_BIN_FILE            L"rawdump.bin"
#define RAWDUMP_INFO_FILE           L"rawdumpinfo.xml"
#define DEFAULT_CRASH_DUMP_PATH     L"C:\\Data\\CrashDump\\"
#define LEGACY_CRASH_DUMP_PATH      L"C:\\CrashDump\\"

// Check for this partition by name, GUID is not unique
#define CRASH_DUMP_PARTITION_NAME       L"CrashDump"

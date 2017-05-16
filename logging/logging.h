/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
    logging.h

Environment:
    User Mode
    
--*/
#pragma once

//
// Defining wpdmp log file header
//
typedef struct _LOG_FILE_HEADER {
    ULONGLONG LogFileOffset;
    ULONGLONG LogNumber;
} LOG_FILE_HEADER, *PLOG_FILE_HEADER;

//
// Allowable size of the log: 100KB
// 
#define LOG_FILE_SIZE 102400

VOID
DumpGUID(
    _In_ const GUID* Guid
);

HRESULT
OpenLogFile(
_In_ LPWSTR LogFilePath
);

HRESULT
CloseLogFile(
);

VOID
DmpLog(
_In_ PCSTR Format,
...
);
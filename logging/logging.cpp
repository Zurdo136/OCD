/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
    logging.cpp

Environment:
    User Mode
    
--*/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>
#include <initguid.h>
#include <rawdump.h>
#include "logging.h"
#include <ntstrsafe.h>

//
// Global Log file handle.
//
HANDLE g_LogFileHandle = NULL;

//
// Global File header
//
LOG_FILE_HEADER g_logFileHeader;

//
// Global Scratch buffer
//
#define SCRATCH_BUFFER_SIZE  4096
CHAR g_ScratchBuffer[SCRATCH_BUFFER_SIZE];

//
// Bytes written in the file starting from the Header location.
//
ULONGLONG g_bytesFromBeginning = 0;



VOID
DumpGUID(
_In_ const GUID* Guid
)
{
    DmpLog("\t\t{%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}\r\n",
        Guid->Data1, Guid->Data2, Guid->Data3,
        Guid->Data4[0], Guid->Data4[1], Guid->Data4[2],
        Guid->Data4[3], Guid->Data4[4], Guid->Data4[5],
        Guid->Data4[6], Guid->Data4[7]);
}

HRESULT
OpenLogFile(
    _In_ LPWSTR LogFilePath
)
/*++

Routine Description:
This routine open the Log file and sets the file pointer to the offset pointed by the 
file header.

Arguments:
LogFilePath: path to the log file 


Return Value:
HRESULT

--*/
{
    HRESULT hr = S_OK;
    BOOL result = FALSE;
    DWORD dwBytesRead;
    LARGE_INTEGER offset;
    SYSTEMTIME st;
    wprintf(L"Opening log file %s \n\r", LogFilePath);

    g_LogFileHandle = CreateFileW(LogFilePath,
                        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        nullptr,
                        OPEN_ALWAYS,
                        0,
                        nullptr);


    if (g_LogFileHandle == INVALID_HANDLE_VALUE){
        hr = HRESULT_FROM_WIN32(GetLastError());
        wprintf(L"OpenLogFile:Create File Failed 0x%x\n\r", hr);
        goto Exit;
    }

    //
    // Log File header is located in the beginning of the file which contains the offset 
    // where the log needs to eb continued to be written.
    //
    offset.QuadPart = 0;
    result = SetFilePointerEx(g_LogFileHandle, offset,
                NULL, FILE_BEGIN);
    if (!result) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Exit;
    }
    result = ReadFile(g_LogFileHandle,
                &g_logFileHeader, sizeof(LOG_FILE_HEADER),
                &dwBytesRead, NULL);
    if (g_logFileHeader.LogFileOffset < sizeof(LOG_FILE_HEADER) || !result || !dwBytesRead) {
        //
        // This is the first time the file is open. Adjusting the file offset to not
        // overwrite the Log File Header.
        //
        g_logFileHeader.LogFileOffset = sizeof(LOG_FILE_HEADER);
        g_logFileHeader.LogNumber = 1;
    }

    //
    // Setting the file pointer to the location pointed by the File header.
    //
    offset.QuadPart = g_logFileHeader.LogFileOffset;
    g_bytesFromBeginning = g_logFileHeader.LogFileOffset;

    result = SetFilePointerEx(g_LogFileHandle, offset,
                NULL, FILE_BEGIN);
    GetSystemTime(&st);

    DmpLog("\r\n\r\n********** Offline Crash log opened[%I64u]: %u/%u/%u %u:%u:%u:%u **********\r\n\r\n",
        g_logFileHeader.LogNumber,
        st.wMonth,
        st.wDayOfWeek,
        st.wYear,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds);


Exit:
    return hr;

}

HRESULT
OpenLogFileWithoutMetadataUpdate(
    _In_ LPWSTR LogFilePath
)
/*++

Routine Description:
This routine simply opens the log file without updating the metadata information.

Arguments:
None

Return Value:
HRESULT

--*/
{
    HRESULT hr = S_OK;

    g_LogFileHandle = CreateFileW(LogFilePath,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_ALWAYS,
        0,
        nullptr);
    if (g_LogFileHandle == INVALID_HANDLE_VALUE){
        hr = HRESULT_FROM_WIN32(GetLastError());
        wprintf(L"OpenLogFileWithoutMetadataUpdate:Create File Failed 0x%x\n\r", hr);
    }
    
    return hr;
}


HRESULT
CloseLogFile(
)
/*++

Routine Description:
This function writes that the log file is about to be closed, saves log
file offset, iteration number and closes wpdmp log file.

Arguments:
None

Return Value:
HRESULT

--*/
{
    SYSTEMTIME st;    
    LONG high_word = 0;
    DWORD low_word = 0;
    DWORD dwBytesWritten;
    LONG offset = 0;
    HRESULT hr = S_OK;
    BOOL result;

    if (g_LogFileHandle == NULL || g_LogFileHandle == INVALID_HANDLE_VALUE){
        goto Exit;
    }

    GetSystemTime(&st);
    DmpLog("\r\n\r\n********** Offline Crash log closed[%I64u]: %u/%u/%u %u:%u:%u:%u **********\r\n\r\n",
        g_logFileHeader.LogNumber,
        st.wMonth,
        st.wDayOfWeek,
        st.wYear,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds);

    low_word = SetFilePointer(g_LogFileHandle, offset, &high_word, FILE_CURRENT);
    if (low_word == INVALID_SET_FILE_POINTER) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Cleanup;
    }

    g_logFileHeader.LogFileOffset = high_word;
    g_logFileHeader.LogFileOffset = g_logFileHeader.LogFileOffset << 32 | low_word;
    g_logFileHeader.LogNumber++;

    //
    // Writing the log file number.
    //
    low_word = SetFilePointer(g_LogFileHandle, offset, NULL, FILE_BEGIN);
    if (low_word == INVALID_SET_FILE_POINTER) {
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto Cleanup;
    }
    result = WriteFile(g_LogFileHandle, &g_logFileHeader,
        sizeof(LOG_FILE_HEADER) , &dwBytesWritten,
        NULL);

    if (!result || !dwBytesWritten) {
        hr = HRESULT_FROM_NT(GetLastError());
        wprintf(L"Failed to update Log File Header while closing the File 0x%x\n", hr);
    }

    //
    // Sometimes the Last write is not reflected on the header.
    //
    FlushFileBuffers(g_LogFileHandle);
    wprintf(L"g_logFileHeader.LogFileOffset %I64u \n", g_logFileHeader.LogFileOffset);
    wprintf(L"g_logFileHeader.LogNumber %I64u \n", g_logFileHeader.LogNumber);

Cleanup:
    if (g_LogFileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(g_LogFileHandle);
    }

Exit:
    return hr;
}


HRESULT
CloseLogFileWithoutMetadataUpdate(
)
/*++

Routine Description:
This routine simply closes the file without updating the metadata information.

Arguments:
None

Return Value:
HRESULT

--*/
{
    HRESULT hr = E_FAIL;
    if (g_LogFileHandle != INVALID_HANDLE_VALUE) {
        if(CloseHandle(g_LogFileHandle)) {
            hr = S_OK;
        }
        else {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return hr;
}


VOID
DmpLog(
_In_ PCSTR Format,
...
)

/*++
Routine Description:
This function writes to wpdmp log file.

Arguments:
Format - String format.

... - Arguments.

Return Value:
None.

--*/
{
    HRESULT hr;
    va_list Arglist;
    BOOL result;
    DWORD dwBytesWritten;
    DWORD bufferLength;

    if (g_LogFileHandle == NULL || g_LogFileHandle == INVALID_HANDLE_VALUE) {
        goto Exit;
    }
        
    va_start(Arglist, Format);
    hr = RtlStringCbVPrintfA(g_ScratchBuffer, SCRATCH_BUFFER_SIZE,
        Format, Arglist);        
    va_end(Arglist);
    if (!SUCCEEDED(hr)) {
        goto Exit;
    }

    bufferLength = (DWORD)strlen(g_ScratchBuffer);
    result = WriteFile(g_LogFileHandle, g_ScratchBuffer,
        bufferLength, &dwBytesWritten,
        NULL);

    g_bytesFromBeginning += bufferLength;
    if (!result || g_bytesFromBeginning > LOG_FILE_SIZE) {
        //
        // We have crossed the 
        LARGE_INTEGER offset;
        offset.QuadPart = sizeof(LOG_FILE_HEADER);
        result = SetFilePointerEx(g_LogFileHandle, offset,
            NULL, FILE_BEGIN);
        if (!result) {
            //
            // If there was an error writing to a file, most likely cause
            // is that the log file is full. Resetting cursor to the
            // beginning
            //
            WriteFile(g_LogFileHandle, g_ScratchBuffer,
                bufferLength, &dwBytesWritten,
                NULL);
        } // if (SUCCEEDED(result) && !result)
    } // if (!result || g_bytesFromBeginning > LOG_FILE_SIZE)

Exit:
    return;
}


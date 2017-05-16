#pragma once

namespace Raw2Dump
{
    bool ConvertRawToDump(
        _In_ LPWSTR rawDumpPath, 
        _In_ LPWSTR rawInfoFile, 
        _In_ LPWSTR logFile,
        _In_ LPWSTR windowsDumpFile);
}


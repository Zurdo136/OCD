// dllmain.cpp : Defines the entry point for the DLL application.
#include <dumputil.h>

//Our Provider GUID
static const GUID guidRaw2DumpTraceLoggingProvider =
{ 0x7222f677, 0x3ee1, 0x46b7, { 0x16, 0xb9, 0x13, 0xf5, 0x70, 0x99, 0x30, 0xcc }};

// Allocate TraceLogging Provider
TRACELOGGING_DEFINE_PROVIDER(g_hRaw2DumpTraceLoggingProvider,
    "Raw2DumpTraceLoggingProvider",
    (0x7222f677, 0x3ee1, 0x46b7, 0x16, 0xb9, 0x13, 0xf5, 0x70, 0x99, 0x30, 0xcc));

BOOL
APIENTRY
DllMain(
    HMODULE hModule,
    DWORD  Reason,
    LPVOID lpReserved
    )
{
    UNREFERENCED_PARAMETER(lpReserved);

    switch (Reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        TraceLoggingRegister(g_hRaw2DumpTraceLoggingProvider);
        break;

    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}


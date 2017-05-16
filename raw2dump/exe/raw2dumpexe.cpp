
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <Strsafe.h>
#include "rawdump.h"
#include "common.h"

#define RAWDUMP_INFO_FILE_NAME L"rawdumpinfo.xml"
#define RAWDUMP_LOGFILE_NAME L"offlinecrash.log"

typedef HRESULT(CALLBACK* ConvertRawToDump)(LPWSTR, LPWSTR, LPWSTR, LPWSTR);

int __cdecl wmain(int argc, WCHAR ** argv)
{
    int retVal = 0;
    ConvertRawToDump pfnConvertRawToDump = nullptr;
    
    wprintf(L"RAW2DUMP library\r\n");

    if (argc < 3) {
        wprintf(L"Usage: raw2dumpexe <raw file path> <dump file path>, (argc==%d)\r\n", argc);
        return 1;
    }

    HINSTANCE const hoffdump = LoadLibrary(L"raw2dump.dll");

    if (nullptr != hoffdump) {
        // Get the pointer to the function
        pfnConvertRawToDump = (ConvertRawToDump)GetProcAddress(hoffdump, "ConvertRawToDump");
        if (nullptr != pfnConvertRawToDump) {
            wprintf(L"       RAWDUMP FILE: %s\r\n", argv[1]);
            wprintf(L"  Windows Dump FILE: %s\r\n", argv[2]);
            HRESULT hr = pfnConvertRawToDump(argv[1], NULL , NULL, argv[2]);
            if (FAILED(hr)) {
                wprintf(L"ConvertRawToDump failed %x\r\n", hr);
                retVal = 2;
                goto Exit;
            }
        } else {
            wprintf(L"GetProcAddress(ConvertRawToDump) failed %d\r\n", GetLastError());
            retVal = 3;
            goto Exit;
        }
    } else {
        wprintf(L"LoadLibrary(raw2dump.dll) failed %d\r\n", GetLastError());
        retVal = 4;
        goto Exit;
    }

Exit:
    if (hoffdump != nullptr) {
        FreeLibrary(hoffdump);
    }
    return retVal;
}


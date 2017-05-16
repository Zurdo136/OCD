
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <stdlib.h>
#include <stdio.h>

typedef HRESULT(CALLBACK* ConvertRawToDump)(LPWSTR, LPWSTR, LPWSTR, LPWSTR);

int __cdecl wmain(int argc, WCHAR ** argv)
{
    int retVal = 0;

    ConvertRawToDump pfnConvertRawToDump = nullptr;
    wprintf(L"Offline Dump Tool Test started\n");

    if (argc < 5) {
        wprintf(L"Usage: offdumptest <raw file> <info file> <logfile> <dump file>, (argc==%d)\n", argc);
        return 1;
    }

    HINSTANCE const hoffdump = LoadLibrary(L"raw2dump.dll");

    if (nullptr != hoffdump) {
        // Get the pointer to the function
        pfnConvertRawToDump = (ConvertRawToDump)GetProcAddress(hoffdump, "ConvertRawToDump");
        if (nullptr != pfnConvertRawToDump) {
            HRESULT hr = pfnConvertRawToDump(argv[1], argv[2], argv[3], argv[4]);
            if (FAILED(hr)) {
                wprintf(L"ConvertRawToDump failed %x\n", hr);
                retVal = 2;
                goto Exit;
            }
        } else {
            wprintf(L"GetProcAddress(ConvertRawToDump) failed %d\n", GetLastError());
            retVal = 3;
            goto Exit;
        }
    } else {
        wprintf(L"LoadLibrary(raw2dump.dll) failed %d\n", GetLastError());
        retVal = 4;
        goto Exit;
    }

Exit:
    if (hoffdump != nullptr) {
        FreeLibrary(hoffdump);
    }
    return retVal;
}


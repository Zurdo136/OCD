/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name: 
    offdmpuploader.cpp

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
#include <werapi.h>
#include <winbase.h>

#define	UNREFERENCED(x)	x

typedef NTSTATUS(CALLBACK* CheckAndSubmitOfflineCrash)(VOID);

DWORD
__cdecl
wmain(
_In_ DWORD /*argc*/,
_In_ WCHAR const * /*const argv[]*/
)
{	
	CheckAndSubmitOfflineCrash CheckAndSubmitOfflineCrashPtr = nullptr;	

	HINSTANCE const hoffdump = LoadLibraryExW(L"offdmpsvc.dll", nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	wprintf(L"\n==== Offline Dump Uploader Tool  =====");

	if (hoffdump != nullptr) {
		CheckAndSubmitOfflineCrashPtr = (CheckAndSubmitOfflineCrash)GetProcAddress(hoffdump, "CheckAndSubmitOfflineCrash");
		if (CheckAndSubmitOfflineCrashPtr != nullptr) {
			(*CheckAndSubmitOfflineCrashPtr)();
		}
	}
	else {
		wprintf(L"\nCould not load the offdmpsvc.dll");
	}

   if (hoffdump != nullptr) {
     FreeLibrary(hoffdump);
  }
    
	return HRESULT_FROM_WIN32(GetLastError());
}
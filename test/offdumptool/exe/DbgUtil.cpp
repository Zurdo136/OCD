/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   dbgutil.cpp

Environment:
   User Mode

Author:
   radutta 
--*/
#include "common.h"
#include <atlbase.h>
#include <dbgeng.h>

#define IO_BUFFER_SIZE 0x800000

typedef enum {
    MEMORY_OS = 1,
    MEMORY_NONOS = 2
} MEMORY_TYPE;

#define NON_OS_DDR_GUID {0x3B9DEA8E, 0x75BE, 0x478F, {0x9F, 0xB4, 0x7D, 0x2C, 0x5F, 0x9D, 0xDE, 0x31}};
#define MEMORY_MAP_GUID {0x11DF58C5, 0x3FDC, 0x4BB9, {0x98, 0x4D, 0xD1, 0xD8, 0x3D, 0xDF, 0x4D, 0x37}};

//
// DDR Section Memory Map
// Based on DDR Seciton plus some 
// additional info to facilitate reading
// DDR sections
//
typedef struct _DDR_MEMORY_MAP
{
    UINT64      Base;

    //
    // End is Base + Size - 1
    //
    UINT64      End;
    UINT64      Size;

    UINT64      Offset;

    //
    // Contiguous tells us whether the current section is 
    // contiguous from previous section in terms of physical
    // addresses. What that means is that previous section's
    // End is just one byte less than this section's Base. 
    //
    BOOLEAN     Contiguous;

    MEMORY_TYPE Type;

    UINT32      DDRIndex;
}DDR_MEMORY_MAP, *PDDR_MEMORY_MAP;

typedef struct _SV_SPECIFIC_NAME_TO_GUID
{
    GUID    Guid;
    WCHAR   Name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
}SV_SPECIFIC_NAME_TO_GUID;

static SV_SPECIFIC_NAME_TO_GUID GUIDToName[] = {{0xAB3A051F, 0xEF0B, 0x4A5F, {0xA7, 0x9A, 0x80, 0xC2, 0x43, 0xBA, 0x08, 0x48}, L"AP_REG.BIN"},
                                                {0xD0A267A1, 0x9CA5, 0x471D, {0x8E, 0x9C, 0x79, 0xC9, 0x86, 0xBE, 0x77, 0x77}, L"OCIMEM.BIN"},
                                                {0x100B990B, 0x0F9B, 0x40B3, {0x82, 0xEF, 0x06, 0x61, 0x4F, 0x53, 0x05, 0xFE}, L"CODERAM.BIN"},
                                                {0x82233308, 0xCE47, 0x4D52, {0x92, 0x11, 0xF4, 0x2E, 0x89, 0x61, 0x8A, 0xF4}, L"DATARAM.BIN"},
                                                {0x91A8C35C, 0xA340, 0x4F2E, {0xB7, 0x27, 0x65, 0x39, 0x47, 0xDB, 0x9C, 0x76}, L"MSGRAM.BIN"}, 
                                                {0x877F61E0, 0xA870, 0x4635, {0x9F, 0x41, 0x33, 0x00, 0x53, 0x20, 0x26, 0x05}, L"LPM.BIN"},
                                                {0x10D25EDD, 0x1558, 0x4B88, {0xAB, 0x5C, 0xE8, 0x1E, 0x7F, 0x47, 0xDA, 0xD9}, L"PMIC_PON.BIN"}, 
                                                {0xD0352E48, 0xE359, 0x459E, {0x9B, 0xBF, 0x2E, 0x16, 0xE6, 0x28, 0xAC, 0xFB}, L"RST_STAT.BIN"}, 
                                                {0x066A56C8, 0xCE2A, 0x4686, {0xB6, 0x10, 0x5B, 0xFC, 0x22, 0xD0, 0xC7, 0xAB}, L"load.cmm"},
                                                {0x0df632e9, 0x5c48, 0x43aa, {0xb8, 0xbd, 0x5f, 0xf6, 0x18, 0x05, 0x02, 0x5f}, L"rawdump.bin"},
                                                {0x62fb2678, 0x933f, 0x4177, {0x86, 0x29, 0xff, 0x3f, 0x70, 0x55, 0x02, 0xe3}, L"DDR_DATA.BIN"},
                                                {0x6901D825, 0x0E25, 0x4D6C, {0x8C, 0x11, 0xE0, 0xAB, 0x2E, 0x98, 0xCA, 0xEF}, L"UNKNOWN"}};

HRESULT
ExtractSecondaryDataFromDumpFile(LPWSTR dumpfile)
{
    CComPtr<IDebugDataSpaces4>  pDbgDataSpaces;
    CComPtr<IDebugClient5>      pDbgClient;
    CComPtr<IDebugControl4>     pDbgControl;
    HRESULT                     hr = S_OK;
    UINT32                      guidToNameIndex = 0;
    UINT32                      guidToNameTableSize = 0;
    LPBYTE                      pData = NULL;
    ULONG                       secondaryDataSize = 0;
    ULONG                       buffersize = 0;
    size_t                      BytesWritten = 0;
    WCHAR                       Filename[MAX_PATH];
    DEVICE_IO                   hFile;

    guidToNameTableSize = sizeof(GUIDToName) / sizeof(GUIDToName[0]);

    hr = DebugCreate(
                __uuidof(IDebugClient5), 
                (void**) &pDbgClient
                );

    if (FAILED(hr) || pDbgClient == NULL) {
          LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to  Get IDebugClient5");
            goto Exit;
    }
    
    hr = pDbgClient->QueryInterface(
                        __uuidof(IDebugControl4),
                        (void**) &pDbgControl
                        );
    if (FAILED(hr) || pDbgControl == NULL) {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to  Get IDebugControl4");
            goto Exit;
    }
   
    hr = pDbgClient->QueryInterface(
                        __uuidof(IDebugDataSpaces4),
                        (void**) &pDbgDataSpaces
                        );

    if (FAILED(hr) ||pDbgDataSpaces == NULL ) {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to  Get IDebugDataSpaces4");
        goto Exit;
    }

    hr = pDbgClient->OpenDumpFileWide(
                dumpfile,
                NULL
                );
    if (FAILED(hr)) {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to open Dump file: %s",
                dumpfile);

        goto Exit;
    }

    hr = pDbgControl->WaitForEvent(
                0,
                INFINITE
                );

    if (FAILED(hr)) {
          LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to WaitForEvent");
        goto Exit;
    }

    // Find the matching GUID for the section name.
    //
    for (guidToNameIndex = 0; guidToNameIndex < guidToNameTableSize; guidToNameIndex++) {

        LogLibInfoPrintf(L"Extracting data for %s.",
                         GUIDToName[guidToNameIndex].Name);

        hr = pDbgDataSpaces->ReadTagged(
            &(GUIDToName[guidToNameIndex].Guid),
            0,
            pData,
            0,
            &secondaryDataSize
        );

        if (FAILED(hr)) {
            if (hr == E_NOINTERFACE) {
                LogLibInfoPrintf(L" Data not found for %s",
                                 GUIDToName[guidToNameIndex].Name);
                continue;
            } else {
                LogLibErrorPrintf(
                    hr,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" Failed to ReadTagged for size. HR: 0x%x",
                    hr);
                goto Exit;
            }
        }

        //
        // Wpdmp.efi always copies the 20byte name to 
        // the beginning of the blob. We don't need to copy it
        // to the bin file. 
        //

        //
        // Buffer should at least have the 20 byte name. 
        //
        if (secondaryDataSize < RAW_DUMP_SECTION_HEADER_NAME_LENGTH) {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Data size is less than expected. Actual: 0x%x bytes",
                secondaryDataSize);
            continue;
        }

        buffersize = secondaryDataSize - RAW_DUMP_SECTION_HEADER_NAME_LENGTH;
        pData = (LPBYTE)malloc(buffersize);
        if (pData == NULL) {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to allocate data ");
            goto Exit;
        }

        hr = pDbgDataSpaces->ReadTagged(
            &(GUIDToName[guidToNameIndex].Guid),
            RAW_DUMP_SECTION_HEADER_NAME_LENGTH,
            pData,
            buffersize,
            &secondaryDataSize
        );
        if (FAILED(hr)) {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to ReadTagged for Data");
            goto Exit;
        }

        wsprintf(Filename, L"%s", GUIDToName[guidToNameIndex].Name);

        if (FALSE == hFile.Open(Filename))
        {
            LogLibErrorPrintf(
                GetLastError(),
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Create File Failed. INVALID_HANDLE_VALUE. %s\n", Filename);
            goto Exit;
        }

        if (FAILED(hFile.Write((PCHAR)pData, buffersize, &BytesWritten)))
        {
            LogLibErrorPrintf(
                GetLastError(),
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to write SV Section to  File");
            goto Exit;
        }

        hFile.Close();
        if (pData) {
            free(pData);
            pData = NULL;
        }
        LogLibInfoPrintf(L"Written SV section to File %s.\r\n", Filename);
    }

    hr = S_OK;

Exit:
    hFile.Close();
    if(pData) {
        free(pData);
    }

    return hr;
}


VOID
DumpDDRMemoryMap(
    _In_ PDDR_MEMORY_MAP DDRMemoryMap
    )
{
    LogLibInfoPrintf(L"DDR_MEMORY_MAP: 0x%p\r\n", DDRMemoryMap);
    LogLibInfoPrintf(L"\tBase    : 0x%I64x\r\n", DDRMemoryMap->Base);
    LogLibInfoPrintf(L"\tEnd     : 0x%I64x\r\n", DDRMemoryMap->End);
    LogLibInfoPrintf(L"\tSize    : 0x%I64x\r\n", DDRMemoryMap->Size);
    LogLibInfoPrintf(L"\tOffset  : 0x%I64x\r\n", DDRMemoryMap->Offset);
    LogLibInfoPrintf(L"\tCont    : 0x%x\r\n", DDRMemoryMap->Contiguous);
    LogLibInfoPrintf(L"\tType    : 0x%x\r\n", DDRMemoryMap->Type);
    LogLibInfoPrintf(L"\tDDRIndex: %u\r\n", DDRMemoryMap->DDRIndex);
}




HRESULT
CopyFromDumpToBin(
    DEVICE_IO *outFile,
    LPBYTE IoBuffer,
    PULONG NonOSByteOffset,
    PDDR_MEMORY_MAP Map,
    IDebugDataSpaces4 *DataSpaces
)
{
    HRESULT         hr = S_OK;

    if ((nullptr == DataSpaces) ||
        (nullptr == Map) ||
        (nullptr == NonOSByteOffset) ||
        (nullptr == IoBuffer) ||
        (nullptr == outFile)
        )
    {
        hr = E_INVALIDARG;
    }
    else
    {
        ULONG64         base = Map->Base;
        ULONG           bytesRemain = (ULONG)Map->Size;
        ULONG           ioIterations = (ULONG)(Map->Size / IO_BUFFER_SIZE);
        GUID            nonOSMemoryGUID = NON_OS_DDR_GUID;
        ULONG           offset = *NonOSByteOffset;


        if ((Map->Size % IO_BUFFER_SIZE) != 0)
        {
            ioIterations++;
        }

        LogLibInfoPrintf(L"%u iterations required to copy 0x%x bytes of data.\r\n",
                         ioIterations,
                         Map->Size);

        RtlZeroMemory(IoBuffer, IO_BUFFER_SIZE);

        for (ULONG ioIteration = 0; ioIteration < ioIterations; ioIteration++)
        {
            ULONG           bytesRead = 0;
            size_t          bytesWritten = 0;
            ULONG           ioSizeInBytes = (bytesRemain < IO_BUFFER_SIZE) ? bytesRemain : IO_BUFFER_SIZE;

            if (Map->Type == MEMORY_NONOS)
            {
                LogLibInfoPrintf(L"[%u] Calling ReadTagged. Offset: 0x%x Size: 0x%x\r\n",
                                 ioIteration,
                                 offset,
                                 ioSizeInBytes);

                if (FAILED(hr = DataSpaces->ReadTagged(&nonOSMemoryGUID,
                                                       offset,
                                                       IoBuffer,
                                                       ioSizeInBytes,
                                                       NULL))
                    )
                {
                    LogLibErrorPrintf(
                        hr,
                        __LINE__,
                        WIDEN(__FUNCTION__),
                        __WFILE__,
                        L" Failed to ReadTagged for Data. Result: 0x%x", hr);
                    goto Exit;
                }

                bytesRead = ioSizeInBytes;
            }
            else if (Map->Type == MEMORY_OS)
            {
                LogLibInfoPrintf(L"[%u] Calling ReadPhysical. Base: 0x%I64x Size: 0x%x\r\n",
                                 ioIteration,
                                 base,
                                 ioSizeInBytes);

                DataSpaces->ReadPhysical((ULONG64)base,
                                         IoBuffer,
                                         ioSizeInBytes,
                                         &bytesRead);

                if (bytesRead != ioSizeInBytes)
                {
                    hr = E_FAIL;
                    LogLibErrorPrintf(
                        E_FAIL,
                        __LINE__,
                        WIDEN(__FUNCTION__),
                        __WFILE__,
                        L" Failed to read physical memory.");
                    goto Exit;
                }

            }
            else
            {
                hr = E_FAIL;
                LogLibErrorPrintf(
                    E_FAIL,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" Unexpected map type encountered: 0x%x", Map->Type);
                goto Exit;
            }

            //
            // Write to the bin file.
            //
            if (FAILED(hr = outFile->Write((PCHAR)IoBuffer, bytesRead, &bytesWritten)))
            {
                LogLibErrorPrintf(
                    hr,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" ERROR: Failed on write DDR Section to File.");
                goto Exit;
            }
            else if (bytesRead != bytesWritten)
            {
                hr = E_FAIL;
                LogLibErrorPrintf(
                    hr,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" ERROR: write DDR Section incorrect number of bytes, Expected: %#x  Actual: %#x",
                    bytesRead, bytesWritten);
                goto Exit;
            }

            bytesRemain -= (ULONG)bytesWritten;

            offset = (Map->Type == MEMORY_NONOS) ?
                (offset + (ULONG)bytesWritten) :
                (offset);

            base = (Map->Type == MEMORY_OS) ?
                (base + (ULONG)bytesWritten) :
                base;

            LogLibInfoPrintf(L"[%u] 0x%x bytes remain.\r\n",
                             ioIteration,
                             bytesRemain);

        }//for ioIteration

    Exit:

        *NonOSByteOffset = offset;
    }

    return hr;
}


HRESULT
ReconstructDDRSectionsFromDumpFile(LPWSTR dumpfile)
{
    CComPtr<IDebugDataSpaces4>  pDbgDataSpaces;
    CComPtr<IDebugClient5>      pDbgClient;
    CComPtr<IDebugControl4>     pDbgControl;
    HRESULT                     hr = S_OK;
    ULONG                       mapSize = 0;
    WCHAR                       filename[MAX_PATH];
    DEVICE_IO                   hFile;
    GUID                        memMapGUID = MEMORY_MAP_GUID;
    ULONG                       mapCount = 0;
    ULONG                       mapIndex = 0;
    ULONG                       nonOSByteOffset = 0;
    ULONG                       previousDDRSectionIndex = 0;
    LPBYTE                      ioBuffer = NULL;
    PDDR_MEMORY_MAP             memMap = NULL;
    ULONG                       totalSizeInBytes = 0;
    GUID                        nonOSMemoryGUID = NON_OS_DDR_GUID;

    hr = DebugCreate(
        __uuidof(IDebugClient5),
        (void**)&pDbgClient
    );

    if (FAILED(hr) || pDbgClient == NULL) {
        LogLibErrorPrintf(
            hr,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to  Get IDebugClient5");
        goto Exit;
    }

    hr = pDbgClient->QueryInterface(
        __uuidof(IDebugControl4),
        (void**)&pDbgControl
    );
    if (FAILED(hr) || pDbgControl == NULL) {
        LogLibErrorPrintf(
            hr,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to  Get IDebugControl4");
        goto Exit;
    }

    hr = pDbgClient->QueryInterface(
        __uuidof(IDebugDataSpaces4),
        (void**)&pDbgDataSpaces
    );

    if (FAILED(hr) || pDbgDataSpaces == NULL) {
        LogLibErrorPrintf(
            hr,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to  Get IDebugDataSpaces4");
        goto Exit;
    }

    hr = pDbgClient->OpenDumpFileWide(
        dumpfile,
        NULL
    );
    if (FAILED(hr)) {
        LogLibErrorPrintf(
            hr,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to open Dump file");

        goto Exit;
    }

    hr = pDbgControl->WaitForEvent(
        0,
        INFINITE
    );

    if (FAILED(hr)) {
        LogLibErrorPrintf(
            hr,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to WaitForEvent");
        goto Exit;
    }

    hr = pDbgDataSpaces->ReadTagged(
        &memMapGUID,
        0,
        NULL,
        0,
        &mapSize
    );

    if (FAILED(hr)) {
        LogLibErrorPrintf(
            hr,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to find memory map GUID");
        goto Exit;
    }

    mapCount = mapSize / sizeof(DDR_MEMORY_MAP);

    LogLibInfoPrintf(L"Data size for memory map is: 0x%x. Number of elements: %u\r\n",
                     mapSize,
                     mapCount);

    memMap = (PDDR_MEMORY_MAP)malloc(mapSize);

    if (memMap == NULL)
    {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to allocate 0x%x bytes for mem map.\r\n",
            mapSize);
        goto Exit;
    }

    hr = pDbgDataSpaces->ReadTagged(&memMapGUID,
                                    0,
                                    memMap,
                                    mapSize,
                                    &mapSize
    );

    if (FAILED(hr)) {
        LogLibErrorPrintf(
            hr,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to find memory map GUID");
        goto Exit;
    }

    hr = pDbgDataSpaces->ReadTagged(&nonOSMemoryGUID,
                                    0,
                                    NULL,
                                    0,
                                    &totalSizeInBytes
    );

    LogLibInfoPrintf(L"Total size of NonOS data is 0x%x\r\n", totalSizeInBytes);

    ioBuffer = (LPBYTE)malloc(IO_BUFFER_SIZE);

    if (ioBuffer == NULL)
    {
        LogLibErrorPrintf(
            E_FAIL,
            __LINE__,
            WIDEN(__FUNCTION__),
            __WFILE__,
            L" Failed to allocate 0x%x bytes for IO buffer.\r\n",
            IO_BUFFER_SIZE);
        goto Exit;
    }

    for (mapIndex = 0; mapIndex < mapCount; mapIndex++)
    {
        LogLibInfoPrintf(L"***************Starting on map %u.***************\r\n",
                         mapIndex);

        DumpDDRMemoryMap(&memMap[mapIndex]);

        //
        // Check if we need to create a new .bin file.
        //
        if (previousDDRSectionIndex != memMap[mapIndex].DDRIndex)
        {
            LogLibInfoPrintf(L"DDR index changed. Moving to a new file.\r\n");

            if (hFile.GetError() != DEVICE_IO::IO_OK)
            {
                hFile.Close();
            }
        }

        //
        // Create the file if not already.
        //
        if (hFile.GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE)
        {
            wsprintf(filename, L"DDRCS%u.bin", memMap[mapIndex].DDRIndex);

            LogLibInfoPrintf(L"Creating DDR bin file: %s\r\n", filename);

            if (FAILED(hFile.Open(filename)))
            {
                LogLibErrorPrintf(
                    E_FAIL,
                    __LINE__,
                    WIDEN(__FUNCTION__),
                    __WFILE__,
                    L" Create File Failed. INVALID_HANDLE_VALUE. %s. Error: %u\r\n",
                    filename,
                    GetLastError());
                goto Exit;
            }

        }

        previousDDRSectionIndex = memMap[mapIndex].DDRIndex;

        LogLibInfoPrintf(L"About to copy range %u to bin.\r\n",
                         mapIndex);

        hr = CopyFromDumpToBin(&hFile,
                               ioBuffer,
                               &nonOSByteOffset,
                               &memMap[mapIndex],
                               pDbgDataSpaces);

        if (FAILED(hr)) {
            LogLibErrorPrintf(
                hr,
                __LINE__,
                WIDEN(__FUNCTION__),
                __WFILE__,
                L" Failed to copy memory range %u.\r\n", mapIndex);
            goto Exit;
        }

        LogLibInfoPrintf(L"Current NonOS offset is: 0x%I64x\r\n", nonOSByteOffset);
    }

Exit:

    hFile.Close();

    if (memMap != NULL)
    {
        free(memMap);
        memMap = NULL;
    }

    if (ioBuffer != NULL)
    {
        free(ioBuffer);
        ioBuffer = NULL;
    }

    return hr;
}


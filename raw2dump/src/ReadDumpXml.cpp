//
// ReadDumpXml.cpp : Read info from the offline dump XML and store it in the DMP_CONTEXT
//

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <windows.h>
#include <winioctl.h>
#include <windef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strsafe.h>
#include <atlbase.h>
#include <xmllite.h>

#include "raw2dump.h"
#include "dumputil.h"

//
// ------------------------- Function Definitions -------------------------------------------------------------
//


//
// Read the specified XML file into a buffer.
//
// Return a pointer to the buffer on success.  Caller must free the buffer.
//
HRESULT
ReadXmlIntoBuffer(
    _In_ PCWSTR  XmlFileName,
    _Out_ PWSTR* XmlBuffer,
    _Out_ DWORD* XmlBufferLength
    )
{
    DWORD ret = ERROR_SUCCESS;
    HRESULT hr;
    LARGE_INTEGER FileSize = {0};
    PWSTR XmlBuf = nullptr;
    HANDLE hXml = CreateFile(
                     XmlFileName,
                     GENERIC_READ,
                     0, // no sharing
                     nullptr,
                     OPEN_EXISTING,
                     FILE_FLAG_SEQUENTIAL_SCAN,
                     nullptr
                     );
    if (hXml == INVALID_HANDLE_VALUE) {
        ret = GetLastError();
        hr = HRESULT_FROM_WIN32(ret);
        TraceError("CreateFile of XML failed", "HRESULT", hr);
        goto Error;
    }

    if (!GetFileSizeEx(hXml, &FileSize)) {
        ret = GetLastError();
        hr = HRESULT_FROM_WIN32(ret);
        TraceError("GetFileSizeEx of XML failed", "HRESULT", hr);
        goto Error;
    }

    if (FileSize.HighPart != 0) {
        hr = HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
        TraceError("XML file is too large", "HRESULT", hr);
        goto Error;
    }

    DWORD BufSize = FileSize.LowPart;
    XmlBuf = (PWSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, BufSize);
    if (XmlBuf == nullptr) {
        hr = HRESULT_FROM_WIN32(ERROR_OUTOFMEMORY);
        TraceError("HeapAlloc for XML failed", "HRESULT", hr);
        goto Error;
    }

    if (ReadFile(hXml, XmlBuf, BufSize, nullptr, nullptr)) {
        // SUCCESS!
        *XmlBuffer = XmlBuf;
        *XmlBufferLength = BufSize;
        hr = S_OK;
    } else {
        hr = HRESULT_FROM_WIN32(GetLastError());
        TraceError("ReadFile of XML failed", "HRESULT", hr);
        HeapFree(GetProcessHeap(), NULL, XmlBuf);
        XmlBuf = nullptr;
    }

Error:
    if (hXml != INVALID_HANDLE_VALUE) {
        CloseHandle(hXml);
    }
    return hr;
}


// Populate the IStream with the XML, and associate it with the IXmlReader
HRESULT
SetupXmlReader(
    _Inout_ IXmlReader* XmlReader,
    _Inout_ IStream* Stream,
    _In_ PWSTR Xml,
    _In_ ULONG XmlLength
    )
{
    ULONG BytesWritten;
    LARGE_INTEGER Offset;
    HRESULT hr;
        
    hr = Stream->Write(Xml, XmlLength, &BytesWritten);
    if (FAILED(hr)) {
        goto Error;
    }

    if (BytesWritten != XmlLength) {
        hr = E_FAIL;
        goto Error;
    }

    Offset.QuadPart = 0;
    hr = Stream->Seek(Offset, STREAM_SEEK_SET, nullptr);
    if (FAILED(hr)){
        goto Error;
    }

    hr = XmlReader->SetInput(Stream);
    if (FAILED(hr)){
        goto Error;
    }

Error:
    return hr;
}


//
// Convert the string value at the current location to a ULONGLONG
//
HRESULT
XmlStringToULONGLONG(
   _In_ IXmlReader* XmlReader,
   ULONGLONG * pULL
   )
{
    LPCWSTR strValue;
    HRESULT hr = XmlReader->GetValue(&strValue, nullptr);
    if (FAILED(hr)) {
        return hr;
    }

    *pULL = _wcstoui64(strValue, nullptr, 16);
    return S_OK;
}


//
// Retrieve the specified attribute and convert its text value to a ULONGLONG
//
HRESULT
XmlAttrToULONGLONG(
   _In_ IXmlReader* XmlReader,
   PCWSTR AttributeName,
   ULONGLONG * pULL
   )
{
    HRESULT hr = XmlReader->MoveToAttributeByName(AttributeName, nullptr);
    if (hr == S_OK) {
        hr = XmlStringToULONGLONG(XmlReader, pULL);
    }
    return hr;
}


//
// Extract several values from an offline dump XML:
//
// 1. Dump header instance
// 2. AP_REG physical address
// 3. BugCheck parameters
//
HRESULT 
SetContextSettingsFromXml(
    _In_ IXmlReader* XmlReader,
    _Inout_ DMP_CONTEXT * pContext
    )
{
    XmlNodeType NodeType;
    const WCHAR* ElementName;
    HRESULT hr;

    while (S_OK == (hr = XmlReader->Read(&NodeType))) {
        if (NodeType == XmlNodeType_Element) {
            if (FAILED(hr = XmlReader->GetLocalName(&ElementName, nullptr))) {
                goto Error;
            }

            if (!_wcsicmp(ElementName, L"DumpHeader")) {
                TraceInfo("At DumpHeader\n");
                // There may be multiple dump headers in a raw dump. OffDumpSvc figures out the instance
                // number of the right one to use and indicates it in the XML.
                if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"Instance", &pContext->DumpInstance.QuadPart))) {
                    goto Error;
                }
            } else if (!_wcsicmp(ElementName, L"APREGAddress")) {
                TraceInfo("At APREGAddress\n");
                // OffDumpSvc finds this SV-specific block and indicates its file offset with this element.
                if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"PA", (ULONGLONG *)&pContext->APRegAddress.QuadPart))) {
                    goto Error;
                }
            } else if (!_wcsicmp(ElementName, L"InMemDiagBuffer")) {
                ULONGLONG ullTmp;
                hr = XmlAttrToULONGLONG(XmlReader, L"VA", &ullTmp);
                if (FAILED(hr)) {
                    goto Error;
                }
                pContext->InMemDataInfo.DataVA = (PVOID)ullTmp;

                hr = XmlAttrToULONGLONG(XmlReader, L"PA", (ULONGLONG *)&pContext->InMemDataInfo.DataPA.QuadPart);
                if (FAILED(hr)) {
                    goto Error;
                }

                hr = XmlAttrToULONGLONG(XmlReader, L"Size", &ullTmp);
                if (FAILED(hr)) {
                    goto Error;
                }
                pContext->InMemDataInfo.Size = (UINT32)ullTmp;

            } else if (!_wcsicmp(ElementName, L"BugCheckParameters")) {
                TraceInfo("At BugCheckParameters\n");

                LARGE_INTEGER liTmp = {0};
                if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"BugCheckCode", (ULONGLONG *)&liTmp.QuadPart))) {
                    goto Error;
                }
                if (liTmp.QuadPart == 0) {
                    if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"BugcheckCode", (ULONGLONG *)&liTmp.QuadPart))) {
                        goto Error;
                    }
                }

                pContext->BugCheckCode = liTmp.LowPart;

                if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"Param1", (ULONGLONG *)&pContext->BugCheckParam1))) {
                    goto Error;
                }
                if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"Param2", (ULONGLONG *)&pContext->BugCheckParam2))) {
                    goto Error;
                }
                if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"Param3", (ULONGLONG *)&pContext->BugCheckParam3))) {
                    goto Error;
                }
                if (FAILED(hr = XmlAttrToULONGLONG(XmlReader, L"Param4", (ULONGLONG *)&pContext->BugCheckParam4))) {
                    goto Error;
                }
            } else {
                continue; // Move to next element
            }
        }
    }

    hr = S_OK;

    TraceInfo1("From XML", "DumpHeaderInstance", pContext->DumpInstance.QuadPart);
    TraceInfo1("From XML", "APREGAddress", pContext->APRegAddress.QuadPart);
    TraceInfo1("From XML", "BugCheckCode", pContext->BugCheckCode);
    TraceInfo1("From XML", "BugCheck Param1", pContext->BugCheckParam1);
    TraceInfo1("From XML", "BugCheck Param2", pContext->BugCheckParam2);
    TraceInfo1("From XML", "BugCheck Param3", pContext->BugCheckParam3);
    TraceInfo1("From XML", "BugCheck Param4", pContext->BugCheckParam4);

Error:
    return hr;
}            


//
// Read the offline dump XML info and populate DMP_CONTEXT
//
HRESULT
UpdateContextFromXml(
    _Inout_ DMP_CONTEXT * pContext
    )
{
    CComPtr<IXmlReader> XmlReader = nullptr;
    CComPtr<IStream> Stream = nullptr;
    HRESULT hr = E_FAIL;

    PWSTR XmlBuffer = nullptr;
    DWORD XmlBufferLength = 0;

    if (pContext->rawdumpInfoFilePath == nullptr) {
        TraceError("Rawdump info file path NULL", "HRESULT", hr);
        goto Error;
    }

    hr = ReadXmlIntoBuffer(pContext->rawdumpInfoFilePath, &XmlBuffer, &XmlBufferLength);
    if (FAILED(hr)) {
        goto Error;
    }

    // Create XML reader and stream
    hr = CreateXmlReader(__uuidof(IXmlReader), (void**) &XmlReader, nullptr);
    if (FAILED(hr)) {
        goto Error;
    }

    hr = XmlReader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
    if (FAILED(hr)) { 
        goto Error;
    }

    hr = CreateStreamOnHGlobal(nullptr, TRUE, &Stream);
    if (FAILED(hr)) {
        goto Error;
    }

    // Set up stream to point to the XML buffer containing offline dump info
    // and make the reader to point to stream as its input
    hr = SetupXmlReader(XmlReader, Stream, XmlBuffer, XmlBufferLength);
    if (FAILED(hr)) {
        goto Error;
    }

    //
    // Parse XML to get some offline dump settings
    //
    hr = SetContextSettingsFromXml(XmlReader, pContext);

Error:
    if (XmlBuffer) {
        HeapFree(GetProcessHeap(), NULL, XmlBuffer);
    }

    return hr;
}

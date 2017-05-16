/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
offdmpistream.cpp

Environment:
User Mode

--*/

#include "offdmpistream.h"

OffDmpIStream::OffDmpIStream() :
m_hFile(INVALID_HANDLE_VALUE)
{
    // Do Nothing.
}

OffDmpIStream::~OffDmpIStream()
{
    if (m_hFile != INVALID_HANDLE_VALUE)
    {
        printf("Closing file\n");
        CloseHandle(m_hFile);
    }
}

HRESULT OffDmpIStream::OpenFile(_In_ LPCWSTR pszFileName)
{
    HRESULT Result = S_OK;

    m_hFile = CreateFileW(
                pszFileName,
                GENERIC_WRITE,
                0,
                NULL,
				OPEN_ALWAYS,
                FILE_ATTRIBUTE_NORMAL,
                NULL);

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        printf("Failed to open \n");
        return __HRESULT_FROM_WIN32(GetLastError());
    }

    return Result;
}

STDMETHODIMP OffDmpIStream::Write(
    _In_ const void *buffer,
    ULONG cb,    
    _Out_opt_  ULONG *pcbWritten)
{
    ULONG dwBytesWritten = 0;
    BOOL bResult;    

    if (m_hFile == INVALID_HANDLE_VALUE)
    {
        if (pcbWritten)
        {
            *pcbWritten = 0;
        }
        return E_HANDLE;
    }

    bResult = WriteFile(m_hFile, buffer, cb, &dwBytesWritten, NULL);

    if (pcbWritten)
    {
        *pcbWritten = dwBytesWritten;
    }

    if (bResult)
    {
        return S_OK;
    }

    return __HRESULT_FROM_WIN32(GetLastError());
}

STDMETHODIMP OffDmpIStream::QueryInterface(
    _In_ REFIID riid,
    _In_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
    if (riid == IID_IStream)
    {
        *ppvObject = static_cast<IStream *>(this);
        AddRef();
        return S_OK;
    }

    if (riid == IID_IUnknown)
    {
        *ppvObject = static_cast<IUnknown *>(this);
        AddRef();
        return S_OK;
    }

    *ppvObject = NULL;
    return E_NOINTERFACE;
}


//
//    WARNING:
//
// This class is not supposed to be allocated with new on the heap.
// It's designed to be used from the stack. Please see the
// description at the top of this file.

ULONG STDMETHODCALLTYPE OffDmpIStream::AddRef()
{
    return 0xFFFFFFFF;
}

ULONG STDMETHODCALLTYPE OffDmpIStream::Release()
{
    return 0xFFFFFFFF;
}

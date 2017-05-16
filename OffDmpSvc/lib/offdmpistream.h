/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
offdmpistream.h

Environment:
User Mode

--*/

#pragma once
#include <windows.h>
#include <stdlib.h>
#include <shlwapi.h>
#include <xmllite.h>
#include <objbase.h>
#include <atlbase.h>

//
// This class implements IStream interface in order to use 
// XML writer to write to the rawdumpinfo sxml file. 
// Architecture specific implementations may use this interface to 
// generate rawdumpinfo.xml containing device specific and SV Specific 
// information. 
//
class OffDmpIStream : public IStream
{
public:
    OffDmpIStream();
    ~OffDmpIStream();
    
    /*++

    Routine Description:

    This function opens the rawdumpinfo file for writing.
        An existing file is truncated.

    Arguments:
        pszFileName: File name 

    Return Value:
        HRESULT

    --*/
    HRESULT OpenFile(_In_ LPCWSTR pszFileName);

    //
    // IStream implementation:
    //

    /*++

    Routine Description:
        Writes to the rawdumpinfo file

    Arguments:
    buffer: A pointer to the buffer containing the data to be written  
        cb : Number of bytes to be written
        pcbWritten: pointer to the number of bytes actually written

    Return Value:
        HRESULT

    --*/
        
    HRESULT STDMETHODCALLTYPE Write(
        _In_ const void *buffer,
        ULONG cb,
        _Out_opt_  ULONG *pcbWritten);

    //
    // IUnknown implementation:
    //
    HRESULT STDMETHODCALLTYPE QueryInterface(
        _In_ REFIID,
        _In_ void __RPC_FAR *__RPC_FAR *);

    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();

    //
    // Non-implemented IStream functions:
    //
    HRESULT STDMETHODCALLTYPE Read(
        _Out_writes_bytes_to_(cb, *pcbRead)  void * buffer,
        ULONG cb,
        _Out_opt_ ULONG * pcbRead)
    {
        UNREFERENCED_PARAMETER(buffer);
        UNREFERENCED_PARAMETER(cb);
        UNREFERENCED_PARAMETER(pcbRead);

        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE Seek(
        LARGE_INTEGER,
        DWORD,
        _Out_opt_  ULARGE_INTEGER *)
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }


    HRESULT STDMETHODCALLTYPE SetSize(
        ULARGE_INTEGER)
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE CopyTo(
        IStream *,
        ULARGE_INTEGER,
        _Out_opt_  ULARGE_INTEGER *,
        _Out_opt_  ULARGE_INTEGER *)
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE Commit(
        DWORD)
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE Revert()
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE LockRegion(
        ULARGE_INTEGER,
        ULARGE_INTEGER,
        DWORD)
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE UnlockRegion(
        ULARGE_INTEGER,
        ULARGE_INTEGER,
        DWORD)
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE Stat(
        __RPC__out STATSTG *,
        DWORD)
    {
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

    HRESULT STDMETHODCALLTYPE Clone(
         __RPC__deref_out_opt IStream ** ppClone)
    {
        if (ppClone) {
            *ppClone = NULL;
        }
        return HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED);
    }

private:

    HANDLE m_hFile;
};

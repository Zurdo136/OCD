/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
Intelx86.cpp

Environment:
User Mode

--*/


#include "intelx86.h"
#include "wpcrdmpsentinel.h"
#include <bugcodes.h>
#define NO_INTERFACE_DECL
#include <ntefi.h>
#undef NO_INTERFACE_DECL
#include "offdmpistream.h"
#include "buildparams.h"

Intelx86::Intelx86(
    _In_ PDMP_CONTEXT Context) :
    m_Context(Context),    
    m_AddressOfDummyFunction(0)
{
    m_CpuContextAddress.QuadPart = Context->CPUContextAddress.QuadPart;
}

HRESULT
Intelx86::ProcessSVSpecific(VOID)
{
    HRESULT result = E_FAIL;
    bool    IsPresent = FALSE;

    TraceInfo("=== Finished getting dump instance. Get SV section specific info. ====");
   
    //
    // Check if the rawdump has context in it.
    //
    IsPresent = IsCpuContextPresent();
    if (!IsPresent) {
        TraceInfo("ERROR: Failed to get the CPU Context");
        goto Exit;
    }
    
    result = S_OK;

Exit:
    return result;

}

bool
Intelx86::IsCpuContextPresent(
)
{
    return(m_Context->CPUContextAddress.QuadPart != 0);
}

VOID
Intelx86::BuildBugCheckParams(VOID)
{
    TraceInfo("Building bugcheck data...\r\n");

    //
    // Intel has only implemented Watchdog reset. 
    //
    m_Context->DumpHeader->BugCheckCode = FATAL_ABNORMAL_RESET_ERROR;
    m_Context->DumpHeader->BugCheckParameter1 = ONEFOURC_PARAM1_WATCHDOG;
    m_Context->DumpHeader->BugCheckParameter2 = ONEFOURC_PARAM2_NONSECURE_WDOG;
    m_Context->DumpHeader->BugCheckParameter3 = ONEFOURC_VERSION1_PARAM3_DEFAULT;
    m_Context->DumpHeader->BugCheckParameter4 = ONEFOURC_VERSION1_PARAM4_DEFAULT;

}




HRESULT
Intelx86::BuildInfoFile(VOID)
{
    HRESULT              hr;
    OffDmpIStream        StreamFromFile;
    CComPtr<IXmlWriter>  pWriter;

    if (FAILED(hr = StreamFromFile.OpenFile(m_Context->RawDumpInfoPath))) {
        TraceHRESULT("Error creating file writer", hr);
        goto Exit;
    }

    if (FAILED(hr = CreateXmlWriter(__uuidof(IXmlWriter), (void**)&pWriter, NULL)))  {
        TraceHRESULT("Error creating xml writer", hr);
        goto Exit;
    }

    if (FAILED(hr = pWriter->SetOutput(static_cast<IUnknown *>(&StreamFromFile)))) {
        TraceHRESULT("Error setting output for writer", hr);
        goto Exit;
    }

    if (FAILED(hr = pWriter->SetProperty(XmlWriterProperty_Indent, TRUE)))    {
        TraceHRESULT("Error, Method: SetProperty XmlWriterProperty_Indent", hr);
        goto Exit;
    }

    if (FAILED(hr = pWriter->WriteStartDocument(XmlStandalone_Omit))){
        goto Exit;
    }

    //
    // Write to the File 
    // <DeviceSpecificInfo>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"DeviceSpecificInfo", NULL))) {
        goto Exit;
    }

    //
    // <DumpHeader> 
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"DumpHeader", NULL))) {
        goto Exit;
    }

    //
    // Write the Dump header Instance.
    // 
    // Instance="m_Context->DumpInstance.QuadPart"
    //  
    if (FAILED(hr = WriteUINT64AttrString(pWriter,
        m_Context->DumpInstance.QuadPart,
        L"Instance"))) {
        goto Exit;
    }

    //
    // </DumpHeader>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        TraceHRESULT("Error, Method: WriteFullEndElement: DumpHeader", hr);
        goto Exit;
    }

    //
    // <SVSpecific>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"SVSpecific", NULL))) {
        goto Exit;
    }

    //
    // <Intelx86>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"Intelx86", NULL))) {
        goto Exit;
    }

    //
    // <CpuContextAddress>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"CpuContextAddress", NULL))) {
        goto Exit;
    }

    //
    // PA = "m_CpuContextAddress.QuadPart"
    //
    if (FAILED(hr = WriteUINT64AttrString(pWriter,
        m_CpuContextAddress.QuadPart,
        L"PA"))) {
        goto Exit;
    }

    //
    // </CpuContextAddress>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }


    //
    // </Intelx86>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    //
    // </SVSpecific>
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    //
    // <BugCheckParameters>
    //
    if (FAILED(hr = pWriter->WriteStartElement(NULL, L"BugCheckParameters", NULL))) {
        goto Exit;
    }

    //
    // BugcheckCode="m_Context->DumpHeader->BugCheckCode"
    //   
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
        m_Context->DumpHeader->BugCheckCode,
        L"BugCheckCode"))) {
        goto Exit;
    }

    //
    // Param1="m_Context->DumpHeader->BugCheckParameter1"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
        m_Context->DumpHeader->BugCheckParameter1,
        L"Param1"))) {
        goto Exit;
    }
    //
    // Param2="m_Context->DumpHeader->BugCheckParameter2"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
        m_Context->DumpHeader->BugCheckParameter2,
        L"Param2"))) {
        goto Exit;
    }

    //
    // Param3="m_Context->DumpHeader->BugCheckParameter3"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
        m_Context->DumpHeader->BugCheckParameter3,
        L"Param3"))) {
        goto Exit;
    }

    //
    // Param4="m_Context->DumpHeader->BugCheckParameter4"
    // 
    if (FAILED(hr = WriteUINT32AttrString(pWriter,
        m_Context->DumpHeader->BugCheckParameter4,
        L"Param4"))) {
        goto Exit;
    }

    //
    // </BugCheckParameters> 
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    //
    // </DeviceSpecificInfo> 
    //
    if (FAILED(hr = pWriter->WriteFullEndElement())){
        goto Exit;
    }

    if (FAILED(hr = pWriter->WriteEndDocument())) {
        goto Exit;
    }
    hr = S_OK;
Exit:
    return hr;
}

HRESULT
Intelx86::BuildInfoBuffer(
    _Out_ PDEVICE_SPECIFIC_INFO DeviceSpecificInfo
    )
{
    HRESULT hr = S_OK;

    if (nullptr == DeviceSpecificInfo)
    {
        hr = E_FAIL;
    }
    else
    {
        DeviceSpecificInfo->Type = PROCESSOR_ARCHITECTURE_INTEL;
        DeviceSpecificInfo->DumpHeaderInstanceID = m_Context->DumpInstance.QuadPart;
        DeviceSpecificInfo->CpuContextAddress = m_CpuContextAddress.QuadPart;
        DeviceSpecificInfo->BugCheckCode = m_Context->DumpHeader->BugCheckCode;
        DeviceSpecificInfo->BugCheckParam1 = m_Context->DumpHeader->BugCheckParameter1;
        DeviceSpecificInfo->BugCheckParam2 = m_Context->DumpHeader->BugCheckParameter2;
        DeviceSpecificInfo->BugCheckParam3 = m_Context->DumpHeader->BugCheckParameter3;
        DeviceSpecificInfo->BugCheckParam4 = m_Context->DumpHeader->BugCheckParameter4;
    }

    return hr;

}

//
//----------------- Helper function -----------------------------
//
HRESULT
Intelx86::WriteUINT64AttrString(
_In_ CComPtr<IXmlWriter> pWriter,
_In_ ULONGLONG           LongLongIntegerValue,
_In_ LPCWSTR             AttributeName
)
{
    HRESULT hr;
    WCHAR   pszDest[30];
    if (FAILED(hr = StringCchPrintfW(pszDest,
        ARRAYSIZE(pszDest),
        L"0x%I64x",
        LongLongIntegerValue))){
        goto Exit;
    }
    if (FAILED(hr = pWriter->WriteAttributeString(NULL, AttributeName,
        NULL, pszDest))) {
        goto Exit;
    }
Exit:
    return hr;
}


HRESULT
Intelx86::WriteUINT32AttrString(
_In_ CComPtr<IXmlWriter> pWriter,
_In_ UINT32              IntegerValue,
_In_ LPCWSTR             AttributeName
)
{
    HRESULT hr;
    WCHAR   pszDest[30];
    if (FAILED(hr = StringCchPrintfW(pszDest,
        ARRAYSIZE(pszDest),
        L"0x%x",
        IntegerValue))){
        goto Exit;
    }
    if (FAILED(hr = pWriter->WriteAttributeString(NULL, AttributeName,
        NULL, pszDest))) {
        goto Exit;
    }
Exit:
    return hr;
}

Intelx86::~Intelx86()
{
    //
    // Empty.
    //
}

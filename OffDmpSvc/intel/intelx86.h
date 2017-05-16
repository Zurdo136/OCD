/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
intelx86.h

Environment:
User Mode

--*/

#pragma once 
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <xmllite.h>
#include <objbase.h>
#include <atlbase.h>

#include "svspecific.h"

#define CP15_MIDR              15, 0,  0,  0, 0         // Main ID Register

// nonstandard extension used : bit field types other than int
#pragma warning(disable: 4214) 
#include "wponefourc.h"



class Intelx86 : public SvSpecific
{

public:
    /*++

    Routine Description:
    Makes an instance of QCom32.

    Arguments:
    PDMP_CONTEXT - pointer to dump m_Context

    Return Value:
    HRESULT

    --*/
    Intelx86(_In_ PDMP_CONTEXT Context);

    /*++

    Routine Description:
    This routine implements Qualcomm 32 bit processor specific functions
    for the processing the rawdump file.

    Arguments:
    m_Context - pointer to dump m_Context

    Return Value:
    HRESULT

    --*/
    __override
        HRESULT
        ProcessSVSpecific();

    /*++

    Routine Description:

    This function builds the final bugcheck parameters.
    This is a best effort attempt to determine the bugcheck parameters.
    Hence this function returns nothing.


    Arguments:
    None

    Return Value:
    None

    --*/
    __override
        VOID
        BuildBugCheckParams(VOID);

    /*++

    Routine Description:
    This function generates an XML file containind Device specific information.

    Arguments:
    None

    Return Value:
    HRESULT

    --*/
    __override
        HRESULT
        BuildInfoFile(VOID);

    /*++

    Routine Description:
    This function populates the Device specific information (structure) from the class variables.

    Arguments:
    DeviceSpecificInfo: PDEVICE_SPECIFIC_INFO, a common data structure used to capture device 
        specific information that will (later) be used to update the RawDump.bin file.

    Return Value:
    HRESULT

    --*/
    __override
        HRESULT
        BuildInfoBuffer(
            _Out_ PDEVICE_SPECIFIC_INFO DeviceSpecificInfo
        );

    /*++
    Routine Description:
    Desctructor.

    Arguments:
    None

    Return Value:
    None

    --*/
    ~Intelx86();

private:
    //
    // Private member variables
    //
    PDMP_CONTEXT                    m_Context;
    //
    // AP REG info
    //
    LARGE_INTEGER                   m_CpuContextAddress;
    UINT32                          m_AddressOfDummyFunction;

    //
    // OneFourC related fields
    // 
    IN_MEM_DATA_INFO                m_InMemDataInfo;
    PINMEM_DIAG_BUFFER              m_InMemDiagBuffer;
    UCHAR                           m_RstStatBin[QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH];
    UCHAR                           m_PmicPonBin[QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH];


    //
    // private functions
    //

    /*++

    Routine Description:
    This function returns true if Context information is present in the dump.

    Arguments:
    IsApRegValid - Informs the caller when a valid AP_REG was found.

    Return Value:
    bool

    --*/
    bool
        IsCpuContextPresent(        
        );


    /*++

    Routine Description:
    Converts a long long to string and writes to the file
    using XmlWriter under the given attribute field

    Arguments:
    pWriter: Xml Writer
    LongLongIntegerValue: 64 bit integer value
    AttributeName: name of the attribute

    Return Value:
    HRESULT

    --*/
    HRESULT
        WriteUINT64AttrString(
        _In_ CComPtr<IXmlWriter> pWriter,
        _In_ ULONGLONG           LongLongIntegerValue,
        _In_ LPCWSTR             AttributeName
        );

    /*++

    Routine Description:
    Converts a integer to string and writes to the file
    using XmlWriter under the given attribute field

    Arguments:
    pWriter: Xml Writer
    LongLongIntegerValue: 32 bit integer value
    AttributeName: name of the attribute

    Return Value:
    HRESULT

    --*/
    HRESULT
        WriteUINT32AttrString(
        _In_ CComPtr<IXmlWriter> pWriter,
        _In_ UINT32              IntegerValue,
        _In_ LPCWSTR             AttributeName
        );

    /*++

    Routine Description:
    Returns the size of the GUID table containing GUIDs for all the
    SV sections

    Arguments:
    None

    Return Value:
    size of the GUID table

    --*/
    UINT32
        GetGUIDTableSize(VOID);

};

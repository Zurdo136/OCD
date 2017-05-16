/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name: 
    qcom32.h

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



class QCom32 : public SvSpecific
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
    QCom32(_In_ PDMP_CONTEXT Context);
    
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
        Desctructor. Releases the memory for immem diag buffer and 
        and AP Reg buffer.

    Arguments:
        None

    Return Value:
        None

    --*/
    ~QCom32();

private:
    //
    // Private member variables
    //
    PDMP_CONTEXT                    m_Context;
    //
    // AP REG info
    //
    LARGE_INTEGER                   m_APRegAddress;
    PAP_REG_B_FAMILY_HEADER         m_ApReg;    
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
        This function parses through the SV sections and more information about the Bugcheck reasons

    Arguments:
        None

    Return Value:
        HRESULT

    --*/
    HRESULT
        GetSVSpecificInfo(VOID);

    /*++

    Routine Description:
        This function reads ApReg Address from the UEFI Variable

    Arguments:
        IsApRegValid - Informs the caller when a valid AP_REG was found.

    Return Value:
        HRESULT

    --*/
    HRESULT
    QCom32::GetAPReg(
        _Out_ bool* IsApRegValid
    );

    /*++

    Routine Description:
        This routine searchs the DDR sections for inmmem data.
        Actual data if followed by a sentinel.

    Arguments:
        None

    Return Value:
        HRESULT

    --*/
    HRESULT
    GetInMemDataInfo(VOID);

    /*++

    Routine Description:
        This function fills in the contents of ONEFOURC_DIAG_BUFFER.

    Arguments:
        None

    Return Value:
        None

    --*/
    HRESULT
    BuildDiagBuffer(VOID);

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
    
    /*++

    Routine Description:
        Dumps the PMICPON section values

    Arguments:
        Pointer to the PMIC structure in memory

    Return Value:
        None

    --*/
    VOID
    DumpPmicPon(
        _In_reads_(QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH) PUCHAR PmicPonBin
    );
    
    /*++

    Routine Description:
        Dumps the RSTSTAT section values

    Arguments:
        Pointer to the RSTSTAT structure in memory

    Return Value:
        None

    --*/
    VOID
    DumpRstStat(
        _In_reads_(QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH) PUCHAR RstStatBin
    );
        
};

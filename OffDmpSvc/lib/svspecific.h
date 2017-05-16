/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved


Module:
    svspecific.h

 Abstract:
   Interface declaration of SV Specific processing for 
   rawdump file for offline crashdump

Environment:
	User Mode

--*/

#pragma once 

#include "offdmpsvc.h"

// nonstandard extension used : bit field types other than int
#pragma warning(disable: 4214) 
#include "wponefourc.h"


#define RAW_DUMP_SECTION_HEADER_NAME_LENGTH 0x14

//
// SV Specific name to GUID mapping.
//
typedef struct _SV_SPECIFIC_NAME_TO_GUID
{
    GUID    Guid;
    UCHAR   Name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
}SV_SPECIFIC_NAME_TO_GUID;

extern SV_SPECIFIC_NAME_TO_GUID GUIDToName[];

//
// This interface contains Architecture specific implementation for rawdump 
// processing. The implementer is expected to provide :-
// - Bugcheck parameters describing the reasons of the crash
// - an xml file containing windows dump header instance id and vendor/device specific 
//  addresses. 
//
class SvSpecific
{
    
public:

	/*++

	Routine Description:
		perform SV specific processing on the rawdump

	Return Value:
		HRESULT containing the error code.

	--*/
	virtual 
	HRESULT 
	ProcessSVSpecific(VOID) = 0;

	/*++

	Routine Description:
		Destructor 

	Return Value:
		None

	--*/

	/*++

	Routine Description:
	Build bugcheck parameters.

	Return Value:
	None

	Remarks:
	This is a best effort function.
	--*/
	virtual
	VOID
	BuildBugCheckParams(VOID) = 0;


	/*++

	Routine Description:
	This function should build an XML file containing 
	Dump header instance ID and SV specific/device specific
	info.

	Arguments:
	None

	Return Value:
	HRESULT

	--*/
	virtual 
	HRESULT
	BuildInfoFile(VOID) = 0;



    /*++

    Routine Description:
    This function builds a buffer that contains device specific information.
    The infrmation contained in this buffer is same as above except the fact that 
    this will be in terms of raw data format that can be written to a bin file.

    Arguments:
    None

    Return Value:
    HRESULT

    --*/
    virtual
        HRESULT
        BuildInfoBuffer(PDEVICE_SPECIFIC_INFO) = 0;


	virtual ~SvSpecific()
	{
		//
		// Empty.
		//
	}

	
};
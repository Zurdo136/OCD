/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   Device_Specific.cpp

Environment:
   User Mode

Author:
   Jose Pagan (jopagan)
--*/
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#include "Device_Specific.h"

/****************************************************************************************
**  HRESULT ReadDeviceSpecificInfo(
**              _In_    DEVICE_IO hFile,
**              _Out_writes_all_(elementCount(sizeof DEVICE_SPECIFIC_INFO))
**                      PDEVICE_SPECIFIC_INFO_PACKED Context,
**              _In_    ULONGLONG offset
**          )
**
**  This function updates the Context with the information appended to the
**  processed, rawdump.bin file, by OffDmpSvc. If the rawdumpinfo.xml file
**  does not get the same information is also embedded at the end of the
**  rawdump.bin file form the start of a 1K buffer.
**
**  Original code caused data misalignment since the structure was written on
**  ARM23/64 and read by AMD/Intel.  Thus, the writes were unaligned and updated
**  to be packed.
**
**  In the updated code, there was no version or signature to help determine if
**  any data was added to the struct. This requirement forced reworking of this
**  function such that the structures could be distinguished.
**
**  Return Value:
**      HRESULT
**
*****************************************************************************************/
HRESULT
ReadDeviceSpecificInfo(
    _In_    DEVICE_IO *hFile,
    _Out_writes_all_(sizeof DEVICE_SPECIFIC_INFO) PDEVICE_SPECIFIC_INFO Context,
    _In_    ULONGLONG offset
)
{
    HRESULT     hr = S_OK;
    PACK_FLAG   Packing = USE_PACK_UNDEFINED;
    CHAR        DeviceSpecificInfo[DEVICE_SPECIFIC_INFO_BUFFER_LENGTH] = { 0 };
    ULONG       dataVersion = DEVICE_SPECIFIC_INFO_STRUCT_OLDEST_SIGNED_VERSION;
    size_t      bytesProcessed = 0;

    if ((DEVICE_IO::IO_OK != hFile->GetError()) ||
        FAILED(hr = hFile->SetPos(offset)) ||
        FAILED(hr = hFile->Read(DeviceSpecificInfo, sizeof(DeviceSpecificInfo), &bytesProcessed)) ||
        (sizeof(DeviceSpecificInfo) != bytesProcessed)
        )
    { // Failed to read buffer with struct
        hr = E_FAIL;
    }
    else if (DEVICE_SPECIFIC_INFO_SIGNATURE == DEVICE_SPECIFIC_INFO_SIGNED_DATA(DeviceSpecificInfo, Signature))
    { // There is a signature at position zero - this is a new structure
        Packing = USE_LATEST;
        dataVersion = ((PDEVICE_SPECIFIC_INFO)&DeviceSpecificInfo)->Version;
    }
    else if (BUGCHECK_CODE == DEVICE_SPECIFIC_INFO_BASE_DATA(USE_UNSIGNED_PACKED, DeviceSpecificInfo, BugCheckCode))
    { // No signature means an old structure, pre 16/12. Therefore, check if the first data field (Type) is unpacked
        Packing = USE_UNSIGNED_PACKED;
    }
    else if(BUGCHECK_CODE == DEVICE_SPECIFIC_INFO_BASE_DATA(USE_UNSIGNED_UNPACKED, DeviceSpecificInfo, BugCheckCode))
    { // No signature means an old structure, pre 16/12. Using UnPacked structure
        Packing = USE_UNSIGNED_UNPACKED;
    }
    else
    { //Failed to determine packing
        hr = E_FAIL;
    }

    if (SUCCEEDED(hr) && (USE_PACK_UNDEFINED != Packing))
    { // Always retrieve the BASE data
        Context->Type = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, Type);
        Context->DumpHeaderInstanceID = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, DumpHeaderInstanceID);

        Context->BugCheckCode = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, BugCheckCode);
        Context->BugCheckParam1 = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, BugCheckParam1);
        Context->BugCheckParam2 = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, BugCheckParam2);
        Context->BugCheckParam3 = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, BugCheckParam3);
        Context->BugCheckParam4 = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, BugCheckParam4);

        switch (Context->Type)
        { // TYPE is the first item of all struct's (PACKED/UNPACKED) which never changes position
            case DEVICE_TYPE_INTELx86:          // Retained for backward compatibility
            case PROCESSOR_ARCHITECTURE_INTEL:
                Context->CpuContextAddress = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, CpuContextAddress);
                break;

            case DEVICE_TYPE_QCOM32:            // Retained for backward compatibility
            case PROCESSOR_ARCHITECTURE_ARM:
            case PROCESSOR_ARCHITECTURE_ARM64:
                Context->APRegPA = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, APRegPA);
                Context->VA = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, VA);
                Context->PA = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, PA);
                Context->Size = DEVICE_SPECIFIC_INFO_BASE_DATA(Packing, DeviceSpecificInfo, Size);
                break;

            default:
                hr = E_FAIL;
                break;
        }

    }

    // NOTE: below this point, process the remaining structure elements as signed, based on structure version

    return hr;
}


/****************************************************************************************
**  HRESULT  WriteDeviceSpecificInfo(
**              _In_    DEVICE_IO *hFile,
**              _In_reads_(elementCount(sizeof DEVICE_SPECIFIC_INFO))
**                      PDEVICE_SPECIFIC_INFO Context,
**              _In_    ULONGLONG offset
**           )
**
**  This function writes the Context data to a specific location in a file.
**  Although this data is always appended to the rawdump.bin, file (by OffDmpSvc), the
**  offset parameter will allow for writing anywhere within the file.
**
**  This function will always replace the Signature, Version, and PayloadSize with the
**  values appropriate to the most current data structure.
**
**  The original code caused data misalignment as the structure was written on ARM23/64
**  and read by AMD/Intel.  Thus, the writes were typically unaligned and unpacked.
**
**  In the updated code, there was no version or signature, thus to improve maintainability,
**  the struct now has a signature and a version.  Because of this change, the function
**  ReadDeviceSpecificInfo() has to be backward compatible where the legacy structures may
**  be packed or unpacked thus misaligned values need to be detected, on read.
**
**  Return Value:
**      HRESULT
**
*****************************************************************************************/
HRESULT
WriteDeviceSpecificInfo(
    _In_    DEVICE_IO *hFile,
    _In_reads_(sizeof DEVICE_SPECIFIC_INFO) PDEVICE_SPECIFIC_INFO Context,
    _In_    ULONGLONG offset
)
{
    HRESULT     hr = E_FAIL;
    PCHAR       DeviceSpecificInfo[DEVICE_SPECIFIC_INFO_BUFFER_LENGTH] = { 0 };
    size_t      bytesProcessed = 0;

    if (FALSE == memcpy_s( DeviceSpecificInfo, sizeof DeviceSpecificInfo, Context, sizeof DEVICE_SPECIFIC_INFO))
    { // Successful copy of data to the write structure
        // Update the 3 required fields...
        ((PDEVICE_SPECIFIC_INFO)DeviceSpecificInfo)->Signature = DEVICE_SPECIFIC_INFO_SIGNATURE;
        ((PDEVICE_SPECIFIC_INFO)DeviceSpecificInfo)->Version = DEVICE_SPECIFIC_INFO_STRUCT_VERSION;
        ((PDEVICE_SPECIFIC_INFO)DeviceSpecificInfo)->PayloadSize = PAYLOAD_SIZE;

         if( SUCCEEDED(hr = hFile->SetPos(offset)) &&
             SUCCEEDED(hr = hFile->Write((PCHAR)DeviceSpecificInfo, DEVICE_SPECIFIC_INFO_BUFFER_LENGTH, &bytesProcessed)) &&
             (sizeof(DeviceSpecificInfo) == bytesProcessed)
           )
        { // Successful SetPos and Write
            hr = S_OK;
        }

    }

    return hr;
}

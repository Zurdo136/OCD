/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    Device_Specific.h

Environment:
    User Mode

Author:
    Jose Pagan (jopagan)
--*/

#pragma once

#include "DEVICE_IO.h"

#pragma warning( push )

// DISABLE - nonstandard extension used: nameless struct/union
#pragma warning( disable: 4201 )

#define DEVICE_SPECIFIC_INFO_STRUCT_OLDEST_SIGNED_VERSION   1
#define DEVICE_SPECIFIC_INFO_SIGNATURE                      ((UINT64)0x666e497053766544)  // "DevSpInf"
#define DEVICE_SPECIFIC_INFO_STRUCT_VERSION                 1
#define BUGCHECK_CODE                                       (0x14c)

// The size of the buffer that is appended at the end of the rawdump
#define DEVICE_SPECIFIC_INFO_BUFFER_LENGTH                  1024

/******************************************************************************
** Template for the original data structure
**   Using unaligned structures allows for direct access to all the fields and
**     >>> ALERT: This structure is immutable. <<<
*******************************************************************************/
#define DEVICE_SPECIFIC_INFO_STRUCT_TEMPLATE \
        struct \
        { \
            ULONG Type; \
            ULONGLONG DumpHeaderInstanceID; \
            union \
            { \
                struct \
                { \
                    ULONGLONG APRegPA; \
                    struct \
                    { \
                        ULONGLONG VA; \
                        ULONGLONG PA; \
                        ULONG Size; \
                    }; \
                }; \
                struct \
                { \
                    ULONGLONG CpuContextAddress; \
                }; \
            }; \
            ULONG BugCheckCode; \
            ULONG BugCheckParam1; \
            ULONG BugCheckParam2; \
            ULONG BugCheckParam3; \
            ULONG BugCheckParam4; \
        }


// // // // // // // // // // // // // // // // // // // // // //
// Using Standard architecture types from ntexapi_h_x.w
//    PROCESSOR_ARCHITECTURE_INTEL,        =  0
//    PROCESSOR_ARCHITECTURE_ARM           =  5
//    PROCESSOR_ARCHITECTURE_ARM64         = 12
// // // // // // // // // // // // // // // //
// Need to keep this definition for
// backward compatibility
typedef enum
{
    DEVICE_TYPE_INTELx86 = 1,
    DEVICE_TYPE_QCOM32 = 2,
    DEVICE_TYPE_UNKOWN
} SOC_DEVICE_TYPE;
// // // // // // // // // // // // // // // // // // // // // //

/******************************************************************************
** Get data from packed vs unpacked data
*******************************************************************************/
typedef enum
{
    USE_PACK_UNDEFINED = 0,
    USE_LATEST,
    USE_UNSIGNED_PACKED,
    USE_UNSIGNED_UNPACKED
} PACK_FLAG;


#define DEVICE_SPECIFIC_INFO_SIGNED_DATA(base, var) \
            (((PDEVICE_SPECIFIC_INFO)&base)->var)

#define DEVICE_SPECIFIC_INFO_BASE_DATA(packFlag, base, var) \
    ( (USE_LATEST==packFlag) \
        ? (((PDEVICE_SPECIFIC_INFO)&base)->var) \
        : ( (USE_UNSIGNED_PACKED==packFlag) \
            ? ((PDEVICE_SPECIFIC_INFO_UNSIGNED_PACKED)&base)->var \
            : ( (USE_UNSIGNED_UNPACKED==packFlag) \
                ? ((PDEVICE_SPECIFIC_INFO_UNSIGNED_UNPACKED)&base)->var \
                : -1 \
              ) \
          ) \
    )

#define PAYLOAD_SIZE \
        ( sizeof DEVICE_SPECIFIC_INFO \
            - sizeof DEVICE_SPECIFIC_INFO::Signature \
            - sizeof DEVICE_SPECIFIC_INFO::Version \
            - sizeof DEVICE_SPECIFIC_INFO::PayloadSize \
        )


/******************************************************************************
** UNPACKED (old) data structure, from the template
*******************************************************************************/
typedef DEVICE_SPECIFIC_INFO_STRUCT_TEMPLATE
    DEVICE_SPECIFIC_INFO_UNSIGNED_UNPACKED, *PDEVICE_SPECIFIC_INFO_UNSIGNED_UNPACKED;


#pragma pack(1)
/******************************************************************************
** PACKED (old) data structure, from the template
*******************************************************************************/
typedef DEVICE_SPECIFIC_INFO_STRUCT_TEMPLATE
    DEVICE_SPECIFIC_INFO_UNSIGNED_PACKED, *PDEVICE_SPECIFIC_INFO_UNSIGNED_PACKED;


/******************************************************************************
** PACKED (new) data structure
** >>> New elements should be added at the bottom <<<
** The DEVICE_SPECIFIC_INFO_STRUCT_TEMPLATE structure is immutable and any
** changes to this strucutre should occur after version 1. To change this struct
** add fields below DEVICE_SPECIFIC_INFO_STRUCT_TEMPLATE_UPDATED and increment
** DEVICE_SPECIFIC_INFO_STRUCT_VERSION.  The end result will increase the
** PayloadSize by the fields added.
*******************************************************************************/
typedef struct _DEVICE_SPECIFIC_INFO
{
    // NOTE: <<< Do not change the first 4 fields >>>
    UINT64      Signature;
    UINT        Version; // = DEVICE_SPECIFIC_INFO_STRUCT_VERSION;
    UINT        PayloadSize; // reflects the size below this field (use macro above)
    DEVICE_SPECIFIC_INFO_STRUCT_TEMPLATE;
    // NOTE: additional fields can be added below this point
} DEVICE_SPECIFIC_INFO, *PDEVICE_SPECIFIC_INFO;
#pragma pack()

// Restore all warnings
#pragma warning( pop )

////////////////////////////////////////////////////////////////////////////////////////////////

HRESULT
ReadDeviceSpecificInfo(
    _In_    DEVICE_IO *hFile,
    _Out_writes_all_(sizeof DEVICE_SPECIFIC_INFO) PDEVICE_SPECIFIC_INFO Context,
    _In_    ULONGLONG offset
);


HRESULT
WriteDeviceSpecificInfo(
    _In_    DEVICE_IO *hFile,
    _In_reads_(sizeof DEVICE_SPECIFIC_INFO) PDEVICE_SPECIFIC_INFO Context,
    _In_    ULONGLONG offset
);

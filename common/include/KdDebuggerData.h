/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   KdDebuggerData.h

Environment:
   User Mode

Author:
   Jose Pagan (jopagan)
--*/

#pragma once
#include <bugcodes.h>

#define ONEFOURC_PARAM1_LEGACY_0                    0x0
#define ONEFOURC_PARAM1_LEGACY_1                    0x1
#define ONEFOURC_PARAM1_UNKNOWN                     (-1)
#define ONEFOURC_PARAM1_WATCHDOG                    0x2
#define ONEFOURC_PARAM1_FALLBACK                    0x3
#define ONEFOURC_PARAM1_THERMAL                     0x4
#define ONEFOURC_PARAM1_USER_INITIATED              0x5

#define ONEFOURC_PARAM2_NONSECURE_WDOG              0x1
#define ONEFOURC_PARAM2_SECURE_WDOG                 0x2
#define ONEFOURC_PARAM2_RPM_WDOG                    0x3

#define ONEFOURC_PARAM1_DEFAULT                     ONEFOURC_PARAM1_UNKNOWN
#define ONEFOURC_PARAM2_DEFAULT                     0x0
#define ONEFOURC_PARAM3_DEFAULT                     0x0
#define ONEFOURC_PARAM4_DEFAULT                     (-1)

#define DBG_BUGCHECK_SIZE                           (BUGCHECK_ARRAY_SIZE * sizeof UINT)

// Describes the bugcheck data structure, in the KiDebuggerDataBlock
typedef enum
{
    BUGCHECK_CODE_IDX = 0,
    BUGCHECK_PARAM1_IDX,
    BUGCHECK_PARAM2_IDX,
    BUGCHECK_PARAM3_IDX,
    BUGCHECK_PARAM4_IDX,
    BUGCHECK_ARRAY_SIZE
} KI_BUGCHECK_DATA;

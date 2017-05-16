/*++

Copyright (c) Microsoft Corporation. All rights reserved

Module Name:

wponefourc.h

Abstract:

This file defines the bugcheck 14c DIAG_BUFFER and 14c related defines.


Environment:

Kernel mode

--*/

#pragma once

#define ONEFOURC_PARAM1_LEGACY_0                    0x0
#define ONEFOURC_PARAM1_LEGACY_1                    0x1
#define ONEFOURC_PARAM1_RESERVED_104                0x10B
#define ONEFOURC_PARAM1_UNKNOWN                     ((UINT32)-1)
#define ONEFOURC_PARAM1_WATCHDOG                    0x2
#define ONEFOURC_PARAM1_FALLBACK                    0x3
#define ONEFOURC_PARAM1_THERMAL                     0x4
#define ONEFOURC_PARAM1_USER_INITIATED              0x5

#define ONEFOURC_PARAM2_NONSECURE_WDOG              0x1
#define ONEFOURC_PARAM2_SECURE_WDOG                 0x2
#define ONEFOURC_PARAM2_RPM_WDOG                    0x3

#define ONEFOURC_DIAG_BUFFER_SIGNATURE_LENGTH       0x10
#define ONEFOURC_DIAG_BUFFER_VERSION                0x1
#define ONEFOURC_DIAG_BUFFER_BUGCHECK_DATA_COUNT    0x5

#define ONEFOURC_DIAG_BUFFER_SIGNATURE              {'\\', '/', '\\', '/', 'P', 'C', 'l', '2', 'D', '|', '\\', '/', '|', 'P', 'G', 'o'}

typedef struct _ONEFOURC_DIAG_BUFFER
{
	UCHAR           Signature[ONEFOURC_DIAG_BUFFER_SIGNATURE_LENGTH];
	UINT32          Version;
	UINT32          PlatformID;
	UINT32          BugCheckData[ONEFOURC_DIAG_BUFFER_BUGCHECK_DATA_COUNT];
	PVOID           AdditionalData;
} ONEFOURC_DIAG_BUFFER, *PONEFOURC_DIAG_BUFFER;

#define QC_ADDITIONAL_DATA_VERSION                  0x10000
#define QC_ADDITIONAL_DATA_VERSION1_CORE_COUNT      0x4
#define QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH 0x4
#define QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH 0x8

typedef struct _QC_ADDITIONAL_DATA
{
	UINT32          Version;
	UINT32          ApRegScStatus[QC_ADDITIONAL_DATA_VERSION1_CORE_COUNT];
	UCHAR           RstStatBin[QC_ADDITIONAL_DATA_VERSION1_RST_STAT_LENGTH];
	UCHAR           PmicPonBin[QC_ADDITIONAL_DATA_VERSION1_PMIC_PON_LENGTH];
} QC_ADDITIONAL_DATA, *PQC_ADDITIONAL_DATA;

#define ONEFOURC_VERSION1_PARAM1_DEFAULT            ONEFOURC_PARAM1_UNKNOWN
#define ONEFOURC_VERSION1_PARAM2_DEFAULT            0x0
#define ONEFOURC_VERSION1_PARAM3_DEFAULT            0x0
#define ONEFOURC_VERSION1_PARAM4_DEFAULT            ((UINT32)-1)

//
// Struct for in memory diag buffer.
//
typedef struct _INMEM_DIAG_BUFFER
{
	ONEFOURC_DIAG_BUFFER    OneFourCBuffer;
	QC_ADDITIONAL_DATA      QCData;
}INMEM_DIAG_BUFFER, *PINMEM_DIAG_BUFFER;

/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
SvProcessing.h

Environment:
User Mode

Author:
José Pagán (jopagan)
--*/
#pragma once

#include "guiddefs.h"
#include "RawDumpDefs.h"

#define DEFAULT_SV_SECTION_AP_REG_SIZE      0x00001000
#define DEFAULT_SV_SECTION_OCIMEM_SIZE      0x00010000
#define DEFAULT_SV_SECTION_CODERAM_SIZE     0x00028000
#define DEFAULT_SV_SECTION_DATARAM_SIZE     0x00014000
#define DEFAULT_SV_SECTION_MSGRAM_SIZE      0x00004000
#define DEFAULT_SV_SECTION_LPM_SIZE         0x00010000
#define DEFAULT_SV_SECTION_PMIC_PON_SIZE    0x00000008
#define DEFAULT_SV_SECTION_RST_STAT_SIZE    0x00000004
#define DEFAULT_SV_SECTION_LOAD_SIZE        0x00000510
#define DEFAULT_SV_SECTION_RAWDUMP_SIZE     (512 * ONE_MEGABYTE)
#define DEFAULT_SV_SECTION_DDR_DAT_SIZE     0x00100000
#define DEFAULT_SV_SECTION_UNKNOWN_SIZE     0x00000000
#define DEFAULT_SV_SECTION_IPA_SRAM_SIZE    0x00003efc
#define DEFAULT_SV_SECTION_IPA_MBOX_SIZE    0x000000fc
#define DEFAULT_SV_SECTION_IPA_IRAM_SIZE    0x00002000
#define DEFAULT_SV_SECTION_IPA_DRAM_SIZE    0x00003efc


#define SV_SPECIFIC_TABLE_SIZE              GetSvTableSize()

// The table to map SV File (section) name to GUID
typedef struct _SV_SPECIFIC_GUID_TO_NAME
{
    GUID            Guid;
    UCHAR           Name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
    ULARGE_INTEGER  Size;   // the default size of these sections to use
} SV_SPECIFIC_GUID_TO_NAME, *PSV_SPECIFIC_GUID_TO_NAME;

/****************************************************************************************************
**  Internal Function Prototypes
*****************************************************************************************************/
UINT32 GetSvTableSize(void);

/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   SV_Specific.cpp

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/

#include "SV_Specific.h"

// SV Specific name to GUID mapping table
SV_SPECIFIC_GUID_TO_NAME SvSectionTable[] = {
    { SV_SECTION_AP_REG_GUID,   "AP_REG",       DEFAULT_SV_SECTION_AP_REG_SIZE },
    { SV_SECTION_OCIMEM_GUID,   "OCIMEM.BIN",   DEFAULT_SV_SECTION_OCIMEM_SIZE },
    { SV_SECTION_CODERAM_GUID,  "CODERAM.BIN",  DEFAULT_SV_SECTION_CODERAM_SIZE },
    { SV_SECTION_DATARAM_GUID,  "DATARAM.BIN",  DEFAULT_SV_SECTION_DATARAM_SIZE },
    { SV_SECTION_MSGRAM_GUID,   "MSGRAM.BIN",   DEFAULT_SV_SECTION_MSGRAM_SIZE },
    { SV_SECTION_LPM_GUID,      "LPM.BIN",      DEFAULT_SV_SECTION_LPM_SIZE },
    { NULL_GUID,                "IPA_SRAM.BIN", DEFAULT_SV_SECTION_IPA_SRAM_SIZE },
    { NULL_GUID,                "IPA_MBOX.BIN", DEFAULT_SV_SECTION_IPA_MBOX_SIZE },
    { NULL_GUID,                "IPA_IRAM.BIN", DEFAULT_SV_SECTION_IPA_IRAM_SIZE },
    { NULL_GUID,                "IPA_DRAM.BIN", DEFAULT_SV_SECTION_IPA_DRAM_SIZE },
    { SV_SECTION_PMIC_PON_GUID, "PMIC_PON.BIN", DEFAULT_SV_SECTION_PMIC_PON_SIZE },
    { SV_SECTION_RST_STAT_GUID, "RST_STAT.BIN", DEFAULT_SV_SECTION_RST_STAT_SIZE },
    { SV_SECTION_DDR_DAT_GUID,  "DDR_DATA.BIN", DEFAULT_SV_SECTION_DDR_DAT_SIZE },
    { SV_SECTION_LOAD_GUID,     "load.cmm",     DEFAULT_SV_SECTION_LOAD_SIZE },
    { SV_SECTION_RAWDUMP_GUID,  "rawdump.bin",  DEFAULT_SV_SECTION_RAWDUMP_SIZE },
    { SV_SECTION_UNKNOWN_GUID,  "UNKNOWN",      DEFAULT_SV_SECTION_UNKNOWN_SIZE }   //Must be last element.
};


/****************************************************************************************************
** UINT32 GetSvTableSize(void)
**
** Description:
**  Provide number of entries in SvSectionTable struct.
**
** Arguments:
**  NONE
**
** Return:
**  UINT32 - number of table entries
**
*****************************************************************************************************/
UINT32 GetSvTableSize(void)
{
    return (sizeof SvSectionTable / sizeof SvSectionTable[0]);
}

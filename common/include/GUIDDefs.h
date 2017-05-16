/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   GUIDDefs.h

Environment:
   User Mode

Author:
   Jose Pagan (jopagan)
--*/

#pragma once

#include <windows.h>
#include <winioctl.h>
#include <string>

// This header file (GUIDDEF.H) will create external references to the GUIDS below. It should be
// included if not already included, flagged by GUID_DEFINED. These GUIDS are allocated in the
// common library thus allowing the linker to resolve them correctly.
#ifndef GUID_DEFINED
#include <guiddef.h>
#endif

DEFINE_GUID(SVRAWDUMP_PARTITION_GUID,   0x66C9B323, 0xF7FC, 0x48B6, 0xBF, 0x96, 0x6F, 0x32, 0xE3, 0x35, 0xA4, 0x28);
DEFINE_GUID(CRASHDUMP_PARTITION_GUID,   0xebd0a0a2, 0xb9e5, 0x4433, 0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7);

DEFINE_GUID(NULL_GUID,                  0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0);

// UEFI variable GUID for the offline crash dump settings
DEFINE_GUID(DMP_SETTINGS_VENDOR_GUID,   0x853ce3a3, 0xf693, 0x4d9a, 0xb9, 0x5f, 0x84, 0xe2, 0x10, 0x5d, 0x13, 0xf2);

// SV Section GUID list
DEFINE_GUID(SV_SECTION_AP_REG_GUID,     0xAB3A051F, 0xEF0B, 0x4A5F, 0xA7, 0x9A, 0x80, 0xC2, 0x43, 0xBA, 0x08, 0x48);
DEFINE_GUID(SV_SECTION_OCIMEM_GUID,     0xD0A267A1, 0x9CA5, 0x471D, 0x8E, 0x9C, 0x79, 0xC9, 0x86, 0xBE, 0x77, 0x77);
DEFINE_GUID(SV_SECTION_CODERAM_GUID,    0x100B990B, 0x0F9B, 0x40B3, 0x82, 0xEF, 0x06, 0x61, 0x4F, 0x53, 0x05, 0xFE);
DEFINE_GUID(SV_SECTION_DATARAM_GUID,    0x82233308, 0xCE47, 0x4D52, 0x92, 0x11, 0xF4, 0x2E, 0x89, 0x61, 0x8A, 0xF4);
DEFINE_GUID(SV_SECTION_MSGRAM_GUID,     0x91A8C35C, 0xA340, 0x4F2E, 0xB7, 0x27, 0x65, 0x39, 0x47, 0xDB, 0x9C, 0x76);
DEFINE_GUID(SV_SECTION_LPM_GUID,        0x877F61E0, 0xA870, 0x4635, 0x9F, 0x41, 0x33, 0x00, 0x53, 0x20, 0x26, 0x05);
DEFINE_GUID(SV_SECTION_PMIC_PON_GUID,   0x10D25EDD, 0x1558, 0x4B88, 0xAB, 0x5C, 0xE8, 0x1E, 0x7F, 0x47, 0xDA, 0xD9);
DEFINE_GUID(SV_SECTION_RST_STAT_GUID,   0xD0352E48, 0xE359, 0x459E, 0x9B, 0xBF, 0x2E, 0x16, 0xE6, 0x28, 0xAC, 0xFB);
DEFINE_GUID(SV_SECTION_LOAD_GUID,       0x066A56C8, 0xCE2A, 0x4686, 0xB6, 0x10, 0x5B, 0xFC, 0x22, 0xD0, 0xC7, 0xAB);
DEFINE_GUID(SV_SECTION_RAWDUMP_GUID,    0x0df632e9, 0x5c48, 0x43aa, 0xb8, 0xbd, 0x5f, 0xf6, 0x18, 0x05, 0x02, 0x5f);
DEFINE_GUID(SV_SECTION_DDR_DAT_GUID,    0x62fb2678, 0x933f, 0x4177, 0x86, 0x29, 0xff, 0x3f, 0x70, 0x55, 0x02, 0xe3);
DEFINE_GUID(SV_SECTION_UNKNOWN_GUID,    0x6901D825, 0x0E25, 0x4D6C, 0x8C, 0x11, 0xE0, 0xAB, 0x2E, 0x98, 0xCA, 0xEF);

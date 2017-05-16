/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

Module Name:

WpCrDmpSettings.h

Abstract:

This file contains WpCrDmp settings file definitions.

Environment:

Kernel-mode Driver Framework

--*/

//
// Defining structs and enum required for generating preboot crash dump
// setting file.
//
// A settings file begins with a 16-byte file header that has following fields.
//
// Offset      Meaning
// ------      ----------------------------------------------------------------
// 00-03       The identifier of a settings file, must be 0x57, 0x50, 0x43, 0x42
// 04-07       Number of bytes taken by the data payload, excludes file header
// 08-0B       Number of records in the data payload
// 0C-0F       32-bit checksum of the data payload, excludes the file header
//
// Data payload of the file contains a series of records. Each record consists of
// header and single name-value pair.
//
// .--------------------------------------------------------.
// | RecordHeader | NameHeader | Name | ValueHeader | Value |
// .--------------------------------------------------------.
//
// Below is the detailed layout of a single record.
//
// .------------------------------.
// | Record.size                  |
// |------------------------------|
// | NameHdr.type                 |
// |------------------------------|
// | NameHdr.reserved             |
// |------------------------------|
// | NameHdr.size                 |
// |------------------------------|
// | NameHdr.data[NameHdr.size]   | <-- Contains 'Name'
// |------------------------------|
// | ValueHdr.type                |
// |------------------------------|
// | ValueHdr.reserved            |
// |------------------------------|
// | ValueHdr.size                |
// |------------------------------|
// | ValueHdr.data[ValueHdr.size] | <-- Contains 'Value'
// '------------------------------'
//

typedef enum
{
	SettingsTypeNone = 0,
	SettingsTypeSzA = 1,
	SettingsTypeSzW = 2,
	SettingsTypeUint8 = 3,
	SettingsTypeUint16 = 4,
	SettingsTypeUint32 = 5,
	SettingsTypeUint64 = 6,
}SETTINGS_FILE_DATA_TYPE;

#include <pshpack4.h>
typedef struct _SETTINGS_FILE_HEADER
{
	unsigned int Fileid : 32;
	unsigned int Datasize : 32;
	unsigned int Recordcount : 32;
	unsigned int Checksum : 32;
} SETTINGS_FILE_HEADER, *PSETTINGS_FILE_HEADER;
#include <poppack.h>

#include <pshpack4.h>
typedef struct _SETTINGS_RECORD
{
	unsigned int  Size : 16;
	unsigned int  Reserved : 16;
#pragma warning(push)
#pragma warning(disable:4200)
	unsigned char Payload[0];
#pragma warning(pop)
} SETTINGS_RECORD, *PSETTINGS_RECORD;
#include <poppack.h>

#include <pshpack4.h>
typedef struct _SETTINGS_HEADER
{
	unsigned int            Type : 8;
	unsigned int            Reserved : 8;
	unsigned int            Size : 16;
#pragma warning(push)
#pragma warning(disable:4200)
	unsigned char           Data[0];
#pragma warning(pop)
} SETTINGS_HEADER, *PSETTINGS_HEADER;
#include <poppack.h>

typedef struct _SETTINGS_CONTEXT
{
	unsigned char* pBuffer;
	unsigned int   BufferSize;
	unsigned char* pCursor;
	unsigned int   RecordCount;
	unsigned int   DataSize;
} SETTINGS_CONTEXT, *PSETTINGS_CONTEXT;

typedef struct _IN_MEM_DATA_INFO{
	PVOID               DataVA;
	PHYSICAL_ADDRESS    DataPA;
	UINT32              Size;
} IN_MEM_DATA_INFO, *PIN_MEM_DATA_INFO;

//
// Enum to describe online and offline dump state.
//
typedef enum {
	CRASHDUMP_STATE_DISABLED = 0,                  //Crash dump not enabled in registry. 
	CRASHDUMP_STATE_ENABLED_BUT_NOT_INITIALIZED,   //Crash dump enabled in registry but not intialized.
	CRASHDUMP_STATE_ENABLED_INITIALIZED            //Crash dump enabled in registry and is initialized.
}CRASHDUMP_STATE;

#define MAX_FILE_PATH       260

//
// Defining preboot crash dump settings identifier to be used in SETTINGS_FILE_HEADER
// struct
//
#define WPCR_SETTINGS_FILE_IDENTIFIER 'DCPW'

//
// This is an existing CrashControl registry key entry which specifies path to
// the designated crash dump file. This is also the UEFI variable name used 
// by the SBL dump code path for wpdmp.efi to determine the dump file path.
//
#define DEDICATED_DUMP_FILE_KEY_ENTRY L"DedicatedDumpFile"

//
// This is a new, WP specific registry key entry, which describes key
// associated with skipping preboot ram dump.
//
#define ABORT_KEY_NAME_KEY_ENTRY L"WpAbortKeyName"

//
// This is a new, WP specific registry key entry, which defines a scan code for
// a key associated with skipping preboot ram dump.
//
#define ABORT_KEY_SCAN_CODE_KEY_ENTRY L"WpAbortKeyScanCode"

//
// This is a new, WP specific registry key entry, which defines an amount of
// time in seconds that the user will have from skipping preboot ram dump.
//
#define ABORT_TIMEOUT_KEY_ENTRY L"WpAbortTimeout"

//
// This is a new, WP specific registry key entry, which defines an amount of
// time in seconds that the device will display an error before rebooting.
// This key entry is optional and may not exist in which case device will wait
// indefinitely.
//
#define ERROR_TIMEOUT_KEY_ENTRY_OPTIONAL L"WpErrorTimeout"

//
// This is a new, WP specific registry key entry, which points to a memory
// location where crash dump data is stored.
//
#define DUMP_DATA_ADDRESS_REG_ENTRY L"WpDumpDataAddr"

//
// This is a new, WP specific registry key entry, which points to a memory
// location where an in-memory copy of crash dump data is stored.
//
#define DUMP_DATA_COPY_ADDRESS_REG_ENTRY L"WpDumpDataAddrCopy"

//
// This is a new, WP specific registry key entry, which stores the offset of
// BugCheckParameter1 field within DUMP_HEADER structure.
//
#define DUMP_DATA_BUGCHECK_PARAM1_OFFSET L"WpDumpDataBugChkParam1Offset"

//
// Points to the physical memory location where nt!KdDebuggerDataBlock is stored.
//
#define DUMP_DATA_KD_DEBUGGER_DATA_BLOCK_ADDRESS        \
	L"WpDumpKdDebuggerDataBlockAddr"

//
// Points to the physical memory location where a copy nt!KdDebuggerDataBlock is
// stored.
//
#define DUMP_DATA_KD_DEBUGGER_DATA_BLOCK_COPY_ADDRESS   \
	L"WpDumpKdDebuggerDataBlockCopyAddr"

//
// UEFI variable name for the offline crash dump mode address
//
#define WPDMP_CAPTURE_MODE_FLAG_ADDRESS_VAR_NAME L"memory_capture_mode_address"

//
// UEFI variable name for the legacy SV flag address
//
#define WPDMP_SV_LEGACY_FLAG_ADDRESS_VAR_NAME L"legacy_flag_address"

//
// UEFI variable name for the legacy SV flag size in bytes
//
#define WPDMP_SV_LEGACY_FLAG_SIZE_VAR_NAME L"legacy_flag_size"

//
// UEFI variable name for the legacy SV flag set pattern
//
#define WPDMP_SV_LEGACY_FLAG_SET_PATTERN_VAR_NAME L"legacy_flag_set_pattern"

//
// UEFI variable name for the legacy SV flag reset pattern
//
#define WPDMP_SV_LEGACY_FLAG_RESET_PATTERN_VAR_NAME L"legacy_flag_reset_pattern"

//
// UEFI variable name for wpcrdmp.sys to indicate to wpdmp.efi whether wpdmp.efi
// should process the dump or not in the SBL dump path. This should be there
// for the UEFI dump path but not all Fluids have UEFI space to handle this unless 
// all the 8960 Fluids in the lab go through RPMB reprovision.
//
#define WPDMP_OFFLINE_DUMP_STATE L"OfflineDumpState"

//
// UEFI variable GUID for the offline crash dump mode address
//
DEFINE_GUID(WPDMP_CAPTURE_MODE_FLAG_ADDRESS_VENDOR_GUID,
	0x83e7a47a, 0x4023, 0x40d2, 0x98, 0x52, 0x05, 0xec, 0x34, 0xca, 0xaf, 0x87);

//
// UEFI variable name for the offline crash dump settings
//
#define WPDMP_SETTINGS_VAR_NAME L"wpdmp_settings"

//
// UEFI variable name in mem data.
//
#define WPDMP_IN_MEM_DATA_INFO L"InMemoryDataInfo"

//
// UEFI variable name for dump instance.
//
#define DUMP_INSTANCE L"DumpInstance"

//
// UEFI variable GUID for the offline crash dump settings
//
DEFINE_GUID(WPDMP_SETTINGS_VENDOR_GUID,
	0x853ce3a3, 0xf693, 0x4d9a, 0xb9, 0x5f, 0x84, 0xe2, 0x10, 0x5d, 0x13, 0xf2);

//
// Defining helper inline routine for calculating checksum
//

__inline
UINT32
SettingsCalcCheckSum(
_In_bytecount_(SizeInBytes)   PUCHAR    pData,
_In_                          ULONG     SizeInBytes
)
/*++

Routine Description:

This function calculates checksum.

Arguments:

pData              Data that needs checksum calculation.

SizeInBytes        Number of bytes that should be used in checksum
calculations.

Return Value:

Checksum.

Notes:

This private function can only be called from where the function arguments
are validated prior to calling this function.

--*/
{
	UINT32 Chksum = 0;

	while (SizeInBytes--)
	{
		Chksum += *pData;
		pData++;
	}

	return Chksum;
}

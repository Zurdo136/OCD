/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   FunctionalTests.h

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/

#pragma once

#include <stdio.h>
#include <DEVICE_IO.h>
#include <RawDumpDefs.h>
#include <Device_Specific.h>
#include <DisplayFuncs.h>

#define TEST_PATTERN_BEGIN      32       // <space>
#define TEST_PATTERN_END        126      // Last Ascii Char
#define TEST_PATTERN_SIZE       (TEST_PATTERN_END - TEST_PATTERN_BEGIN + 1)
#define OFFSET2VALUE(offset)    (CHAR)( ((offset) % TEST_PATTERN_SIZE) + TEST_PATTERN_BEGIN )
#define TEST_FILLER_SIZE        1024    // Size of Device Specific filler

// DEVICE_IO class tests
UINT Test_Unopened(DEVICE_IO *pIn, wstring devName, UINT devID );
UINT Test_Open_File(DEVICE_IO *pIn, wstring devName, UINT devID);
UINT Test_Open_Device(DEVICE_IO *pIn, wstring devName, UINT devID);
UINT Test_Open_Partition_File(DEVICE_IO *pIn, wstring devName, UINT devID);
UINT Test_Open_Partition_Device(DEVICE_IO *pIn, wstring devName, UINT devID);
UINT Test_Open_Partition_Position(DEVICE_IO *pIn, wstring devName, UINT devID);
UINT Test_Open_Partition_Position_Read_Headers(DEVICE_IO *pIn, wstring devName, UINT devID, ULONG bufSize);
UINT Test_Open_Partition_Position_Read_Chunks (DEVICE_IO *pIn, wstring devName, UINT devID, ULONG bufSize);
UINT Test_Open_Partition_Position_Read_Write_Chunk (DEVICE_IO *pIn, wstring devName, UINT devID, ULONG bufSize);

// Device Specific data structure tests
UINT Test_Device_Specific(DEVICE_IO *pIn, wstring devName, UINT devID);

// // // // // Helpers // // // // //
// DEVICE_IO class helpers
UINT ResultPartitionedDevice(DEVICE_IO *pIn, wstring devName, UINT devID);
BOOL ValidateBuffer(PCHAR buff, ULONG buffSize, ULONGLONG readOffset);
BOOL TEST_SetPos_Func(DEVICE_IO * pIn, ULONGLONG newPos);
BOOL TEST_Read_Func(DEVICE_IO * pIn, PCHAR buff, UINT buffSize);
BOOL TEST_Write_Func (DEVICE_IO * pIn, PCHAR buff, UINT buffSize);

// Device Specific data structure helpers
UINT Test_Write_Read_Device_Specific(DEVICE_IO *pIn);

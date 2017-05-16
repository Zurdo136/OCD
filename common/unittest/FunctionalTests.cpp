/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   FunctionalTests.cpp

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/

#include <FunctionalTests.h>

//  UINT        TestUnopened(DEVICE_IO *pIn, wstring devName, UINT devID )
UINT Test_Unopened(DEVICE_IO *pIn, wstring devName, UINT devID )
{
    UINT failCount = 0;
    HRESULT ret;
    ULONGLONG curPos;

    // Correct error code should be IO_ERROR_INVALID_HANDLE
    if (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE)
    {
        printf("\t\t     Error code: PASSED\r\n");
    }
    else
    {
        printf("\t\t     Error code: FAILED\r\n");
        failCount++;
    }

    // Device Name/ID:
    if( devID == INVALID_DEVICE_ID)
    {
        if( devName.empty() )
        {
            if (pIn->GetDeviceName().empty())
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
        else
        {
            if (devName.compare(pIn->GetDeviceName()) == 0)
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
    }
    else
    {
        if (pIn->GetDeviceName().empty())
        {
            printf("\t\t      Device ID: PASSED\r\n");
        }
        else
        {
            printf("\t\t       Device ID: FAILED (ID: %d) (Expected Name: \"empty string\") (Actual Name: %ls)\r\n", devID, pIn->GetDeviceName().c_str());
            failCount++;
        }

    }

    // Device Type: Type should be UNINITIALIZED_DEVICE_TYPE
    if (pIn->GetDeviceType() == DEVICE_IO::UNINITIALIZED_DEVICE_TYPE)
    {
        printf("\t\t    Device Type: PASSED\r\n");
    }
    else
    {
        failCount++;
        switch (pIn->GetDeviceType())
        {
            case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
                printf("\t\t    Device Type: FAILED (PLAIN_FILE_DEVICE_TYPE)\r\n");
                break;
            case DEVICE_IO::RAW_DEVICE_TYPE:
                printf("\t\t    Device Type: FAILED (RAW_DEVICE_TYPE)\r\n");
                break;
            case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
                printf("\t\t    Device Type: FAILED (UNSUPPORTED_DEVICE_TYPE)\r\n");
                break;
            case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
                printf("\t\t    Device Type: FAILED (REMOVABLE_MEDIA_DEVICE_TYPE)\r\n");
                break;
        }
    }

    // Device Size: if no device initialized, size = 0
    if (pIn->GetCurrentFileSize() == 0)
    {
        printf("\t\t    Device Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t    Device Size: FAILED (%ld)\r\n", pIn->GetCurrentFileSize());
        failCount++;
    }


    // Device partition type: should be PARTITION_STYLE_RAW
    if (pIn->GetDevicePartitionType() == PARTITION_STYLE_RAW)
    {
        printf("\t\t  Dev Part Type: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Dev Part Type: FAILED\r\n");
        failCount++;
    }

    // Device Partition Count: should be zero
    if (pIn->GetPartitionCount() == 0)
    {
        printf("\t\t Dev Part Count: PASSED\r\n");
    }
    else
    {
        printf("\t\t Dev Part Count: FAILED (Count: %ld)\r\n", pIn->GetPartitionCount());
        failCount++;
    }

    // Device pos: should fail with returned position of zero and error = IO_ERROR_INVALID_HANDLE
    ret = pIn->GetIoPos(&curPos);
    if (FAILED(ret) && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        if (curPos == 0)
        {
            printf("\t\t     Device Pos: PASSED\r\n");
        }
        else
        {
            printf("\t\t     Device Pos: FAILED (Position: %d)\r\n", curPos);
            failCount++;
        }

    }
    else
    {
        printf("\t\t     Device Pos: FAILED (Position: %d) (GetError: %#x)\r\n", curPos, pIn->GetError());
        failCount++;
    }

    // // // // // // // // // // // // // // // // // // // // // // // // // // // // //

    // Partition Name: if no selected partition, no name {empty()}
    if (pIn->GetCurrentPartitionName().compare(L"unsupported") == 0)
    {
        printf("\t\t  Cur Part Name: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Name: FAILED (Name:%ls)\r\n", pIn->GetCurrentPartitionName().c_str());
        failCount++;
    }

    // Partition Type: With no current (selected) partition, PARTITION_STYLE_RAW and IO_ERROR_INVALID_METHOD_USED
    if ((pIn->GetCurrentPartitionType() == PARTITION_STYLE_RAW)
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_METHOD_USED))
    {
        printf("\t\t Cur Part Style: PASSED (PARTITION_STYLE_RAW)\r\n");
    }
    else
    {
        switch (pIn->GetCurrentPartitionType())
        {
        case PARTITION_STYLE_MBR:
            printf("\t\t Cur Part Style: FAILED (PARTITION_STYLE_MBR)\r\n");
            break;
        case PARTITION_STYLE_GPT:
            printf("\t\t Cur Part Style: FAILED (PARTITION_STYLE_GPT)\r\n");
            break;
        default:
            printf("\t\t Cur Part Style: FAILED (undefined/invalid)\r\n");
            break;
        }
        failCount++;
    }

    // Partition Size: if no selected partition, size = 0
    if (pIn->GetCurrentPartitionSize() == 0)
    {
        printf("\t\t  Cur Part Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Size: FAILED\r\n");
        failCount++;
    }

    // Partition Index: if no selected partition, no index {INVALID_INDEX}
    if (pIn->GetCurrentPartitionNumber() == INVALID_INDEX)
    {
        printf("\t\t Cur Part Index: PASSED\r\n");
    }
    else
    {
        printf("\t\t Cur Part Index: FAILED\r\n");
        failCount++;
    }

    // if no selected partition, no GUID {empty()}
    if (pIn->GetCurrentPartitionGUID().compare(L"unsupported") == 0)
    {
        printf("\t\t  Cur Part GUID: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part GUID: FAILED (GUID: %s)\r\n", pIn->GetCurrentPartitionGUID());
        failCount++;
    }

    // Partition Pos: should fail with returned position of zero and error = IO_ERROR_UNSUPPORTED_DEVICE_TYPE
    ret = pIn->GetPos(&curPos);
    if (FAILED(ret) && (pIn->GetError() == DEVICE_IO::IO_ERROR_UNSUPPORTED_DEVICE_TYPE) )
    {
        if (curPos == 0)
        {
            printf("\t\t   Cur Part Pos: PASSED\r\n");
        }
        else
        {
            printf("\t\t   Cur Part Pos: FAILED (Position: %d)\r\n", curPos);
            failCount++;
        }

    }
    else
    {
        printf("\t\t         GetPos: FAILED (Position: %d) (GetError: %#x)\r\n", curPos, pIn->GetError());
        failCount++;
    }

    return failCount;
}

//  UINT        TestOpenFile(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Open_File(DEVICE_IO *pIn, wstring devName, UINT devID)
{
    UINT failCount = 0;
    HRESULT ret;
    ULONGLONG curPos;

    // FAIL - this is an unopened file (also, a file has no partitions)
    if (FAILED(pIn->SetPartition((UINT)0))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - (UINT)(0)\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - (UINT)(0)\r\n", pIn->GetError());
        failCount++;
    }

    // FAIL - this is an unopened file (also, a file has no partitions)
    if (FAILED(pIn->SetPartition(DEVICE_IO::SVRAWDUMP))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - DEVICE_IO::SVRAWDUMP\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - DEVICE_IO::SVRAWDUMP\r\n", pIn->GetError());
        failCount++;
    }

    // FAIL - this is an unopened file (also, a file has no partitions)
    if (FAILED(pIn->SetPartition("ReallyLongPartitonNameForTestingHere"))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - long partition name\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - long partition name\r\n", pIn->GetError());
        failCount++;
    }

    if (SUCCEEDED(pIn->Open()))
    {
        printf("\t\t         Open(): PASSED\r\n");
    }
    else
    {
        printf("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    DisplayResults01(pIn);

    // Device Name/ID:
    if (devID == INVALID_DEVICE_ID)
    {
        if (devName.empty())
        {
            if (pIn->GetDeviceName().empty())
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
        else
        {
            if (devName.compare(pIn->GetDeviceName()) == 0)
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
    }
    else
    {
        if (pIn->GetDeviceName().empty())
        {
            printf("\t\t      Device ID: PASSED\r\n");
        }
        else
        {
            printf("\t\t      Device ID: FAILED (ID: %d) (Expected Name: \"empty string\") (Actual Name: %ls)\r\n", devID, pIn->GetDeviceName().c_str());
            failCount++;
        }

    }

    // Device Type: should be PLAIN_FILE_DEVICE_TYPE
    if (pIn->GetDeviceType() == DEVICE_IO::PLAIN_FILE_DEVICE_TYPE)
    {
        printf("\t\t    Device Type: PASSED\r\n");
    }
    else
    {
        failCount++;
        switch (pIn->GetDeviceType())
        {
        case DEVICE_IO::RAW_DEVICE_TYPE:
            printf("\t\t    Device Type: FAILED (RAW_DEVICE_TYPE)\r\n");
            break;
        case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
            printf("\t\t    Device Type: FAILED (UNINITIALIZED_DEVICE_TYPE)\r\n");
            break;
        case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
            printf("\t\t    Device Type: FAILED (UNSUPPORTED_DEVICE_TYPE)\r\n");
            break;
        case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
            printf("\t\t    Device Type: FAILED (REMOVABLE_MEDIA_DEVICE_TYPE)\r\n");
            break;
        }
    }

    // Device Size: if device is initialized, size > 0
    if (pIn->GetCurrentFileSize() != 0)
    {
        printf("\t\t    Device Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t    Device Size: FAILED (%ld)\r\n", pIn->GetCurrentFileSize());
        failCount++;
    }

    // Device partition type: should be PARTITION_STYLE_RAW
    if (pIn->GetDevicePartitionType() == PARTITION_STYLE_RAW)
    {
        printf("\t\t  Dev Part Type: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Dev Part Type: FAILED\r\n");
        failCount++;
    }

    // Partition count: should be zero
    if (pIn->GetPartitionCount() == 0 && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_METHOD_USED))
    {
        printf("\t\t Dev Part Count: PASSED\r\n");
    }
    else
    {
        printf("\t\t Dev Part Count: FAILED (Count: %ld)\r\n", pIn->GetPartitionCount());
        failCount++;
    }

    // Device Pos: should not fail
    ret = pIn->GetIoPos(&curPos);
    if (SUCCEEDED(ret))
    {
        printf("\t\t     Device Pos: PASSED\r\n");
    }
    else
    {
        printf("\t\t     Device Pos: FAILED (Position: %d) (GetError: %#x)\r\n", curPos, pIn->GetError());
        failCount++;
    }

    // // // // // // // // // // // // // // // // // // // // // // // // // // // // //

    // if no selected partition, no name {empty()}
    if (pIn->GetCurrentPartitionName().compare(L"unsupported") == 0)
    {
        printf("\t\t  Cur Part Name: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Name: FAILED (Name: %ls)\r\n", pIn->GetCurrentPartitionName().c_str());
        failCount++;
    }

    // With no current (selected) partition, PARTITION_STYLE_RAW and IO_ERROR_INVALID_METHOD_USED
    if ((pIn->GetCurrentPartitionType() == PARTITION_STYLE_RAW)
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_METHOD_USED))
    {
        printf("\t\t Cur Part Style: PASSED (PARTITION_STYLE_RAW)\r\n");
    }
    else
    {
        switch (pIn->GetCurrentPartitionType())
        {
        case PARTITION_STYLE_MBR:
            printf("\t\t Cur Part Style: FAILED (PARTITION_STYLE_MBR)\r\n");
            break;
        case PARTITION_STYLE_GPT:
            printf("\t\t Cur Part Style: FAILED (PARTITION_STYLE_GPT)\r\n");
            break;
        default:
            printf("\t\t Cur Part Style: FAILED (undefined/invalid)\r\n");
            break;
        }
        failCount++;
    }

    // if no selected partition, size = 0
    if ((pIn->GetCurrentPartitionSize() == 0) && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_METHOD_USED))
    {
        printf("\t\t  Cur Part Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Size: FAILED\r\n");
        failCount++;
    }

    // if no selected partition, no index {INVALID_INDEX}
    if (pIn->GetCurrentPartitionNumber() == INVALID_INDEX)
    {
        printf("\t\t Cur Part Index: PASSED\r\n");
    }
    else
    {
        printf("\t\t Cur Part Index: FAILED\r\n");
        failCount++;
    }

    // if no selected partition, no GUID {empty()}
    if (pIn->GetCurrentPartitionGUID().compare(L"unsupported") == 0)
    {
        printf("\t\t  Cur Part GUID: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part GUID: FAILED (GUID: %s)\r\n", pIn->GetCurrentPartitionGUID());
        failCount++;
    }

    // Partition pos: should not fail
    ret = pIn->GetPos(&curPos);
    if (SUCCEEDED(ret))
    {
        printf("\t\t         GetPos: PASSED\r\n");
    }
    else
    {
        printf("\t\t         GetPos: FAILED (Position: %d) (GetError: %#x)\r\n", curPos, pIn->GetError());
        failCount++;
    }


    return failCount;
}

//  UINT        TestOpenDevice(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Open_Device(DEVICE_IO *pIn, wstring devName, UINT devID)
{
    UINT failCount = 0;
    HRESULT ret;
    ULONGLONG curPos;

    // This call should fail since the device is unopened
    if (FAILED(pIn->SetPartition((UINT)0))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - (UINT)(0)\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - (UINT)(0)\r\n", pIn->GetError());
        failCount++;
    }

    // This call should fail since the device is unopened
    if (FAILED(pIn->SetPartition(DEVICE_IO::SVRAWDUMP))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - DEVICE_IO::SVRAWDUMP\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - DEVICE_IO::SVRAWDUMP\r\n", pIn->GetError());
        failCount++;
    }

    // This call should fail since the device is unopened
    if (FAILED(pIn->SetPartition("ReallyLongPartitonNameForTestingHere"))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - long partition name\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - long partition name\r\n", pIn->GetError());
        failCount++;
    }

    if (SUCCEEDED(pIn->Open()))
    {
        printf("\t\t         Open(): PASSED\r\n");
    }
    else
    {
        printf("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    DisplayResults01(pIn);

    if (devID == INVALID_DEVICE_ID)
    {
        if (devName.empty())
        {
            if (pIn->GetDeviceName().empty())
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
        else
        {
            if (devName.compare(pIn->GetDeviceName()) == 0)
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
    }
    else
    {
        if (!pIn->GetDeviceName().empty())
        {
            wstring tmpName = PHYSICAL_DEVICE_STRING;
            WCHAR val[MAX_DEV_ID_STRING_SIZE] = { 0 };

            _itow(devID, val, 10);
            tmpName.append(val);

            if (0 == _wcsicmp(pIn->GetDeviceName().c_str(), tmpName.c_str()))
            {
                printf("\t\t      Device ID: PASSED\r\n");
            }
            else
            {
                printf("\t\t      Device ID: FAILED (ID: %d) (Expected Name: %ls) (Actual Name: %ls)\r\n", devID, tmpName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }

        }
        else
        {
            printf("\t\t      Device ID: FAILED (ID: %d) (Expected Name: \"empty string\") (Actual Name: %ls)\r\n", devID, pIn->GetDeviceName().c_str());
            failCount++;
        }

    }

    // Device Type:
       if (pIn->GetDeviceType() == DEVICE_IO::RAW_DEVICE_TYPE)
    { // Device type could be RAW_DEVICE_TYPE
        printf("\t\t    Device Type: PASSED (RAW_DEVICE_TYPE)\r\n");
    }
    else if( pIn->GetDeviceType() == DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE)
    { // Device type could be REMOVABLE_MEDIA_DEVICE_TYPE
        printf("\t\t    Device Type: PASSED (REMOVABLE_MEDIA_DEVICE_TYPE)\r\n");
    }
    else
    {
        failCount++;
        switch (pIn->GetDeviceType())
        {
        case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
            printf("\t\t    Device Type: FAILED (PLAIN_FILE_DEVICE_TYPE)\r\n");
            break;
        case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
            printf("\t\t    Device Type: FAILED (UNINITIALIZED_DEVICE_TYPE)\r\n");
            break;
        case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
            printf("\t\t    Device Type: FAILED (UNSUPPORTED_DEVICE_TYPE)\r\n");
            break;
        default:
            printf("\t\t    Device Type: Unknown\r\n");
            break;
        }
    }


    // Device Size: if no device initialized, size = 0
    if (pIn->GetCurrentFileSize() != 0)
    {
        printf("\t\t    Device Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t    Device Size: FAILED (%ld)\r\n", pIn->GetCurrentFileSize());
        failCount++;
    }

    // Device partition type: could be PARTITION_TYPE_GPT or PARTITION_TYPE_MBR
    switch( pIn->GetDevicePartitionType() )
    {
        case PARTITION_STYLE_GPT:
            printf("\t\t  Dev Part Type: PASSED (GPT)\r\n");
            break;
        case PARTITION_STYLE_MBR:
            printf("\t\t  Dev Part Type: PASSED (MBR)\r\n");
            break;
        default:
            printf("\t\t  Dev Part Type: FAILED\r\n");
            failCount++;
            break;
    }

    // Device partition count: should not be zero
    if ((pIn->GetPartitionCount() != 0) && (pIn->GetError() == DEVICE_IO::IO_OK))
    {
        printf("\t\t Dev Part Count: PASSED (Count: %ld)\r\n", pIn->GetPartitionCount());
    }
    else
    {
        printf("\t\t Dev Part Count: FAILED (Count: %ld)\r\n", pIn->GetPartitionCount());
        failCount++;
    }

    // Device pos: should not fail
    ret = pIn->GetIoPos(&curPos);
    if (SUCCEEDED(ret))
    {
        printf("\t\t     Device Pos: PASSED\r\n");
    }
    else
    {
        printf("\t\t     Device Pos: FAILED (Error: %#x) (Position: %d)\r\n", pIn->GetError(), curPos);
        failCount++;
    }

    // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // //

    // Partition: name: if no selected partition, no name {empty()}
    if (pIn->GetCurrentPartitionName().compare(L"unselected") == 0)
    {
        printf("\t\t  Cur Part Name: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Name: FAILED (Name: %ls)\r\n", pIn->GetCurrentPartitionName().c_str());
        failCount++;
    }

    // Partition Type: With no current (selected) partition, PARTITION_TYPE_RAW and IO_ERROR_PARTITION_NOT_SET
    if ((pIn->GetCurrentPartitionType() == PARTITION_STYLE_RAW)
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_PARTITION_NOT_SET))
    {
        printf("\t\t Cur Part Style: PASSED (Partition not set)\r\n");
    }
    else
    {
        switch (pIn->GetCurrentPartitionType())
        {
        case PARTITION_STYLE_MBR:
            printf("\t\t Cur Part Style: FAILED (PARTITION_STYLE_MBR)\r\n");
            break;
        case PARTITION_STYLE_GPT:
            printf("\t\t Cur Part Style: FAILED (PARTITION_STYLE_GPT)\r\n");
            break;
        default:
            printf("\t\t Cur Part Style: FAILED (undefined/invalid)\r\n");
            break;
        }
        failCount++;
    }

    // Partition size: if size == 0 then no partition selected should be the error code
    if ((pIn->GetCurrentPartitionSize() == 0) && (pIn->GetError() == DEVICE_IO::IO_ERROR_PARTITION_NOT_SET))
    {
        printf("\t\t  Cur Part Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Size: FAILED\r\n");
        failCount++;
    }

    // Partition index: if no selected partition, no index {INVALID_INDEX}
    if (pIn->GetCurrentPartitionNumber() == INVALID_INDEX)
    {
        printf("\t\t Cur Part Index: PASSED\r\n");
    }
    else
    {
        printf("\t\t Cur Part Index: FAILED\r\n");
        failCount++;
    }

    // Partition GUID: if no selected partition, no GUID {empty()}
    if (pIn->GetCurrentPartitionGUID().compare(L"{00000000-0000-0000-0000-000000}") == 0)
    {
        printf("\t\t  Cur Part GUID: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part GUID: FAILED (GUID: %s)\r\n", pIn->GetCurrentPartitionGUID());
        failCount++;
    }

    // Partition Pos: should fail with returned position of zero and error = IO_ERROR_PARTITION_NOT_SET
    ret = pIn->GetPos(&curPos);
    if (FAILED(ret) && (pIn->GetError() == DEVICE_IO::IO_ERROR_PARTITION_NOT_SET))
    {
        printf("\t\t       GetPos(): PASSED\r\n");
    }
    else
    {
        printf("\t\t       GetPos(): FAILED (Error: %#x) %#x\r\n", pIn->GetError(), curPos);
        failCount++;
    }

    return failCount;
}

//  UINT        TestOpenPartitionFile(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Open_Partition_File(DEVICE_IO *pIn, wstring devName, UINT devID)
{
    UINT failCount = 0;

    // This call should fail since a file has no partitions
    if (FAILED(pIn->SetPartition((UINT)0))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - (UINT)(0)\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - (UINT)(0)\r\n", pIn->GetError());
        failCount++;
    }

    // This call should fail since a file has no partitions
    if (FAILED(pIn->SetPartition(DEVICE_IO::SVRAWDUMP))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - DEVICE_IO::SVRAWDUMP\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - DEVICE_IO::SVRAWDUMP\r\n", pIn->GetError());
        failCount++;
    }

    // This call should fail since a file has no partitions
    if (FAILED(pIn->SetPartition("ReallyLongPartitonNameForTestingHere"))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - long partition name\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - long partition name\r\n", pIn->GetError());
        failCount++;
    }

    if (SUCCEEDED(pIn->Open()))
    {
        printf("\t\t         Open(): PASSED\r\n");
    }
    else
    {
        printf("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    DisplayResults01(pIn);
    // This Test should fail since a file has no partitions
    if (FAILED(pIn->SetPartition("LongPartitionNameBeingSetShouldFailThisTest"))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_METHOD_USED))
    {
        printf("\t\t SetPartition(): PASSED - long partition name\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - long partition name\r\n", pIn->GetError());
        failCount++;
    }

    // This Test should fail since a file has no partitions
    if (FAILED(pIn->SetPartition(DEVICE_IO::SVRAWDUMP))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_METHOD_USED))
    {
        printf("\t\t SetPartition(): PASSED\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    if (SUCCEEDED(pIn->Close()))
    {
        printf("\t\t        Close(): PASSED\r\n");
    }
    else
    {
        printf("\t\t        Close(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }
    failCount += Test_Open_File(pIn, devName, devID);

    return failCount;
}

//  UINT        TestOpenPartitionDevice(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Open_Partition_Device(DEVICE_IO *pIn, wstring devName, UINT devID)
{
    UINT failCount = 0;

    // This call should fail since the device is unopened
    if (FAILED(pIn->SetPartition((UINT)0))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - (UINT)(0)\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - (UINT)(0)\r\n", pIn->GetError());
        failCount++;
    }

    // This call should fail since the device is unopened
    if (FAILED(pIn->SetPartition(DEVICE_IO::SVRAWDUMP))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - DEVICE_IO::SVRAWDUMP\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - DEVICE_IO::SVRAWDUMP\r\n", pIn->GetError());
        failCount++;
    }

    // This call should fail since the device is unopened
    if (FAILED(pIn->SetPartition("ReallyLongPartitonNameForTestingHere"))
        && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE))
    {
        printf("\t\t SetPartition(): PASSED - long partition name\r\n");
    }
    else
    {
        printf("\t\t SetPartition(): FAILED (Error: %#x) - long partition name\r\n", pIn->GetError());
        failCount++;
    }

    if (SUCCEEDED(pIn->Open()))
    {
        printf("\t\t         Open(): PASSED\r\n");
    }
    else
    {
        printf("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    // This Test should fail because the partiton name is too long - test for RAW Devices Only
    if (pIn->GetDeviceType() == DEVICE_IO::RAW_DEVICE_TYPE)
    {
        if (FAILED(pIn->SetPartition("LongPartitionNameBeingSetShouldFailThisTest"))
            && (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_PARTITION_NAME))
        {
            printf("\t\t SetPartition(): PASSED - long partition name\r\n");
        }
        else
        {
            printf("\t\t SetPartition(): FAILED (Error: %#x) - long parition name\r\n", pIn->GetError());
            failCount++;
        }
    }

    if ((pIn->GetDeviceType() == DEVICE_IO::RAW_DEVICE_TYPE) && SUCCEEDED(pIn->SetPartition(DEVICE_IO::SVRAWDUMP)))
    {
        printf("\t\t SetPartition(): PASSED\r\n");
    }
    else if ((pIn->GetDeviceType() == DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE) && SUCCEEDED(pIn->SetPartition((UINT)0)))
    {
        printf("\t\t SetPartition(): PASSED\r\n");
    }
    else
    {
        printf("\t\t 2 - SetPartition(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }



    DisplayResults01(pIn);
    failCount += ResultPartitionedDevice(pIn, devName, devID);

    return failCount;
}

//  UINT        Test_Open_Partition_Position_File(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Open_Partition_Position(DEVICE_IO *pIn, wstring devName, UINT devID)
{
    UINT failCount = 0;
    ULONGLONG  size = 0;

    if (SUCCEEDED(pIn->Open()))
    {
        switch (pIn->GetDeviceType())
        {
        case DEVICE_IO::RAW_DEVICE_TYPE:
        case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
            pIn->Close();
            failCount = Test_Open_Partition_Device(pIn, devName, devID);
            size = pIn->GetCurrentPartitionSize();
            break;

        case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
            pIn->Close();
            failCount = Test_Open_Partition_File(pIn, devName, devID);
            size = pIn->GetCurrentFileSize();
            break;

        case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
        case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
            pIn->Close();
            failCount++;
            break;
        }
    }
    else
    {
        printf("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }
    if (TEST_SetPos_Func(pIn, 100) == FALSE)
    {
        failCount++;
    }

    if (TEST_SetPos_Func(pIn, 1225) == FALSE)
    {
        failCount++;
    }

    if (TEST_SetPos_Func (pIn, 6000) == FALSE)
    {
        failCount++;
    }

    if (TEST_SetPos_Func (pIn, 0x323) == FALSE)
    {
        failCount++;
    }

    // Test boundary condition on MMOS, blockSize - 1
    if (TEST_SetPos_Func (pIn, (pIn->GetBlockSize()-1) ) == FALSE)
    {
        failCount++;
    }

    // Test boundary condition on MMOS, blockSize
    if (TEST_SetPos_Func (pIn, pIn->GetBlockSize()) == FALSE)
    {
        failCount++;
    }

    // Test boundary condition on MMOS, blockSize + 1
    if (TEST_SetPos_Func (pIn, (pIn->GetBlockSize () + 1)) == FALSE)
    {
        failCount++;
    }

    // Test condition where cache contains the current block
    if (TEST_SetPos_Func (pIn, (2 * (pIn->GetBlockSize () + 1))) == FALSE)
    {
        failCount++;
    }

    if (TEST_SetPos_Func(pIn, 0) == FALSE)
    {
        failCount++;
    }

    if (TEST_SetPos_Func(pIn, 1395) == FALSE)
    {
        failCount++;
    }

    if (TEST_SetPos_Func(pIn, 0x48) == FALSE)
    {
        failCount++;
    }

    // Test Setting to end of file or partition
    if (TEST_SetPos_Func (pIn, size) == FALSE)
    {
        failCount++;
    }

    // Test Setting to end of file lese some bytes
    if (TEST_SetPos_Func (pIn, (size - 25)) == FALSE)
    {
        failCount++;
    }

    // Test Setting to end of file lese some bytes
    if (TEST_SetPos_Func (pIn, (size - 325)) == FALSE)
    {
        failCount++;
    }

    if (TEST_SetPos_Func(pIn, 1225) == FALSE)
    {
        failCount++;
    }

    // Test Setting past end of file or partition
    if (TEST_SetPos_Func(pIn, size + 100) == FALSE)
    {
        failCount++;
    }

    return failCount;
}

//  UINT        Test_Open_Partition_Position_Read(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Open_Partition_Position_Read_Headers(DEVICE_IO *pIn, wstring devName, UINT devID, ULONG bufSize)
{
    UNREFERENCED_PARAMETER (bufSize);

    RAW_DUMP_HEADER dumpHeader = { 0 };
    size_t          bytesRead = 0;
    UINT            failCount = 0;
    ULONGLONG       size = 0;

    if (SUCCEEDED (pIn->Open ()))
    {
        switch (pIn->GetDeviceType ())
        {
            case DEVICE_IO::RAW_DEVICE_TYPE:
            case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
                pIn->Close ();
                failCount = Test_Open_Partition_Device (pIn, devName, devID);
                size = pIn->GetCurrentPartitionSize ();
                break;

            case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
                pIn->Close ();
                failCount = Test_Open_Partition_File (pIn, devName, devID);
                size = pIn->GetCurrentFileSize ();
                break;

            case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
            case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
                pIn->Close ();
                failCount++;
                break;
        }
    }
    else
    {
        printf ("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError ());
        failCount++;
    }

    if (FALSE == TEST_SetPos_Func (pIn, 16245))
    { // move the offset away from the first block
        failCount++;
    }

    if (FALSE == TEST_SetPos_Func (pIn, 0x215))
    { // Should return and find the first block and the known structure there.
        failCount++;
    }

    if (FALSE == TEST_SetPos_Func (pIn, 0))
    { // Should return and find the first block and the known structure there.
        failCount++;
    }

    if (SUCCEEDED (pIn->Read ((PCHAR)&dumpHeader, sizeof dumpHeader, &bytesRead)) && (sizeof dumpHeader == bytesRead))
    { // Choose to verify the header and sections here
        RAW_DUMP_SECTION_HEADER         sectionHeader = { 0 };
        UINT64                          dataOffset = 0;
        UINT64                          headerOffset = bytesRead;
        UINT64                          sizeAccumulator = sizeof dumpHeader;
        UINT64                          sizeOfHeaders = sizeof dumpHeader + (dumpHeader.SectionsCount * sizeof sectionHeader);
        ULONGLONG                       myPos = 0;

        printf ("\t\t         Read(): PASSED - read dumpHeader \r\n");
        for (UINT i = 0; i < dumpHeader.SectionsCount; i++)
        {
            pIn->SetPos (0);
            pIn->SetPos (headerOffset);
            if (SUCCEEDED (pIn->Read ((PCHAR)&sectionHeader, sizeof sectionHeader, &bytesRead)) && (sizeof sectionHeader == bytesRead))
            {
                printf ("\t\t         Read(): PASSED - read sectionHeader 0x%02x \r\n", i);
                sizeAccumulator += bytesRead + sectionHeader.Size;
                headerOffset += bytesRead;
                if (i == 0)
                { // Capture the offset of the first entry, should be the place where the secsions leave off
                    dataOffset = sectionHeader.Offset;
                }
            }
            else
            { // Should return and find the first block and the known structure there.
                if (sizeof sectionHeader == bytesRead)
                {
                    printf ("\t\t         Read(): FAILED - read sectionHeader 0x%02x (Error: %#x)\r\n", i, pIn->GetError ());
                }
                else
                {
                    printf ("\t\t         Read(): FAILED - read sectionHeader 0x%02x (Error: %#x) (Expected: %#x) (Actual: %#x)\r\n", i, pIn->GetError (), sizeof sectionHeader, bytesRead);
                }
                failCount++;
            }

        }

        if (sizeAccumulator == dumpHeader.DumpSize)
        { // Accumulated size should match the file size
            printf ("\t\t         Read(): PASSED - read dumpHeader and %d sectionHeaders -> file size\r\n", dumpHeader.SectionsCount, pIn->GetError ());
        }
        else
        { // Should return and find the first block and the known structure there.
            if (sizeof sectionHeader == bytesRead)
            {
                printf ("\t\t         Read(): FAILED - read dumpHeader and %d sectionHeaders -> file size (Error: %#x)\r\n", dumpHeader.SectionsCount, pIn->GetError ());
            }
            else
            {
                printf ("\t\t         Read(): FAILED - read dumpHeader and %d sectionHeaders -> file size (Error: %#x) (Expected: %#x) (Actual: %#)\r\n", dumpHeader.SectionsCount, pIn->GetError (), dumpHeader.DumpSize, sizeAccumulator);
            }
            failCount++;
        }

        pIn->GetPos (&myPos);
        if (myPos == sizeOfHeaders)
        {
            printf ("\t\t         Read(): PASSED - current I/O position\r\n");
        }
        else
        { // Should return and find the first block and the known structure there.
            if (sizeof sectionHeader == bytesRead)
            {
                printf ("\t\t         Read(): FAILED - read dumpHeader and %d sectionHeaders (Error: %#x)\r\n", dumpHeader.SectionsCount, pIn->GetError ());
            }
            else
            {
                printf ("\t\t         Read(): FAILED - read dumpHeader and %d sectionHeaders (Error: %#x) (Expected: %#x) (Actual: %#)\r\n", dumpHeader.SectionsCount, pIn->GetError (), sizeof sectionHeader, bytesRead);
            }
            failCount++;
        }


    }
    else
    {
        printf ("\t\t         Read(): FAILED - read dumpHeader (Error: %#x)\r\n", pIn->GetError ());
        failCount++;
    }

    return failCount;
}

//  UINT        Test_Open_Partition_Position_Read_Chunks(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Open_Partition_Position_Read_Chunks(DEVICE_IO *pIn, wstring devName, UINT devID, ULONG bufSize)
{
    UNREFERENCED_PARAMETER (bufSize);

    size_t          bytesRead = 0;
    UINT            failCount = 0;
    ULONGLONG       size = 0;
    PCHAR            pBuf = nullptr;
    ULONGLONG        readOffset = 0;

    if (SUCCEEDED (pIn->Open ()))
    {
        switch (pIn->GetDeviceType ())
        {
            case DEVICE_IO::RAW_DEVICE_TYPE:
            case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
                pIn->Close ();
                failCount = Test_Open_Partition_Device (pIn, devName, devID);
                size = pIn->GetCurrentPartitionSize ();
                break;

            case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
                pIn->Close ();
                failCount = Test_Open_Partition_File (pIn, devName, devID);
                size = pIn->GetCurrentFileSize ();
                break;

            case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
            case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
                pIn->Close ();
                failCount++;
                break;
        }
    }
    else
    {
        printf ("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError ());
        failCount++;
    }

    readOffset = 99126;
    if (FALSE == TEST_SetPos_Func (pIn, readOffset))
    { // move the offset away from the first block
        failCount++;
    }
    else
    {
        const UINT bufSize = 2125;
        pBuf = new CHAR[bufSize];
        if (SUCCEEDED(pIn->Read ((PCHAR)pBuf, bufSize, &bytesRead)) && (bytesRead == bufSize) )
        {
            if (ValidateBuffer (pBuf, (ULONG)bytesRead, readOffset))
            {
                printf ("\t\t         Read(): PASSED - buffer VALID ");
            }
            else
            {
                printf ("\t\t         Read(): FAILED - buffer INVALID ");
                failCount++;
            }

            printf ("(Requested: %#lx) (Actual Read: %#lx)\r\n", bufSize, bytesRead);

        }
        else
        {
            printf ("\t\t         Read(): FAILED (Error: %#x) (Expected Read: %#lx) (Actual Read: %#lx)\r\n", pIn->GetError (), bufSize, bytesRead);
            failCount++;
        }

        delete pBuf;
    }


    // Set the position to the last cache buffer and read a little less than what remains in the partition
    #define BYTES_REMAIN  (pIn->GetBlockSize () + 20)
    readOffset = pIn->GetCurrentPartitionSize () - BYTES_REMAIN;
    if (FALSE == TEST_SetPos_Func (pIn, readOffset))
    { // Should return and find the first block and the known structure there.
        failCount++;
    }
    else
    {
        const UINT bufSize = pIn->GetBlockSize () + 10;
        pBuf = new CHAR[bufSize];
        if (SUCCEEDED (pIn->Read ((PCHAR)pBuf, bufSize, &bytesRead)) && (bytesRead == bufSize))
        {
            if (ValidateBuffer (pBuf, (ULONG)bytesRead, readOffset))
            {
                printf ("\t\t         Read(): PASSED - buffer VALID ");
            }
            else
            {
                printf ("\t\t         Read(): FAILED - buffer INVALID ");
                failCount++;
            }

            printf ("(Remaining: %#lx) (Requested: %#lx)  (Actual Read: %#lx)\r\n", BYTES_REMAIN, bufSize, bytesRead);
        }
        else
        {
            printf ("\t\t         Read(): FAILED (Error: %#x) (Requested: %#lx) (Actual Read: %#lx)\r\n", pIn->GetError (), bufSize, bytesRead);
            failCount++;
        }

        delete pBuf;
    }
    #undef BYTES_REMAIN

    // Set the position to the last cache buffer and read a more than what remains in the partition
    #define BYTES_REMAIN  (pIn->GetBlockSize () + 20)
    readOffset = pIn->GetCurrentPartitionSize () - BYTES_REMAIN;
    if (FALSE == TEST_SetPos_Func (pIn, readOffset))
    { // Should return and find the first block and the known structure there.
        failCount++;
    }
    else
    {
        const UINT bufSize = 2 * pIn->GetBlockSize () + 10;
        pBuf = new CHAR[bufSize];
        if ( SUCCEEDED(pIn->Read ((PCHAR)pBuf, bufSize, &bytesRead)) &&
             (DEVICE_IO::IO_ERROR_READ_PARTIAL == pIn->GetError()) &&
             (bytesRead == BYTES_REMAIN)
           )
        {
            if (ValidateBuffer (pBuf, (ULONG)bytesRead, readOffset))
            {
                printf ("\t\t         Read(): PASSED - buffer VALID ");
            }
            else
            {
                printf ("\t\t         Read(): FAILED - buffer INVALID ");
                failCount++;
            }

            printf ("(Remaining: %#lx) (Requested: %#lx)  (Actual Read: %#lx)\r\n", BYTES_REMAIN, bufSize, bytesRead);
        }
        else
        {
            printf ("\t\t         Read(): FAILED (Error: %#x) (Requested: %#lx) (Actual Read: %#lx)\r\n", pIn->GetError (), bufSize, bytesRead);
            failCount++;
        }

        delete pBuf;
    }
    #undef BYTES_REMAIN

    readOffset = 0x1095;
    if (FALSE == TEST_SetPos_Func (pIn, readOffset))
    { // Should return and find the first block and the known structure there.
        failCount++;
    }
    else
    {
        const UINT bufSize = 65432;
        pBuf = new CHAR[bufSize];
        if (SUCCEEDED (pIn->Read ((PCHAR)pBuf, bufSize, &bytesRead)) && (bytesRead == bufSize))
        {
            if (ValidateBuffer (pBuf, (ULONG)bytesRead, readOffset))
            {
                printf ("\t\t         Read(): PASSED - buffer VALID ");
            }
            else
            {
                printf ("\t\t         Read(): FAILED - buffer INVALID ");
                failCount++;
            }

            printf ("(Requested: %#lx)  (Actual Read: %#lx)\r\n", bufSize, bytesRead);
        }
        else
        {
            printf ("\t\t         Read(): FAILED (Error: %#x) (Expected Read: %#lx) (Actual Read: %#lx)\r\n", pIn->GetError (), bufSize, bytesRead);
            failCount++;
        }

        delete pBuf;
    }

    readOffset = 0x3435;
    if (FALSE == TEST_SetPos_Func (pIn, readOffset))
    { // Should return and find the first block and the known structure there.
        failCount++;
    }
    else
    {
        const UINT bufSize = 33221;
        pBuf = new CHAR[bufSize];
        if (SUCCEEDED (pIn->Read ((PCHAR)pBuf, bufSize, &bytesRead)) && (bytesRead == bufSize))
        {
            if (ValidateBuffer (pBuf, (ULONG)bytesRead, readOffset))
            {
                printf ("\t\t         Read(): PASSED - buffer VALID ");
            }
            else
            {
                printf ("\t\t         Read(): FAILED - buffer INVALID ");
                failCount++;
            }

            printf ("(Requested: %#lx)  (Actual Read: %#lx)\r\n", bufSize, bytesRead);
        }
        else
        {
            printf ("\t\t         Read(): FAILED (Error: %#x) (Expected Read: %#lx) (Actual Read: %#lx)\r\n", pIn->GetError (), bufSize, bytesRead);
            failCount++;
        }

        delete pBuf;
    }

    return failCount;
}

//  UINT        Test_Open_Partition_Position_Read_Write_Chunk (DEVICE_IO *pIn, wstring devName, UINT devID, ULONG bufSize)
UINT Test_Open_Partition_Position_Read_Write_Chunk (DEVICE_IO *pIn, wstring devName, UINT devID, ULONG bufSize)
{
    UNREFERENCED_PARAMETER (bufSize);

    size_t      bytesProcessed = 0;
    ULONGLONG   readOffset = 0;
    UINT        failCount = 0;
    ULONGLONG   size = 0;
    PCHAR       pBufSave = nullptr;
    PCHAR       pBufVerify = nullptr;
    PCHAR       pBufWrite = nullptr;

    if (SUCCEEDED (pIn->Open ()))
    {
        switch (pIn->GetDeviceType ())
        {
            case DEVICE_IO::RAW_DEVICE_TYPE:
            case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
                pIn->Close ();
                failCount = Test_Open_Partition_Device (pIn, devName, devID);
                size = pIn->GetCurrentPartitionSize ();
                break;

            case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
                pIn->Close ();
                failCount = Test_Open_Partition_File (pIn, devName, devID);
                size = pIn->GetCurrentFileSize ();
                break;

            case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
            case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
                pIn->Close ();
                failCount++;
                break;
        }
    }
    else
    {
        printf ("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError ());
        failCount++;
    }

    readOffset = 90210;
    readOffset = 0x100100100;
    if (FALSE == TEST_SetPos_Func (pIn, readOffset))
    { // move the offset away from the first block
        failCount++;
    }
    else
    {
        const UINT bufSize = 42125;

        pBufSave = new CHAR[bufSize];
        if (SUCCEEDED (pIn->Read((PCHAR)pBufSave, bufSize, &bytesProcessed)) && (bytesProcessed == bufSize))
        {
            if (ValidateBuffer (pBufSave, (ULONG)bytesProcessed, readOffset))
            {
                printf ("\t\t         Read(): PASSED - save buffer VALID ");
            }
            else
            {
                printf ("\t\t         Read(): FAILED - save buffer INVALID ");
                failCount++;
            }

            printf ("(Requested: %#lx) (Actual Read: %#lx)\r\n", bufSize, bytesProcessed);
        }
        else
        {
            printf ("\t\t         Read(): FAILED save buffer read (Error: %#x) (Expected Read: %#lx) (Actual Read: %#lx)\r\n", pIn->GetError (), bufSize, bytesProcessed);
            failCount++;
        }

        pBufWrite = new CHAR[bufSize];
        if (nullptr != pBufWrite)
        {
            memset(pBufWrite, '2', bufSize);
            // overwrite the data with zeros
            if (FALSE == TEST_SetPos_Func(pIn, readOffset))
            { // move the offset away from the first block
                failCount++;
            }
            else
            {
                if (SUCCEEDED(pIn->Write((PCHAR)pBufWrite, bufSize, &bytesProcessed)) && (bytesProcessed == bufSize))
                {
                    printf("\t\t        Write(): PASSED\r\n");

                }
                else
                {
                    printf("\t\t        Write(): FAILED (Error: %#x) (Expected Write: %#lx) (Actual Write: %#lx)\r\n", pIn->GetError(), bufSize, bytesProcessed);
                    failCount++;
                }
            }

            pBufVerify = new CHAR[bufSize];
            if (nullptr != pBufVerify)
            {
                ZeroMemory(pBufVerify, bufSize);
                // overwrite the data with zeros
                if (FALSE == TEST_SetPos_Func(pIn, readOffset))
                { // move the offset away from the first block
                    failCount++;
                }
                else
                {
                    if (SUCCEEDED(pIn->Read((PCHAR)pBufVerify, bufSize, &bytesProcessed)) && (bytesProcessed == bufSize))
                    {
                        if (0 == memcmp(pBufWrite, pBufVerify, bytesProcessed))
                        {
                            printf("\t\t         Read(): PASSED - buffer written VALID ");
                        }
                        else
                        {
                            printf("\t\t         Read(): FAILED - buffer written INVALID ");
                            failCount++;
                        }

                        printf("(Requested: %#lx) (Actual Read: %#lx)\r\n", bufSize, bytesProcessed);
                    }
                    else
                    {
                        printf("\t\t         Read(): FAILED buffer written (Error: %#x) (Expected Read: %#lx) (Actual Read: %#lx)\r\n", pIn->GetError(), bufSize, bytesProcessed);
                        failCount++;
                    }
                }

                delete pBufVerify;
            }

            delete pBufWrite;
        }

        // Restore the original data
        if (FALSE == TEST_SetPos_Func (pIn, readOffset))
        { // move the offset away from the first block
            failCount++;
        }
        else
        {
            if (SUCCEEDED (pIn->Write((PCHAR)pBufSave, bufSize, &bytesProcessed)) && (bytesProcessed == bufSize))
            {
                printf ("\t\t        Write(): PASSED - restore blocks\r\n");

            }
            else
            {
                printf ("\t\t        Write(): FAILED - restore blocks (Error: %#x) (Expected Write: %#lx) (Actual Write: %#lx)\r\n", pIn->GetError (), bufSize, bytesProcessed);
                failCount++;
            }
        }

        delete pBufSave;
    }

    return failCount;
}

//    UINT        Test_Device_Specific(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT Test_Device_Specific(DEVICE_IO *pIn, wstring devName, UINT devID)
{
    UNREFERENCED_PARAMETER(devID);

    UINT        failCount = 0;
    ULONGLONG   size = pIn->GetCurrentFileSize();

    // remove the file if it already exists
    if (0 != size )
    {
        pIn->Close();
        DeleteFileW(devName.c_str());
    }

    //  This open should occur on an empty or a non-existing file
    if ( FAILED(pIn->Open(devName))
         && (DEVICE_IO::IO_ERROR_ALREADY_OPENED != pIn->GetError())
       )
    {
        printf("\t\t         Open(): FAILED (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }
    else
    {
        size = pIn->GetCurrentFileSize();

        if (DEVICE_IO::PLAIN_FILE_DEVICE_TYPE == (pIn->GetDeviceType()) )
        {
            failCount = Test_Write_Read_Device_Specific(pIn);
            size = pIn->GetCurrentFileSize();
        }
        else
        {
            failCount++;
        }

        pIn->Close();

    }

    return failCount;
}

// // // // // Helpers // // // // //


//  UINT        ResultPartitionedDevice(DEVICE_IO *pIn, wstring devName, UINT devID)
UINT ResultPartitionedDevice(DEVICE_IO *pIn, wstring devName, UINT devID)
{
    UINT failCount = 0;
    HRESULT ret;
    ULONGLONG curPos;

    if (devID == INVALID_DEVICE_ID)
    {
        if (devName.empty())
        {
            if (pIn->GetDeviceName().empty())
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
        else
        {
            if (devName.compare(pIn->GetDeviceName()) == 0)
            {
                printf("\t\t    Device Name: PASSED\r\n");
            }
            else
            {
                printf("\t\t    Device Name: FAILED (Expected: %s) (Actual: %s)\r\n", devName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }
        }
    }
    else
    {
        if (!pIn->GetDeviceName().empty())
        {
            wstring tmpName = PHYSICAL_DEVICE_STRING;
            WCHAR val[MAX_DEV_ID_STRING_SIZE] = { 0 };

            _itow(devID, val, 10);
            tmpName.append(val);

            if (0 == _wcsicmp(pIn->GetDeviceName().c_str(), tmpName.c_str()))
            {
                printf("\t\t      Device ID: PASSED\r\n");
            }
            else
            {
                printf("\t\t      Device ID: FAILED (ID: %d) (Expected Name: %ls) (Actual Name: %ls)\r\n", devID, tmpName.c_str(), pIn->GetDeviceName().c_str());
                failCount++;
            }

        }
        else
        {
            printf("\t\t      Device ID: FAILED (ID: %d) (Expected Name: \"empty string\") (Actual Name: %ls)\r\n", devID, pIn->GetDeviceName().c_str());
            failCount++;
        }

    }

    // Device Type: RAW or Removable are OK, always display the type
    switch (pIn->GetDeviceType())
    {
    case DEVICE_IO::RAW_DEVICE_TYPE:
        printf("\t\t    Device Type: PASSED (RAW_DEVICE_TYPE)\r\n");
        break;

    case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
        printf("\t\t    Device Type: PASSED (REMOVABLE_MEDIA_DEVICE_TYPE)\r\n");
        break;

    case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
        printf("\t\t    Device Type: FAILED (PLAIN_FILE_DEVICE_TYPE)\r\n");
        break;

    case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
        printf("\t\t    Device Type: FAILED (UNINITIALIZED_DEVICE_TYPE)\r\n");
        break;

    case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
    default:
        printf("\t\t    Device Type: FAILED (UNSUPPORTED_DEVICE_TYPE)\r\n");
        break;
    }

    // Device Size: if no device initialized, size = 0
    if (pIn->GetCurrentFileSize() != 0)
    {
        printf("\t\t    Device Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t    Device Size: FAILED (%ld)\r\n", pIn->GetCurrentFileSize());
        failCount++;
    }

    // Device partition type: should be PARTITION_TYPE_GPT or PARTITION_TYPE_MBR
    if (pIn->GetDevicePartitionType() == PARTITION_STYLE_GPT)
    {
        printf("\t\t  Dev Part Type: PASSED (GPT)\r\n");
    }
    else if (pIn->GetDevicePartitionType() == PARTITION_STYLE_MBR)
    {
        printf("\t\t  Dev Part Type: PASSED (MBR)\r\n");
    }
    else
    {
        printf("\t\t  Dev Part Type: FAILED\r\n");
        failCount++;
    }

    // Device partition count: should be zero
    if (pIn->GetPartitionCount() != 0)
    {
        printf("\t\t Dev Part Count: PASSED\r\n");
    }
    else
    {
        printf("\t\t Dev Part Count: FAILED (Count: %ld)\r\n", pIn->GetPartitionCount());
        failCount++;
    }

    // Device pos: should fail with returned position of zero and error = IO_ERROR_INVALID_HANDLE
    ret = pIn->GetIoPos(&curPos);
    if (SUCCEEDED(ret))
    {
        printf("\t\t     Device Pos: PASSED\r\n");
    }
    else
    {
        printf("\t\t     Device Pos: FAILED (Position: %d) (GetError: %#x)\r\n", curPos, pIn->GetError());
        failCount++;
    }

    // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // // //

    // Partition: name: if no selected partition, no name {empty()}
    if (!pIn->GetCurrentPartitionName().empty())
    {
        printf("\t\t  Cur Part Name: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Name: FAILED (Name:%ls)\r\n", pIn->GetCurrentPartitionName().c_str());
        failCount++;
    }

    // Partition Type: With a no current partition selected it should be one of
    // PARTITION_STYLE_MBR or PARTITION_STYLE_GPT
    switch (pIn->GetCurrentPartitionType())
    {
    case PARTITION_STYLE_MBR:
        printf("\t\t Cur Part Style: PASSED (PARTITION_STYLE_MBR)\r\n");
        break;
    case PARTITION_STYLE_GPT:
        printf("\t\t Cur Part Style: PASSED (PARTITION_STYLE_GPT)\r\n");
        break;
    default:
        printf("\t\t Cur Part Style: FAILED (undefined/invalid)\r\n");
        failCount++;
        break;
    }

    // Partition size: if no selected partition then size should be 0
    if (pIn->GetCurrentPartitionSize() != 0)
    {
        printf("\t\t  Cur Part Size: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part Size: FAILED\r\n");
        failCount++;
    }

    // Partition index: if no selected partition, no index {INVALID_INDEX}
    if (pIn->GetCurrentPartitionNumber() != INVALID_INDEX)
    {
        printf("\t\t Cur Part Index: PASSED\r\n");
    }
    else
    {
        printf("\t\t Cur Part Index: FAILED\r\n");
        failCount++;
    }

    // Partition GUID: if no selected partition, no GUID {empty()}
    if (pIn->GetCurrentPartitionGUID().compare(L"{00000000-0000-0000-0000-000000}") != 0)
    {
        printf("\t\t  Cur Part GUID: PASSED\r\n");
    }
    else
    {
        printf("\t\t  Cur Part GUID: FAILED (GUID: %s)\r\n", pIn->GetCurrentPartitionGUID());
        failCount++;
    }

    // Partition Pos: should fail with returned position of zero and error = IO_ERROR_PARTITION_NOT_SELECTED
    ret = pIn->GetPos(&curPos);
    if (SUCCEEDED(ret))
    {
        printf("\t\t         GetPos: PASSED\r\n");
    }
    else
    {
        printf("\t\t         GetPos: FAILED (Position: %d) (GetError: %#x)\r\n", curPos, pIn->GetError());
        failCount++;
    }

    return failCount;
}

// BOOL ValidateBuffer (PCHAR buff, UINT buffSize, ULONGLONG readOffset)
BOOL ValidateBuffer (PCHAR buff, ULONG buffSize, ULONGLONG readOffset)
{
    BOOL ret = TRUE;

    for (ULONGLONG i = 0; i < buffSize; i++)
    {
        if (OFFSET2VALUE(readOffset + i) != buff[i])
        {
            ret = FALSE;
            break;
        }

    }

    return ret;
}

// BOOL TEST_SetPos_Func(DEVICE_IO * pIn, ULONGLONG newPos)
BOOL TEST_SetPos_Func(DEVICE_IO * pIn, ULONGLONG newPos)
{
    BOOL ret = FALSE;

    HRESULT hr = pIn->SetPos(newPos);
    if (FAILED (hr))
    { // SetPos() should fail if we set the size to some value to (or beyond) the partition size
        if ( (newPos >= pIn->GetCurrentPartitionSize()) &&
             (DEVICE_IO::IO_ERROR_EOF == pIn->GetError())
           )
        { // Setting the position to (or beyond) the partition size should fail with EOF - PASS
            printf ("\t\t       GetPos(): PASSED - set to EOF\r\n");
            ret = TRUE;
        }
        else
        { // Non-EOF failures - FAIL
            printf ("\t\t       SetPos(): FAILED (Error: %#x) (Expected Position: %#lx) (Actual Position: %#lx) \r\n", pIn->GetError (), pIn->GetCurrentPartitionSize(), newPos);
        }

    }
    else
    { // SetPos() to some value within the partition should be successful
        ULONGLONG curPos;

        if (FAILED(pIn->GetPos(&curPos)))
        {
            printf("\t\t       GetPos(): FAILED (Error: %#x)\r\n", pIn->GetError());
        }
        else
        { // Verify we are in the correct position after SetPos()
            switch (pIn->GetDeviceType())
            {
                case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
                case DEVICE_IO::RAW_DEVICE_TYPE:
                    if (newPos > pIn->GetCurrentPartitionSize())
                    { // If setting to position past the end of partition
                        if (pIn->GetCurrentPartitionSize() == curPos)
                        { // should clamp to partition size
                            printf("\t\t       SetPos(): PASSED (%#lx)\r\n", newPos);
                            ret = TRUE;
                        }
                        else
                        {
                            printf("\t\t       SetPos(): FAILED (Error: %#x) (Expected: %#lx) (Actual CurPos: %#lx)\r\n", pIn->GetError(), pIn->GetCurrentFileSize(), curPos);
                        }
                    }
                    else
                    { // otherwise, should be in the right place in the partition.
                        if (curPos == newPos)
                        {
                            printf("\t\t       SetPos(): PASSED (POS:%#lx)\r\n", newPos);
                            ret = TRUE;
                        }
                        else
                        {
                            printf("\t\t       SetPos(): FAILED (Error: %#x) (Expected: %#lx) (Actual CurPos: %#lx)\r\n", pIn->GetError(), pIn->GetCurrentFileSize(), curPos);
                        }
                    }

                    break;

                case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
                    if (newPos == curPos)
                    { // For fiels position can be set past EOF
                        printf("\t\t       SetPos(): PASSED (POS:%#lx)\r\n", newPos);
                        ret = TRUE;
                    }
                    else
                    {
                        printf("\t\t       SetPos(): FAILED (Error: %#x) (Expected: %#lx) (Actual CurPos: %#lx)\r\n", pIn->GetError(), newPos, curPos);
                    }
                    break;

                case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
                case DEVICE_IO::UNSUPPORTED_DEVICE_TYPE:
                default:
                    printf("\t\t       SetPos(): FAILED (Error: %#x) Wrong Device Type!\r\n", pIn->GetError());
                    break;
            }
        }

    }

    return ret;
}

// BOOL TEST_Read_Func(DEVICE_IO * pIn, PCHAR buff, UINT buffSize)
BOOL TEST_Read_Func (DEVICE_IO * pIn, PCHAR buff, UINT buffSize)
{
    BOOL ret = FALSE;
    size_t bytesRead = 0;

    if (SUCCEEDED (pIn->Read(buff, buffSize, &bytesRead)))
    {
        if (bytesRead == buffSize)
        {
            printf ("\t\t         Read(): PASSED\r\n");
        }
        else
        {
            printf ("\t\t         Read(): FAILED (Error: %#x) (Expected: %d bytes) (Actual %d bytes)\r\n", pIn->GetError (), buffSize, bytesRead);
        }

    }
    else
    {
        printf ("\t\t         Read(): FAILED (Error: %#x)\r\n", pIn->GetError ());
    }

    return ret;
}

// BOOL TEST_Write_Func(DEVICE_IO * pIn, PCHAR buff, UINT buffSize)
BOOL TEST_Write_Func (DEVICE_IO * pIn, PCHAR buff, UINT buffSize)
{
    BOOL ret = FALSE;
    size_t bytesWritten = 0;

    if (SUCCEEDED (pIn->Write(buff, buffSize, &bytesWritten)))
    {
        if (bytesWritten == buffSize)
        {
            printf ("\t\t         Read(): PASSED\r\n");
        }
        else
        {
            printf ("\t\t         Read(): FAILED (Error: %#x) (Expected: %d bytes) (Actual %d bytes)\r\n", pIn->GetError (), buffSize, bytesWritten);
        }

    }
    else
    {
        printf ("\t\t         Read(): FAILED (Error: %#x)\r\n", pIn->GetError ());
    }

    return ret;
}

// UINT Test_Write_Read_Device_Specific(DEVICE_IO *pIn)
UINT Test_Write_Read_Device_Specific(DEVICE_IO *pIn)
{
    UNREFERENCED_PARAMETER(pIn);

    DEVICE_SPECIFIC_INFO    devInfoWrite1 = { 0 };
    DEVICE_SPECIFIC_INFO    devInfoWrite2 = { 0 };
    DEVICE_SPECIFIC_INFO    devInfoVerify = { 0 };
    ULONGLONG               offset = 0;
    UINT                    failCount = 0;
    size_t                  bytesProcessed = 0;
    CHAR                    pBufFiller[TEST_FILLER_SIZE] = { 0 };

    // Init the buffer with repeating 'A' - 'Z' pattern
    for (UINT i = 0; i < sizeof(pBufFiller); i++)
    {
        pBufFiller[i] = 'A' + (i % 26);
    }

    // Init the two data structures with different data
    devInfoWrite1.Type = PROCESSOR_ARCHITECTURE_INTEL;
    devInfoWrite1.DumpHeaderInstanceID = (ULONGLONG)-1;
    devInfoWrite1.CpuContextAddress = 0xfedcba9876543210; // Intel struct element
    devInfoWrite1.BugCheckCode = 76;
    devInfoWrite1.BugCheckParam1 = 1;
    devInfoWrite1.BugCheckParam2 = 2;
    devInfoWrite1.BugCheckParam3 = 3;
    devInfoWrite1.BugCheckParam4 = 4;

    devInfoWrite2.Type = PROCESSOR_ARCHITECTURE_ARM64;
    devInfoWrite2.DumpHeaderInstanceID = (ULONGLONG)-2;
    devInfoWrite2.APRegPA = 0x1010101010101010;         // ARM struct element
    devInfoWrite2.VA = 0x3232323232323232;              // ARM struct element
    devInfoWrite2.PA = 0x5454545454545454;              // ARM struct element
    devInfoWrite2.Size = 0x76767676;
    devInfoWrite2.BugCheckCode = 98;
    devInfoWrite2.BugCheckParam1 = 5;
    devInfoWrite2.BugCheckParam2 = 6;
    devInfoWrite2.BugCheckParam3 = 7;
    devInfoWrite2.BugCheckParam4 = 8;

    // // //  Write the data  // // //

    // Write the DEVICE_SPECIFIC_INFO to position zero
    if (FAILED(WriteDeviceSpecificInfo(pIn, &devInfoWrite1, offset)))
    {
        printf("\t\t   DevSpecInfo1: FAILED WriteDeviceSpecificInfo() - (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    // Write our test data pattern
    if (FAILED(pIn->Write(pBufFiller, sizeof pBufFiller, &bytesProcessed)))
    {
        printf("\t\t        Write(): FAILED buffer write in Device Specific Info - (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    // Make sure we know where the append happens
    if (FAILED(pIn->GetPos(&offset)))
    {
        printf("\t\t       GetPos(): FAILED in Device Specific Info - (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    // Append DEVICE_SPECIFIC_INFO
    if (FAILED(WriteDeviceSpecificInfo(pIn, &devInfoWrite2, offset)))
    {
        printf("\t\t   DevSpecInfo1: FAILED Append with WriteDeviceSpecificInfo() - (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }

    // // //  Read and verify the data  // // //
    
    // Check the data at offset 0 - strcut 1
    if (FAILED(ReadDeviceSpecificInfo(pIn, &devInfoVerify, 0)))
    { // Read struct 1
        printf("\t\t   DevSpecInfo1: FAILED ReadDeviceSpecificInfo() - (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }
    else if ( 0 != memcmp(&devInfoVerify, &devInfoWrite1, sizeof devInfoWrite1))
    { // Verify struct 1
        printf("\t\t   DevSpecInfo1: FAILED verification\r\n");
        failCount++;
    }
    else
    {
        printf("\t\t   DevSpecInfo1: PASSED verification\r\n");
    }

    // Check the append data (struct 2)
    if (FAILED(ReadDeviceSpecificInfo(pIn, &devInfoVerify, offset)))
    { // Read struct 2
        printf("\t\t   DevSpecInfo2: FAILED ReadDeviceSpecificInfo() - (Error: %#x)\r\n", pIn->GetError());
        failCount++;
    }
    else if (0 != memcmp(&devInfoVerify, &devInfoWrite2, sizeof devInfoWrite2))
    { // Verify struct 2
        printf("\t\t   DevSpecInfo2: FAILED verification\r\n");
        failCount++;
    }
    else
    {
        printf("\t\t   DevSpecInfo2: PASSED verification\r\n");
    }


    return failCount;
}
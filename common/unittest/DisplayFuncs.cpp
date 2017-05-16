/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   DisplayFuncs.cpp

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/

#include <DisplayFuncs.h>

void DisplayResults01(DEVICE_IO *pIn)
{
    printf("\t                   name: %ls\r\n", pIn->GetDeviceName().empty() ? L"<<Not Set>>" : pIn->GetDeviceName().c_str());

    switch (pIn->GetDeviceType())
    {
    case DEVICE_IO::UNINITIALIZED_DEVICE_TYPE:
        printf("\t                   Type: UNINITIALIZED_DEVICE_TYPE\r\n");
        break;
    case DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE:
        printf("\t                   Type: REMOVABLE_MEDIA_DEVICE_TYPE\r\n");
        break;
    case DEVICE_IO::RAW_DEVICE_TYPE:
        printf("\t                   Type: RAW_DEVICE_TYPE\r\n");
        break;
    case DEVICE_IO::PLAIN_FILE_DEVICE_TYPE:
        printf("\t                   Type: PLAIN_FILE_DEVICE_TYPE\r\n");
        break;
    default:
        printf("\t                   Type: UNSUPPORTED_DEVICE_TYPE\r\n");
        break;
    }

    if (pIn->GetDeviceType() == DEVICE_IO::PLAIN_FILE_DEVICE_TYPE)
    { // For plain files use this block
        ULONGLONG curPos;
        HRESULT ret = pIn->GetPos(&curPos);

        printf("\t                   Size: %lld\r\n", pIn->GetCurrentFileSize());
        if (SUCCEEDED(ret))
        {
            printf("\t       Current position: %lld\r\n", curPos);
        }
        else
        {
            printf("\t       Current position: FAILED (error: %lld)\r\n", ret);
        }

    }
    else if (pIn->GetDeviceType() == DEVICE_IO::RAW_DEVICE_TYPE)
    { // For devices use this block
        if (pIn->GetError() == DEVICE_IO::IO_ERROR_PARTITION_NOT_FOUND)
        {
            printf("\t                  ERROR: Partition not found\r\n");
        }
        else if (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE)
        {
            printf("\t                  ERROR: RAW device not opened\r\n");
        }
        else
        {
            printf("\t        Partition Information:\r\n");
            printf("\t                  count: %ld\r\n", pIn->GetPartitionCount());
            if (pIn->GetCurrentPartitionNumber() == INVALID_INDEX)
            {
                printf("\t               Selected: none\r\n");
            }
            else
            {
                printf("\t               Selected: %ld\r\n", pIn->GetCurrentPartitionNumber());

                switch (pIn->GetError())
                {
                case DEVICE_IO::IO_ERROR_PARTITION_NOT_FOUND:
                    printf("\t             Error code: %ls\r\n", L"Partition not found");
                    break;
                case DEVICE_IO::IO_ERROR_PARTITION_NOT_SET:
                    printf("\t             Error code: %ls\r\n", L"Partition not set or selected");
                    break;
                case DEVICE_IO::IO_OK:
                    printf("\t                   Name: %ls\r\n", pIn->GetCurrentPartitionName().c_str());
                    printf("\t                   GUID: %ls\r\n", pIn->GetCurrentPartitionGUID().c_str());
                    printf("\t                   Size: %lld\r\n", pIn->GetCurrentPartitionSize());
                    break;
                default:
                    printf("\t             Error code: %llx\r\n", pIn->GetError());
                    break;
                }

            }
        }
    }
    else if (pIn->GetDeviceType() == DEVICE_IO::REMOVABLE_MEDIA_DEVICE_TYPE)
    { // For Removable media (SD cards) devices use this block
        if (pIn->GetError() == DEVICE_IO::IO_ERROR_PARTITION_NOT_FOUND)
        {
            printf("\t                  ERROR: Partition not found\r\n");
        }
        else if (pIn->GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE)
        {
            printf("\t                  ERROR: REMOVABLE MEDIA device not opened\r\n");
        }
        else
        {
            printf("\t        Partition count: %ld\r\n", pIn->GetPartitionCount());
            if (pIn->GetCurrentPartitionNumber() == INVALID_INDEX)
            {
                printf("\t     Partition Selected: none\r\n");
            }
            else
            {
                printf("\t     Partition Selected: %ld\r\n", pIn->GetCurrentPartitionNumber());

                switch (pIn->GetError())
                {
                    case DEVICE_IO::IO_ERROR_PARTITION_NOT_FOUND:
                        printf("\t             Error code: %ls\r\n", L"Partition not found");
                        break;
                    case DEVICE_IO::IO_ERROR_PARTITION_NOT_SET:
                        printf("\t             Error code: %ls\r\n", L"Partition not set or selected");
                        break;
                    case DEVICE_IO::IO_OK:
                        printf("\t         Partition Name: %ls\r\n", pIn->GetCurrentPartitionName().c_str());
                        printf("\t         Partition GUID: %ls\r\n", pIn->GetCurrentPartitionGUID().c_str());
                        printf("\t         Partition Size: %lld\r\n", pIn->GetCurrentPartitionSize());
                        break;
                    default:
                        printf("\t             Error code: %llx\r\n", pIn->GetError());
                        break;
                }

            }
            
        }
        
    }

}

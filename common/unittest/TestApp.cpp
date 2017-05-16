/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   TestApp.cpp

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/

#include <stdio.h>
#include <DEVICE_IO.h>
#include <FunctionalTests.h>
#include <DisplayFuncs.h>

#define DEFAULT_DEVICE_SPECIFIC_FILE_NAME   L"C:\\tmp\\Device_Specific_Test_File.bin"
#define DEFAULT_PLAIN_INPUT_FILE_NAME       L"C:\\tmp\\8996_UFS_SMALL.bin"
#define DEFAULT_PARTITION_FILE_NAME         L"C:\\tmp\\8996_SVRawDump_Partition.bin"
#define DEFAULT_DEVICE_ID                   3
#define DEFAULT_BUFFER_SIZE                 0x5000

WCHAR PLAIN_INPUT_FILE_NAME[MAX_PATH + 1]   = {0};
WCHAR PARTITION_FILE_NAME[MAX_PATH + 1]     = {0};
UINT  DEVICE_ID                             = DEFAULT_DEVICE_ID;
WCHAR DEVICE_NAME[MAX_PATH + 1]             = {0};
ULONG BUFFER_SIZE                            = DEFAULT_BUFFER_SIZE;

int __cdecl main(int argc, char **argv)
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    UINT totalFailed = 0;
    UINT scenarioFailures = 0;
    UINT testId = 0;

    wcscpy_s(PLAIN_INPUT_FILE_NAME, sizeof PLAIN_INPUT_FILE_NAME, DEFAULT_PLAIN_INPUT_FILE_NAME);
    wcscpy_s(PARTITION_FILE_NAME, sizeof PARTITION_FILE_NAME, DEFAULT_PARTITION_FILE_NAME);
    wcscpy_s(DEVICE_NAME, sizeof DEVICE_NAME, PHYSICAL_DEVICE_STRING);
    {
        _itow(DEVICE_ID, &DEVICE_NAME[wcslen(DEVICE_NAME)], 10);
    }



    printf("=== BEGIN: Test Application for File_IO\r\n");

    // // // // // //  Testing of Device Specific Data Structure Library  // // // // // //

    // // // Test - Uninitialized DEVICE_IO class
    printf("=== === (%d) Begin: - Test Device Spcific Functionality (%ls)\r\n", testId, DEFAULT_DEVICE_SPECIFIC_FILE_NAME);
    {
        UINT localFailures;
        DEVICE_IO  myTest(DEFAULT_DEVICE_SPECIFIC_FILE_NAME);

        myTest.Open();
        DisplayResults01(&myTest);
        localFailures = Test_Device_Specific(&myTest, DEFAULT_DEVICE_SPECIFIC_FILE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: - Test Device Spcific Functionality (%ls)\r\n", testId++, DEFAULT_DEVICE_SPECIFIC_FILE_NAME);

    // // // // // //  Testing of DEVICE_IO Library  // // // // // //

    // // // Test - Uninitialized DEVICE_IO class
    printf("=== === (%d) Begin: - Test uninitialized DEVICE_IO class\r\n", testId);
    {
        UINT localFailures;
        DEVICE_IO  myTest;

        DisplayResults01(&myTest);
        localFailures = Test_Unopened(&myTest, L"", INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: - Test uninitialized DEVICE_IO class\r\n\n", testId++);


    // // // Test - create(name) + close - plain file by name
    printf("=== === (%d) Begin: OPEN - Test for create(name) + close a plain file: %ls\r\n", testId, PLAIN_INPUT_FILE_NAME);
    {
        UINT localFailures;
        DEVICE_IO myTest(PLAIN_INPUT_FILE_NAME);

        DisplayResults01(&myTest);
        localFailures = Test_Unopened(&myTest, PLAIN_INPUT_FILE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(name) + close a plain file: %ls\r\n\n", testId++, PLAIN_INPUT_FILE_NAME);


    // // // Test - Create(name) + close - Device by name
    printf("=== === (%d) Begin: OPEN - Test for create(name) + close on a device name: %ls\r\n", testId, DEVICE_NAME);
    {
        UINT localFailures;
        DEVICE_IO myTest(DEVICE_NAME);

        DisplayResults01(&myTest);
        localFailures = Test_Unopened(&myTest, DEVICE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(name) + close on a device name: %ls\r\n\n", testId++, DEVICE_NAME);


    // // // // Test - Create(ID) + close - Device by ID
    printf("=== === (%d) Begin: OPEN - Test for create(ID) + close on a device ID: %d\r\n", testId, DEVICE_ID);
    {
        UINT localFailures;
        DEVICE_IO myTest(DEVICE_ID);

        DisplayResults01(&myTest);
        localFailures = Test_Unopened(&myTest, L"", DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(ID) + close on a device ID: %d\r\n\n", testId++, DEVICE_ID);


    // // // Test - Create(name) + Open + Close  - Plain file
    printf("=== === (%d) Begin: OPEN - Test for create(Name) + open + close on a plain file: %ls\r\n", testId, PLAIN_INPUT_FILE_NAME);
    {
        DEVICE_IO myTest(PLAIN_INPUT_FILE_NAME);

        UINT localFailures = Test_Open_File(&myTest, PLAIN_INPUT_FILE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(Name) + open + close on a plain file: %ls\r\n\n", testId++, PLAIN_INPUT_FILE_NAME);


    // // // Test - Create(name) + Open + Close  - Device Name
    printf("=== === (%d) Begin: OPEN - Test for create(Name) + open + close on a device name: %ls\r\n", testId, DEVICE_NAME);
    {
        DEVICE_IO myTest(DEVICE_NAME);

        UINT localFailures = Test_Open_Device(&myTest, DEVICE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(Name) + open + close on a device name: %ls\r\n\n", testId++, DEVICE_NAME);


    // // // Test - Create(ID) + Open + Close  - Device ID
    printf("=== === (%d) Begin: OPEN - Test for create(ID) + open + close on a device ID: %d\r\n", testId, DEVICE_ID);
    {
        DEVICE_IO myTest(DEVICE_ID);

        UINT localFailures = Test_Open_Device(&myTest, L"", DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(ID) + open + close on a device ID: %d\r\n\n", testId++, DEVICE_ID);


    // // // Test - Create + setname + Open + Close - Plain file
    printf("=== === (%d) Begin: OPEN - Test for create + setName + open + close on a plain file: %ls\r\n", testId, PLAIN_INPUT_FILE_NAME);
    {
        UINT localFailures = 0;
        DEVICE_IO myTest;

        if (SUCCEEDED(myTest.SetDeviceName(PLAIN_INPUT_FILE_NAME)))
        {
            printf("\t\tSetDeviceName(): PASSED\r\n");
        }
        else
        {
            printf("\t\tSetDeviceName(): FAILED (Error: %#x)\r\n", myTest.GetError());
            localFailures++;
        }

        localFailures += Test_Open_File(&myTest, PLAIN_INPUT_FILE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create + setName + open + close on a plain file: %ls\r\n\n", testId++, PLAIN_INPUT_FILE_NAME);


    // // // Test - Create + setName + Open + Close  - Device Name
    printf("=== === (%d) Begin: OPEN - Test for create + setName + open + close using device name: %ls\r\n", testId, DEVICE_NAME);
    {
        UINT localFailures = 0;
        DEVICE_IO myTest;


        if (SUCCEEDED(myTest.SetDeviceName(DEVICE_NAME)))
        {
            printf("\t\tSetDeviceName(): PASSED\r\n");
        }
        else
        {
            printf("\t\tSetDeviceName(): FAILED (Error: %#x)\r\n", myTest.GetError());
            localFailures++;
        }

        localFailures += Test_Open_Device(&myTest, DEVICE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create + setName + open + close using device name: %ls\r\n\n", testId++, DEVICE_NAME);


    // // // Test - Create + setID + Open + Close - Device ID
    printf("=== === (%d) Begin: OPEN - Test for create + setID + open + close using device ID: %d\r\n", testId, DEVICE_ID);
    {
        UINT localFailures = 0;
        DEVICE_IO myTest;

        if (SUCCEEDED(myTest.SetDeviceID(DEVICE_ID)))
        {
            printf("\t\t  SetDeviceID(): PASSED\r\n");
        }
        else
        {
            printf("\t\t  SetDeviceID(): FAILED (Error: %#x)\r\n", myTest.GetError());
            localFailures++;
        }

        localFailures = Test_Open_Device(&myTest, L"", DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create + setID + open + close using device ID: %d\r\n\n", testId++, DEVICE_ID);

    // // // Test - Create(name) + Open + setPartition(enum) + Close - Plain file
    printf("=== === (%d) Begin: OPEN - Test for create(Name) + open + setPartition + close on a plain file: %ls\r\n", testId, PLAIN_INPUT_FILE_NAME);
    {
        DEVICE_IO myTest(PLAIN_INPUT_FILE_NAME);

        UINT localFailures = Test_Open_Partition_File(&myTest, PLAIN_INPUT_FILE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(Name) + open + setPartition + close on a plain file: %ls\r\n\n", testId++, PLAIN_INPUT_FILE_NAME);

    // // // Test - Create(name) + Open + setPartition(enum) + Close - Device Name
    printf("=== === (%d) Begin: OPEN - Test for create(Name) + open + setPartition + close on a device name: %ls\r\n", testId, DEVICE_NAME);
    {
        DEVICE_IO myTest(DEVICE_NAME);

        UINT localFailures = Test_Open_Partition_Device(&myTest, DEVICE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(Name) + open + setPartition + close on a device name: %ls\r\n\n", testId++, DEVICE_NAME);


    // // // Test - Create(name) + Open + setPartition(enum) + Close - Device ID
    printf("=== === (%d) Begin: OPEN - Test for create(ID) + open + setPartition + close on a device ID: %d\r\n", testId, DEVICE_ID);
    {
        DEVICE_IO myTest(DEVICE_ID);

        UINT localFailures = Test_Open_Partition_Device(&myTest, L"", DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(ID) + open + setPartition + close on a device ID: %d\r\n\n", testId++, DEVICE_ID);


    // // // Test - Create + SetName + Open + setPartition + Close - Plain file
    printf("=== === (%d) Begin: OPEN - Test for create + setName + open + setPartition + close on a plain file: %ls\r\n", testId, PLAIN_INPUT_FILE_NAME);
    {
        UINT localFailures = 0;
        DEVICE_IO myTest;

        if (SUCCEEDED(myTest.SetDeviceName(PLAIN_INPUT_FILE_NAME)))
        {
            printf("\t\tSetDeviceName(): PASSED\r\n");
        }
        else
        {
            printf("\t\tSetDeviceName(): FAILED (Error: %#x)\r\n", myTest.GetError());
            localFailures++;
        }

        localFailures += Test_Open_Partition_File(&myTest, PLAIN_INPUT_FILE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create + setName + open + setPartition + close on a plain file: %ls\r\n\n", testId++, PLAIN_INPUT_FILE_NAME);


    // // // Test - Create + SetName + Open + setPartition + Close - Device Name
    printf("=== === (%d) Begin: OPEN - Test for create + setName + open + setPartition + close using device name: %ls\r\n", testId, DEVICE_NAME);
    {
        UINT localFailures = 0;
        DEVICE_IO myTest;

        if (SUCCEEDED(myTest.SetDeviceName(DEVICE_NAME)))
        {
            printf("\t\tSetDeviceName(): PASSED\r\n");
        }
        else
        {
            printf("\t\tSetDeviceName(): FAILED (Error: %#x)\r\n", myTest.GetError());
            localFailures++;
        }

        localFailures += Test_Open_Partition_Device(&myTest, DEVICE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create + setName + open + setPartition + close using device name: %ls\r\n\n", testId++, DEVICE_NAME);


    // // // Test - Create + SetName + Open + setPartition + Close - Device ID
    printf("=== === (%d) Begin: OPEN - Test for create + setID + open + setPartition + close using device ID: %d\r\n", testId, DEVICE_ID);
    {
        UINT localFailures = 0;
        DEVICE_IO myTest;

        if (SUCCEEDED(myTest.SetDeviceID(DEVICE_ID)))
        {
            printf("\t\t  SetDeviceID(): PASSED\r\n");
        }
        else
        {
            printf("\t\t  SetDeviceID(): FAILED (Error: %#x)\r\n", myTest.GetError());
            localFailures++;
        }

        localFailures += Test_Open_Partition_Device(&myTest, L"", DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create + setID + open + setPartition + close using device ID: %d\r\n\n", testId++, DEVICE_ID);

    // // // Test - Create(name) + Open + setPartition(enum) + Set Position + Close - Plain file
    printf("=== === (%d) Begin: OPEN - Test for create(Name) + open + setPartition + Set Position + close on a plain file: %ls\r\n", testId, PLAIN_INPUT_FILE_NAME);
    {
        DEVICE_IO myTest(PLAIN_INPUT_FILE_NAME);

        UINT localFailures = Test_Open_Partition_Position(&myTest, PLAIN_INPUT_FILE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(Name) + open + setPartition + Set Position + close on a plain file: %ls\r\n\n", testId++, PLAIN_INPUT_FILE_NAME);


    // // // Test - Create(name) + Open + setPartition(enum) + Set Position + Close - Device Name
    printf("=== === (%d) Begin: OPEN - Test for create(Name) + open + setPartition + Set Position + close on a device name: %ls\r\n", testId, DEVICE_NAME);
    {
        DEVICE_IO myTest(DEVICE_NAME);

        UINT localFailures = Test_Open_Partition_Position(&myTest, DEVICE_NAME, INVALID_DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)   End: OPEN - Test for create(Name) + open + setPartition + Set Position + close on a device name: %ls\r\n\n", testId++, DEVICE_NAME);


    // // // Test - Create(name) + Open + setPartition(enum) + Set Position + Close - Device ID
    printf("=== === (%d) Begin: OPEN - Test for create(ID) + open + setPartition + Set Position + close on a device ID: %d\r\n", testId, DEVICE_ID);
    {
        DEVICE_IO myTest(DEVICE_ID);

        UINT localFailures = Test_Open_Partition_Position(&myTest, L"", DEVICE_ID);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf(">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf("\tTest scenario: PASSED\r\n");
        }

        myTest.Close();
    }
    printf("=== === (%d)    End: OPEN - Test for create(ID) + open + setPartition + Set Position + close on a device ID: %d\r\n\n", testId++, DEVICE_ID);


    printf ("=== === (%d) Begin: OPEN - Test for create + Open(ID) + Partition + SetPos + Read(Headers) + close, Headers, on a device ID: %d\r\n", testId, DEVICE_ID);
    {
        UINT localFailures;
        DEVICE_IO  myTest;

        myTest.SetDeviceID (DEVICE_ID);
        DisplayResults01 (&myTest);
        localFailures = Test_Open_Partition_Position_Read_Headers (&myTest, L"", DEVICE_ID, BUFFER_SIZE);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf (">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf ("\tTest scenario: PASSED\r\n");
        }

        myTest.Close ();
    }
    printf ("=== === (%d)   End: OPEN - Test for create + Open(ID) + Partition + SetPos + Read(Headers) + close, Headers, on a device ID: %d\r\n", testId++, DEVICE_ID);


    printf ("=== === (%d) Begin: OPEN - Test for create + Open(ID) + Partition + SetPos + Read(Chunks) + close, Chunks, on a device ID: %d\r\n", testId, DEVICE_ID);
    {
        UINT localFailures;
        DEVICE_IO  myTest;

        myTest.SetDeviceID (DEVICE_ID);
        DisplayResults01 (&myTest);
        localFailures = Test_Open_Partition_Position_Read_Chunks(&myTest, L"", DEVICE_ID, BUFFER_SIZE);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf (">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf ("\tTest scenario: PASSED\r\n");
        }

        myTest.Close ();
    }
    printf ("=== === (%d)   End: OPEN - Test for create + Open(ID) + Partition + SetPos + Read(Chunks) + close, Chunks, on a device ID: %d\r\n", testId++, DEVICE_ID);


    printf ("=== === (%d) Begin: OPEN - Test for create + Open(ID) + Partition + SetPos + Read(Chunks) + Write(Chunk) + close, Headers, on a device ID: %d\r\n", testId, DEVICE_ID);
    {
        UINT localFailures;
        DEVICE_IO  myTest;

        myTest.SetDeviceID (DEVICE_ID);
        DisplayResults01 (&myTest);
        localFailures = Test_Open_Partition_Position_Read_Write_Chunk(&myTest, L"", DEVICE_ID, BUFFER_SIZE);
        if (localFailures > 0)
        {
            totalFailed += localFailures;
            scenarioFailures++;
            printf (">>> Test scenario: FAILED (Failures: %d)\r\n", localFailures);
        }
        else
        {
            printf ("\tTest scenario: PASSED\r\n");
        }

        myTest.Close ();
    }
    printf ("=== === (%d)   End: OPEN - Test for create + Open(ID) + Partition + SetPos + Read(Chunks) + Write(Chunk) + close, Headers, on a device ID: %d\r\n", testId++, DEVICE_ID);

    // // // //
    printf("=== END: Test Application for File_IO\r\n");

    printf("\r\n\t                === Summary ===\r\n");
    printf("\t    Scenarios Run: %d, PASSED: %d,  FAILED: %d\r\n", testId, (testId-scenarioFailures), scenarioFailures);

    // Report if any individual tests failed, in the future it may be possible that
    // some tests will fail yet do not result in a scenario failure.
    if (totalFailed == 0)
    {
        printf("\t Individual tests: PASSED\r\n");
    }
    else
    {
        printf("\t Individual tests: FAILED (%d)\r\n", totalFailed);

    }

    return 0;
}

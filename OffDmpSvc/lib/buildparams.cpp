/*++

Copyright (c) 2014 Microsoft Corporation, All Rights Reserved

Module Name:
    buildparams.cpp

Environment:
    User Mode

--*/
#include "QCom32.h"
#include "intelx86.h"
#include "svspecific.h"
#include "buildparams.h"
#include "wpcrdmpsentinel.h"
#include <zwapi.h>
#define NO_INTERFACE_DECL
#include <ntefi.h>
#include <ntexapi.h>
#undef NO_INTERFACE_DECL
#include <bugcodes.h>
#include "logging.h"
#include <guiddef.h>
#include <new.h>
#include <werapi.h>
#include <Pathcch.h>


#define LENGTH_PATH_TO_LOG_FILE     50
#define COPY_BUFFER_SIZE            (4*1024)
//
// This defines the maximum number of times this library is going to try
// finding the rawdump partition in the device.
//
#define DRIVE_LAYOUT_INFO_MAX_TRIES 8

typedef bool (CALLBACK* ConvertRawToDump)(LPWSTR, LPWSTR, LPWSTR); // raw2dump.dll export

HRESULT ConvertRaw2WindowsDump(
    _In_ PDMP_CONTEXT Context
)
{
    //
    // Load raw2dump.dll and call its ConvertRawToDump function
    //
    ConvertRawToDump pfnConvertRawToDump = nullptr;
    HRESULT hr = E_FAIL;
    HINSTANCE const hRaw2Dump = LoadLibraryExW(L"raw2dump.dll", nullptr, 0);

    if (nullptr != hRaw2Dump) {
        pfnConvertRawToDump = (ConvertRawToDump)GetProcAddress(hRaw2Dump, "ConvertRawToDump");
        if (nullptr != pfnConvertRawToDump) {
            bool ret = pfnConvertRawToDump(
                           Context->RawDumpPath,
                           Context->RawDumpInfoPath,
                           WINDOWSDUMP_FILE_PATH
                           );
            if (ret == false) {
                hr = E_FAIL;
                TraceInfo("ConvertRawToDump failed");
            } else {
                hr = S_OK;
            }
        } else {
            hr = HRESULT_FROM_WIN32(GetLastError());
            TraceHRESULT("GetProcAddress(ConvertRawToDump)", hr);
        }

        FreeLibrary(hRaw2Dump);
    } else {
        hr = HRESULT_FROM_WIN32(GetLastError());
        TraceHRESULT("LoadLibraryEx(raw2dump.dll) failed\n", hr);
    }
    return hr;
}


bool ShouldConvertRaw2WindowsDump(void)
{
    HKEY hKey;
    DWORD rc;
    DWORD val;
    DWORD vallen;
    bool ret = false;

    //
    // If HKLM\System\CurrentControlSet\Control\CrashControl\Raw2DumpEnabled is non-zero, then
    // attempt to use raw2dump.dll to perform the conversion.
    // The absence of this registry value implies that the feature is disabled.
    //
    rc = RegOpenKeyExW(HKEY_LOCAL_MACHINE, CRASHCONTROL_PATH, 0, KEY_READ, &hKey);
    if (rc == ERROR_SUCCESS) {
        vallen = sizeof(val);
        val = 0;
        rc = RegQueryValueExW(hKey, CRASHCONTROL_RAW2DUMP_ENABLED, nullptr, nullptr, (LPBYTE)&val, (LPDWORD)&vallen);
        if (rc == ERROR_SUCCESS) {
            if (val != 0) {
                ret = true;
            }
        }

        RegCloseKey(hKey);
    }

    return ret;
}


HRESULT
SubmitOfflineCrashDump(
_In_ PDMP_CONTEXT Context
)
/*++

Routine Description:
    This function does the following:-
    (1) Locate the raw dump.
    (2) Retrieve and validate RAW_DUMP_HEADER.
    (3) Validate the RAW_DUMP_HEADER.SectionTable.
    (4) Locate DUMP_HEADER.
    (5) Validate DUMP_HEADER
    (6) Do SV Specific processing
    (7) Create a Generic watson report
    (8) Attach the raw file, and raw info file
    (9) Submit the watson report

Arguments:
    Context

Return Value
    HRESULT

--*/
{
    HRESULT      result = E_FAIL;
    bool         ValidateResult = FALSE;
    SvSpecific   *SvSpecificData = nullptr;
    SYSTEM_INFO  sysInfo;


    //
    // Dump is expected. Find the dedicated partition and get a handle to it.
    //
    TraceInfo("=========== Raw dump is expected. Determine location.============");
    result = LocateAndCopyRawDump(Context);
    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to locate the Raw dump.", result);
        goto Exit;
    }

    Context->DumpHeaderAddress.QuadPart = 0;
    Context->DumpHeader = (PDUMP_HEADER32)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DUMP_HEADER32));
    if (Context->DumpHeader == nullptr) {
        result = E_OUTOFMEMORY;
        TraceHRESULT("Failed to allocate heap for dump header exiting",result);
        goto Exit;
    }

    TraceInfo("=========== Found the partition. Checking if there's a valid RAW_DUMP_HEADER.===========");
    ValidateResult = FALSE;
    result = VerifyRawDumpHeader(Context, &ValidateResult);
    if (SUCCEEDED(result) && ValidateResult == FALSE) {
        result = E_BAD_DATA;
    }

    if(!SUCCEEDED(result)){
        TraceHRESULT("Raw Dump Partition header is invalid.", result);
        goto Exit;
    }

    //
    // If we are dealing with SD card dumps, will will have to collate all
    // sections into a single file rawdump.bin so that the rest of the logic
    // just fits in.
    //
    if (Context->SBLDumpLocation == SBL_DUMP_LOCATION_SD) {
        result = CollateSDRawDumps(Context);
        if (!SUCCEEDED(result)) {
            TraceInfo("Failed to collate rawdump section files.");
            result = E_BAD_DATA;
            goto Exit;
        }

    }

    TraceInfo("=========== Found a valid dump header. Validating the section table.===========");
    ValidateResult = FALSE;
    result = VerifyRawDumpSectionTable(Context, &ValidateResult);
    if (SUCCEEDED(result) && ValidateResult == FALSE) {
        result = E_BAD_DATA;
    }
    if(!SUCCEEDED(result)){
        TraceHRESULT("Raw Dump Partition Section table is invalid.", result);
        goto Exit;
    }

    TraceInfo("=========== Got a valid section table. Building a memory based on DDR sections.===========");
    result = BuildDDRMemoryMap(Context);
    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to Build DDR Memory Map.", result);
        goto Exit;
    }

    TraceInfo("=========== Built DDR Sections. Getting dump instance.===========");
    result = GetDumpInstance(&(Context->DumpInstance));
    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to get the dump instance from the registry, using default value of zero.", result);
    }

    TraceInfo("===========Proceeding towards SV specific processing.===========");
    //
    // Find out which architecture this code is running on.
    //
    GetSystemInfo(&sysInfo);
    Context->Is64Bit = FALSE;
    switch (sysInfo.wProcessorArchitecture)
    {
        case PROCESSOR_INTEL_386:
        case PROCESSOR_ARCHITECTURE_INTEL:
            SvSpecificData = new (std::nothrow) Intelx86(Context);
            break;

        case PROCESSOR_ARCHITECTURE_ARM64:
            Context->Is64Bit = TRUE;
        case PROCESSOR_ARCHITECTURE_ARM:
            SvSpecificData = new (std::nothrow) QCom32(Context);
            break;

        default:
            TraceInfo("Unidentified architecture");
            goto Exit;
    }

    if (SvSpecificData == nullptr) {
        result = E_OUTOFMEMORY;
        TraceHRESULT("Cannot allocate SV specific data", result);
        goto Exit;
    }

    result = SvSpecificData->ProcessSVSpecific();
    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to process SV specific data.", result);
        goto Exit;
    }

    TraceInfo("=========== Diag Buffer is built. Building the Bugcheck parameters now.===========");
    SvSpecificData->BuildBugCheckParams();
    TraceInfo("Bugcheck parameters built. Making raw header info xml file");

    result = SvSpecificData->BuildInfoFile();
    if (!SUCCEEDED(result)) {
        TraceHRESULT("Failed to write raw dump xml info file.", result);
        goto Exit;
    }

    //
    // If the file is on SD we already have created the rawdump.bin file.
    //
    if (Context->SBLDumpLocation != SBL_DUMP_LOCATION_SD) {
        TraceInfo("Creating File on the Disk to be uploaded to the Watson Backend");
        result = CreateRawDumpDotBin(Context);
        if (!SUCCEEDED(result)) {
            TraceHRESULT("Failed to create a temporary rawdump.bin on the disk.", result);
            goto Exit;
        }

    }

    //
    // Attaching device specific information at the end of the rawdump.
    //
    TraceInfo("=========== Attaching device specific info to the rawdump ===========");
    result = AppendDeviceSpecificInfoToRawDump(Context, SvSpecificData);
    if (!SUCCEEDED(result)){
        TraceHRESULT("Failed to device specific info to rawdump.bin", result);
        goto Exit;
    }

    //
    // Use raw2dump.dll to generate the Windows dump if configured to do so.
    //
    if (ShouldConvertRaw2WindowsDump()) {
        result = ConvertRaw2WindowsDump(Context);
        if (SUCCEEDED(result)) {
            TraceInfo("Successfully converted rawdump to Windows dump\n");
        } else {
            TraceHRESULT("ConvertRaw2WindowsDump failed", result);
        }

        goto Exit;
    }

    Context->hDisk.Close();
    //
    // Attach the info file and the raw dump file to WER and submit the report.
    //
    TraceInfo("=========== Submitting the report to WER ===========");
    TraceMetric("BEGIN:Submit RawDump to WER");

    //
    // closing the log file becuase we will have to upload the file.
    //
    CloseLogFile();

    result = SubmitReportToWER(Context);
    if (!SUCCEEDED(result)) {
        TraceMetric("FAILED:Submit RawDump to WER");
        TraceHRESULT("SubmitReportToWER failed", result);
        goto Exit;
    }
    else
    {
        TraceMetric("DONE:Submit RawDump to WER");
    }

Exit:
    if (SvSpecificData != nullptr) {
        delete SvSpecificData;
    }

    CleanupContext(Context);
    return result;
}


HRESULT
LocateAndCopyRawDump(
    _Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description :
    This function attempts to first locate SVRawDump partition. If it finds the partition,
    it copies the content of the partition into a new file called rawdump.bin.
    If it doesn't find the partition,then it tries to find the rawdump.bin in either the
    root directory of the SD card or in one of the known folders.

Arguments :
    Context - Dump context.

Return Value :
    HRESULT

--*/
{
    HRESULT result = E_NOTFOUND;
    Context->SBLDumpLocation = SBL_DUMP_LOCATION_INVALID;
    Context->SBLDumpFormat = SBL_DUMP_FORMAT_INVALID;

#ifndef SKIP_SVRAWDUMP

    //
    // First look for the raw dump partition.
    //
    TraceInfo("Search for SVRawDump.");

    result = FindDumpPartition(Context);
    if (!SUCCEEDED(result))
    {
        TraceHRESULT("Failed to find SVRawDump.", result);
    }
    else
    {
        //
        // Found the raw dump partition. Record the data and exit.
        //
        TraceInfo("SvRawDump found.");

        Context->SBLDumpLocation = SBL_DUMP_LOCATION_EMMC;
        Context->SBLDumpFormat = SBL_DUMP_FORMAT_RAW;
        goto Exit;

    }
#endif

    //
    // Could not find SvRawDump. Now look for rawdump.bin
    // on the SD card.
    //
    TraceInfo("Search for rawdump.bin.");
    //result = FindRawDumpDotBin(Context);
    result = FindNewestRawDumpDotBinPath(Context);
    if (!SUCCEEDED(result)) {
        TraceHRESULT("Rawdump.bin not found.", result);
        goto Exit;
    }

    //
    // Found the rawdump.bin.
    //
    TraceInfo("Rawdump.bin found.");
    Context->hDisk.Close();
    Context->hDisk.Open(Context->RawDumpOnSDPath);
    Context->SBLDumpLocation = SBL_DUMP_LOCATION_SD;
    Context->SBLDumpFormat = SBL_DUMP_FORMAT_FILE;

Exit:
    return result;
}

HRESULT
FindDumpPartition(
_Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

    This function is used to find the raw dump partition on the disk.
    it will enumerate all GPT partitions and compare the GUID RAWDUMP_GUID

Arguments:

    Context - Pointer to DmpContext

Return Value:

    HRESULT

--*/
{
    // Start out with the default crash dump path, later check for the "CrashDump" partition and adjust
    WCHAR                           CrashDumpPath[MAX_PATH] = {DEFAULT_CRASH_DUMP_PATH};
    SYSTEM_DEVICE_INFORMATION       DeviceInfo = {0};
    ULONG                           pathStrLen = 0;
    UINT32                          DiskIndex = 0;
    PDRIVE_LAYOUT_INFORMATION_EX    DriveLayout = nullptr;
    NTSTATUS                        status = STATUS_UNSUCCESSFUL;
    HRESULT                         result = E_FAIL;

    //
    // Get the number of Disks in the system
    //
    status = NtQuerySystemInformation(SystemDeviceInformation, &DeviceInfo, sizeof(SYSTEM_DEVICE_INFORMATION), 0);
    if (!NT_SUCCESS(status)){
        TraceNTSTATUS("FindDumpPartition NtQuerySystemInformation", status);
        result = HRESULT_FROM_NT(status);
        goto Exit;
    }
    else if (0 == DeviceInfo.NumberOfDisks)
    { // No disks detected
        TraceInfo("ERROR : FindDumpPartition() : NtQuerySystemInformation found no disks");
        goto Exit;
    }

    while (DiskIndex < DeviceInfo.NumberOfDisks)
    { // Loop through the disks looking for partitions
        DEVICE_IO hDevice;

        if ( FAILED(result = hDevice.Open(DiskIndex)) )
        {
            if (hDevice.GetError() == DEVICE_IO::IO_ERROR_INVALID_HANDLE)
            { // If open resulted in invalid handle, try next disk
                DiskIndex++;
                continue;
            }
            else
            { // Any other error is a serious failure, cannot continue
                result = HRESULT_FROM_WIN32(GetLastError());
                TraceHRESULT("FindDumpPartition Create File Failed.", result);
                TraceString("Device" , DiskIndex);
                goto Exit;
            }

        }

        if ( (0 == DiskIndex) && SUCCEEDED(result = hDevice.SetPartition(DEVICE_IO::CRASHDUMP)) )
        { // If there is a CrashDump partition (optional), adjust the path on Drive C:
            TraceInfo1("Found the CrashDump Partition on Drive", "Number", DiskIndex);
            wcscpy_s( CrashDumpPath, _countof(CrashDumpPath), LEGACY_CRASH_DUMP_PATH);
        }

        if ( SUCCEEDED(result = hDevice.SetPartition(RAWDUMP_GUID))
             && SUCCEEDED(result = hDevice.Close())                     // Close the current device
             && SUCCEEDED(result = Context->hDisk.Open(DiskIndex))      // Open it in the context
             && SUCCEEDED(result = Context->hDisk.SetPartition(RAWDUMP_GUID))   // Set the partition
           )
        { // If this drive has and SVRawDumpPartition, we are done searching.
            break;
        }

        hDevice.Close();
        DiskIndex++;
    }

    if (ERROR_SUCCESS != result)
    { // If we did not find the SvRawDump parition, exit without setting "RawDumpPath" and "RawDumpInfoPath"
        TraceInfo("The RAW_DUMP Partition was not found");
        goto Exit;
    }

    if( FALSE == CreateDirectoryW( CrashDumpPath, NULL ) )
    { // Failure is a zero return value
        if( ERROR_ALREADY_EXISTS != GetLastError() )
        { // If dir error is other than "already exisits", bail and return the error
            result = HRESULT_FROM_WIN32(GetLastError());
            TraceInfo("Error: Cannot create the CrashDump Directory.");
            goto Exit;
        }
    }

    pathStrLen = 1 + (ULONG)wcslen(CrashDumpPath) + _countof(RAWDUMP_BIN_FILE);    // Number of elements in the string plus null
    Context->RawDumpPath = (PWCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (pathStrLen * sizeof WCHAR));
    if ( nullptr == Context->RawDumpPath )
    {
        TraceInfo("Could not allocate memory for Context->RawDumpPath ");
        result = E_ALLOCATION_ERROR;
        goto Exit;
    }

    wcscpy_s(Context->RawDumpPath, pathStrLen, CrashDumpPath);
    wcscat_s(Context->RawDumpPath, pathStrLen, RAWDUMP_BIN_FILE);

    pathStrLen = 1 + (ULONG)wcslen(CrashDumpPath) + _countof(RAWDUMP_INFO_FILE);    // Number of elements in the string plus null
    Context->RawDumpInfoPath = (PWCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (pathStrLen * sizeof WCHAR));
    if ( nullptr == Context->RawDumpInfoPath )
    {
        TraceInfo("Could not allocate memory for  Context->RawDumpInfoPath");
        result = E_ALLOCATION_ERROR;
        goto Exit;
    }

    wcscpy_s(Context->RawDumpInfoPath, pathStrLen, CrashDumpPath);
    wcscat_s(Context->RawDumpInfoPath, pathStrLen, RAWDUMP_INFO_FILE);

Exit:
    if (DriveLayout != nullptr) {
        HeapFree(GetProcessHeap(), 0, DriveLayout);
    }

    return result;
}



HRESULT
FindRawDumpDotBin(
    _Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function finds the rawdump.bin file expected to be present in the root
of the disk drive corresponding to the SD card or any other removable media.
TODO: The function implemetentation is incomplete.

Arguments:

Context - Pointer to DmpContext

Return Value:

HRESULT.

--*/
{
    WCHAR DevicePath[MAX_PATH];
    SYSTEM_DEVICE_INFORMATION DeviceInfo;
    DISK_GEOMETRY diskGeometry;

    HANDLE hDevice;
    UINT32 DiskIndex;
    NTSTATUS Status;
    HRESULT result = E_NOTFOUND;
    hDevice = INVALID_HANDLE_VALUE;
    bool removableDiskFound = FALSE;

    //
    // Get the number of Disks in the system
    //
    Status = NtQuerySystemInformation(
                SystemDeviceInformation,
                &DeviceInfo,
                sizeof(SYSTEM_DEVICE_INFORMATION),
                NULL);
    if (!NT_SUCCESS(Status)) {
        result = HRESULT_FROM_NT(Status);
        TraceNTSTATUS("NtQuerySystemInformation", Status);
        goto EXIT;
    }

    //
    // Loop through the disks
    //
    DiskIndex = 0;
    while (DiskIndex < DeviceInfo.NumberOfDisks) {
        ULONG ReturnedLength = 0;
        //
        // Create the Device Path used for getting the handle
        //
        StringCchPrintfW(DevicePath,
            MAX_PATH,
            L"\\\\.\\PhysicalDrive%d",
            DiskIndex);

        //
        // Create a Handle to the Hard Disk
        //
        hDevice = CreateFileW(DevicePath,
            FILE_GENERIC_READ | FILE_GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        if (hDevice == INVALID_HANDLE_VALUE){
            result = HRESULT_FROM_WIN32(GetLastError());
            TraceHRESULT("Create File Failed.", result);
            TraceString("DevicePath", DevicePath);
            goto EXIT;
        }

        //
        // Get the Disk Layout Structure.  This doesn't return needed buffer size, so keep trying.
        // Starting with size of disk and 5 partitions.
        // Don't care about skipping offline Dynamic disks.
        //
        if (DeviceIoControl(hDevice,
                            IOCTL_DISK_GET_DRIVE_GEOMETRY,
                            nullptr,
                            0,
                            &diskGeometry,
                            sizeof(diskGeometry),
                            &ReturnedLength,
                            nullptr) == FALSE) {

            TraceWIN32("PartitionInfo Get Drive Layout IOCTL failed", GetLastError());
            CloseHandle(hDevice);
            continue;
        }

        // IOCTL failed, try next disk
        if (hDevice == INVALID_HANDLE_VALUE){
            DiskIndex++;
            continue;
        }

        if (diskGeometry.MediaType == RemovableMedia) {
            TraceInfo1("Found a removable Disk", "DiskIndex", DiskIndex);
            removableDiskFound = TRUE;

            //
            // keep the handles opened because it will be used to find out the rawdump.bin
            // TODO: add the following code.
            // (1) find if there is any valid rawdump.bin file.
            // (2) if the disk is bitlocked? not sure what to do then.
            //

            //
            // Find the rawdump.bin that we should be working on.
            //
            result = FindNewestRawDumpDotBinPath(Context);
            break;

        }
        else {
            TraceInfo1("Physical Drive not a removable disk", "DiskIndex", DiskIndex);
            CloseHandle(hDevice);
            hDevice = INVALID_HANDLE_VALUE;
            DiskIndex++;
        }
    }

EXIT:
    if(hDevice != nullptr) {
        CloseHandle(hDevice);
    }
    return result;
}

HRESULT
FindNewestRawDumpDotBinPath(
    _Inout_ PDMP_CONTEXT Context
    )
/*++

Routine Description:

This rotine loops through the disk and provides the handle and path of rawdump.bin file
in the most recent not-processed folder.

Arguments:
    Context - Pointer to DmpContext

Return Value:

HRESULT

--*/
{
    DEVICE_IO fileHandle;
    WCHAR rawdumpFilePath[MAX_PATH];
    WCHAR rawdumpFolderPath[MAX_PATH];
    WCHAR driveName[] = L"A:\\";
    DWORD drives;
    HRESULT hr;
    BOOL found = FALSE;
    BOOL dumpHasError = FALSE;
    BOOL alreadyProcessed = FALSE;
    CHAR rawdumpFolderPathA[MAX_PATH];

    Context->RawDumpOnSDPath = (PWCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, MAX_PATH);
    if (!Context->RawDumpOnSDPath) {
        TraceInfo("FindNewestRawDumpDotBinPath: Could not allocate memory ");
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    //
    // Get all the logical drives on the device.
    //
    drives = GetLogicalDrives();
    TraceInfo1("Disk Drives", "Bitmask", drives);
    for (int i = 0; i < 26; i ++) {
        if((drives & (1 << i))) {
           driveName[0] = (WCHAR)(L'A' + i);
           TraceString("Drive name", driveName);
           if(DRIVE_REMOVABLE == GetDriveTypeW(driveName)){
                //
                // We find the fist removable drive and expect it to be the place where
                // rawdump will be present.
                //
               hr = StringCchPrintfW(rawdumpFolderPath, MAX_PATH, L"%s", driveName);
                if(!SUCCEEDED(hr)){
                    goto Exit;
                }
                break;
           }
        } // if((drives & (1 << i)))
     } // for (int i=0; i<26; i++)


    //
    // Check if rawdump.bin is at the root.
    // Create the Device Path used for getting the handle
    //
    hr = StringCchPrintfW(rawdumpFilePath,
                    MAX_PATH,
                    L"%s\\%s",
                    rawdumpFolderPath, RAW_DUMP_HEADER_FILE_NAME);

    if(!SUCCEEDED(hr)) {
        goto Exit;
    }

    if (SUCCEEDED(hr = fileHandle.Open(rawdumpFilePath)))
    {
        TraceInfo("The rawdump file was found in the root directory");
        Context->SDCardRawDumpDotBinFileHandle = fileHandle;
        memcpy(Context->RawDumpOnSDPath, rawdumpFilePath, MAX_PATH);
        goto Exit;
    }
    TraceNTSTATUS("Failed to open rawdump.bin in the root directory", GetLastError());


    //
    // This means we are running with SBL that has multi-dump implemented.
    //
    for (UINT32 index = RAW_DUMP_START_FOLDER_NUMBER; index < RAW_DUMP_MAX_FOLDER_COUNT; index++)
    {
        //
        // rawdumpFolderPath = driveName\1\ or driveName\2\ ...
        //
        hr = StringCchPrintfW(rawdumpFolderPath,
                    MAX_PATH,
                    L"%s\\%d",
                    driveName, index);

        if(!SUCCEEDED(hr)) {
            goto Exit;
        }

        hr = StringCchPrintfW(rawdumpFilePath,
            MAX_PATH,
            L"%s\\%s",
            rawdumpFolderPath, RAW_DUMP_HEADER_FILE_NAME);

        wcstombs(rawdumpFolderPathA, rawdumpFolderPath, MAX_PATH);
        TraceInfo(rawdumpFolderPathA);

        if (FAILED(fileHandle.Open(rawdumpFilePath)))
        {
            continue;
        }

        //
        // SBL will leave errfile.txt in the folder if there were errors so skip the folder
        // if we found the file.
        //
        hr = CheckIfDumpHasError(rawdumpFolderPath, &dumpHasError);
        if (!SUCCEEDED(hr)) {
            TraceHRESULT("Failed to check if dump has errors", hr);
        }
        if (dumpHasError) {
            TraceInfo("Dump appears to have errors");
            continue;
        }

        //
        // we will create the done file so skip if the file is already present.
        //
        hr = CheckIfDumpAlreadyProcessed(rawdumpFolderPath, &alreadyProcessed);
        if (!SUCCEEDED(hr)) {
            TraceHRESULT("Failed to check if dump file was already processed", hr);
        }
        if (alreadyProcessed) {
            TraceInfo("Dump appears to heve been processed");
            continue;
        }
        found = true;
        TraceString("Raw dump file path", rawdumpFilePath);
        break;
    }

    if (found) {
        hr = MarkDumpProcessed(rawdumpFolderPath);
        if (!SUCCEEDED(hr)) {
            TraceHRESULT("Failed to mark the dump file processed", hr);
        }
    }
    else {
        hr = HRESULT_FROM_NT(STATUS_NOT_FOUND);
        goto Exit;
    }

    Context->SDCardRawDumpDotBinFileHandle = fileHandle;
    memcpy(Context->RawDumpOnSDPath, rawdumpFilePath, MAX_PATH);
    Context->diskoffset.QuadPart = 0;

Exit:
    return hr;
}

HRESULT
CheckIfDumpHasError(
    _In_ PWCHAR rawdumpFolderPath,
    _Inout_ PBOOL hasError
)
/*++
    Routine Description :

    This function is only for multi - dump scenario. A dump is considered to have errors if
    the errfile.txt file is present in the dump folder.

Arguments :
    rawdumpFolderPath - Path to the dump folder.
    hasError - TRUE if errfile.txt was found.FALSE otherwise.

Return Value :
    HRESULT

--*/
{
    HRESULT hr;
    WCHAR ErrorFilePath[MAX_PATH];
    DEVICE_IO errorFileHandle;
    *hasError = FALSE;

    if ( FAILED(hr = StringCchPrintfW(ErrorFilePath,
        MAX_PATH,
        L"%s\\%s",
        rawdumpFolderPath, SBL_DUMP_ERROR_FILE)))
    {
        TraceHRESULT("CheckIfDumpHasError:StringCchPrintfW failed", hr);
    }

    else if ( FAILED(hr = errorFileHandle.Open(ErrorFilePath)))
    {
        TraceInfo("Cannot find errfile.txt");
    }
    else
    {
        *hasError = TRUE;
        TraceInfo("errfile.txt FOUND");
        errorFileHandle.Close();
    }

    return hr;
}

HRESULT
CheckIfDumpAlreadyProcessed(
    _In_ PWCHAR rawdumpFolderPath,
    _Inout_ PBOOL alreadyProcessed
)
/*++
Routine Description :

    This function is only for multi - dump scenario.A dump is considered processed if
    the wpdone.txt file is present in the dump folder.

Arguments:

    rawdumpFolderPath - Path to the dump folder.
    alreadyProcessed - TRUE if wpdone file was found. FALSE otherwise.

Return Value:
    HRESULT

--*/
{
    HRESULT hr;
    WCHAR doneFilePath[MAX_PATH];
    DEVICE_IO doneFileHandle;

    *alreadyProcessed = FALSE;

    if( FAILED(hr = StringCchPrintfW(doneFilePath,
            MAX_PATH,
            L"%s\\%s",
            rawdumpFolderPath, MULTI_DUMP_DONE_FILE)) )
    {
        TraceHRESULT("CheckIfDumpAlreadyProcessed:StringCchPrintfW failed", hr);
    }

    else if (FAILED(hr = doneFileHandle.Open(doneFilePath)))
    {
        TraceInfo("Cannot find done file");
    }
    else
    {
        *alreadyProcessed = TRUE;
        TraceInfo("Done file FOUND");
        doneFileHandle.Close();
    }

    return hr;
}

HRESULT
MarkDumpProcessed(
    _In_ PWCHAR rawdumpFolderPath
)
/*++
Routine Description:

    This function is only for multi-dump scenario. This function creates a
    wpdone.txt file in the dump folder to indicate that the dump has been processed.

Arguments:
    rawdumpFolderPath - Path to the dump folder.

Return Value:
    HRESULT
--*/
{
    HRESULT hr = E_FAIL;
    WCHAR markedFilePath[MAX_PATH];
    DEVICE_IO markedFileHandle;

    if (FAILED(hr = StringCchPrintfW(markedFilePath,
        MAX_PATH,
        L"%s\\%s",
        rawdumpFolderPath, MULTI_DUMP_DONE_FILE)))
    {
        TraceHRESULT("CheckIfDumpAlreadyProcessed:StringCchPrintfW failed", hr);
    }
    else if( FAILED(hr = markedFileHandle.Open(markedFilePath)) )
    {
        TraceInfo("Cannot create done file");
    }
    else
    {
        markedFileHandle.Close();
    }

    return hr;
}

HRESULT
CollateSDRawDumps(
_Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description :

This function is used to find the raw dump partition on the disk.
it will enumerate all GPT partitions and compare the GUID RAWDUMP_GUID

Arguments :

Context - Pointer to DmpContext

Return Value :

HRESULT

--*/
{
    HRESULT hr = E_FAIL;
    ULONGLONG   currentOffset = 0;
    size_t      dwBytesWritten = 0;
    WCHAR       rawDumpFolder[MAX_PATH];

    if( FALSE == CopyFileW(Context->RawDumpOnSDPath, Context->RawDumpPath, FALSE) )
    {
        hr = HRESULT_FROM_NT(GetLastError());
        TraceHRESULT("Cannot copy rawdump.bin to  a new location.", hr);
    }
    else if ( FAILED(hr = Context->hDisk.Open(Context->RawDumpPath)))
    {
        TraceHRESULT("Cannot open destination file for processing", hr);
    }
    else if ( FAILED(hr = Context->hDisk.SetPos(0))
              || FAILED(hr = Context->hDisk.Write((PCHAR)Context->RawDumpHeader, Context->RawDumpTableSize, &dwBytesWritten))
              || FAILED(Context->hDisk.GetPos(&currentOffset))
            )
    { // Failed to write the current header and sections table, at the file's beginning
        TraceHRESULT("ERROR: Write() - Write Raw Dump Header Tables failed", hr);
    }
    else
    { // Append the sections
        //
        // get directory path for section files.
        //
        memcpy(rawDumpFolder, Context->RawDumpOnSDPath, MAX_PATH);
        PathCchRemoveFileSpec(rawDumpFolder, MAX_PATH);

        for (UINT32 sectionIndex = 0; sectionIndex < Context->RawDumpHeader->SectionsCount; sectionIndex++)
        {
            DEVICE_IO   sectionFile;
            WCHAR       sectionName[RAW_DUMP_SECTION_HEADER_NAME_LENGTH + 1];
            WCHAR       sectionPath[MAX_PATH];

            mbstowcs(sectionName, (LPCSTR)&(Context->RawDumpHeader->SectionTable[sectionIndex].Name[0]), RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
            TraceString("Opening File", sectionName);
            if ( FAILED(hr = StringCchPrintfW(sectionPath,
                        MAX_PATH,
                        L"%s\\%s",
                        rawDumpFolder, sectionName)) )
            {
                TraceHRESULT("StringCchPrintfW: Skipping the section", hr);
                continue;
            }
            else if ( FAILED(hr = sectionFile.Open(sectionPath)) )
            {
                TraceHRESULT("Cannot open the file, not appending this for further processing", hr);
                continue;
            }
            else
            { // update the section table in the rawdump header.
                ULONGLONG fileSize = 0;

                Context->RawDumpHeader->SectionTable[sectionIndex].Offset = currentOffset;

                // append the file to our rawdump.bin
                AppendFile(&Context->hDisk, &sectionFile, &fileSize);
                currentOffset += fileSize;
                sectionFile.Close();
            }

        }

        //
        // We we are done appending, we should update the rawdump header.
        //
        if ( FAILED(hr = Context->hDisk.SetPos(0)) )
        {
            TraceHRESULT("ERROR: SetPos() - Append failure", hr);
        }
        else if ( FAILED(hr = Context->hDisk.Write((PCHAR)Context->RawDumpHeader, Context->RawDumpTableSize, &dwBytesWritten)) )
        {
            TraceHRESULT("ERROR: Write() - Append failure", hr);
        }

    }

    return hr;
}

HRESULT
AppendFile(
    _In_ DEVICE_IO      *destinationFile,
    _In_ DEVICE_IO      *sourceFile,
    _Inout_ PULONGLONG  bytesAppended
)
{
    HRESULT hr = E_FAIL;

    if (nullptr != bytesAppended )
    {
        *bytesAppended = 0;
    }

    if ( DEVICE_IO::IO_OK != sourceFile->GetError() )
    {
        TraceHRESULT("Source file not ready", hr);
    }
    else if ( DEVICE_IO::IO_OK != destinationFile->GetError() )
    {
        TraceHRESULT("Destination file not ready", hr);
    }
    else if ( SUCCEEDED(hr = sourceFile->SetPos(0)) &&
              SUCCEEDED(hr = destinationFile->SetPos(0))
            )
    {
        ULONGLONG   bytesToCopy = sourceFile->GetCurrentFileSize();

        while ( bytesToCopy > 0 )
        {
            size_t  stBytesRead = 0;
            size_t  stBytesWritten = 0;
            CHAR    buff[COPY_BUFFER_SIZE];

            if (FAILED(hr = sourceFile->Read(buff, sizeof(buff), &stBytesRead)))
            { // Read failed
                TraceHRESULT("FAILED: read of source file", hr);
                break;
            }
            else if (sizeof(buff) != stBytesRead)
            { // Read an unexpected number ofbytes
                TraceInfo2("WARNING: read of source file returned fewer bytes than requested",
                           "Expected", sizeof(buff),"Actual", stBytesRead);
            }
            else if (0 == stBytesRead)
            {
                TraceInfo("FAILED: read of source file returned zero bytes");
                break;
            }
            else if (FAILED(hr = destinationFile->Write(buff, stBytesRead, &stBytesWritten)))
            { // Write failed
                TraceHRESULT("FAILED: write of destination file", hr);
                break;
            }
            else if (0 == stBytesWritten)
            { // Wrote zero bytes
                TraceInfo("FAILED: write of destination file returned zero bytes");
                break;
            }
            else
            {
                bytesToCopy -= stBytesWritten;
                if (stBytesRead != stBytesWritten)
                { // Wrote an unexpected number of bytes
                    TraceInfo2("WARNING: read of source file returned fewer bytes than requested",
                               "Expected", sizeof(buff),"Actual", stBytesRead);
                }
            }

        }

        if (nullptr != bytesAppended )
        {
            *bytesAppended = sourceFile->GetCurrentFileSize() - bytesToCopy;
        }

    }

    return hr;
}

HRESULT
VerifyRawDumpHeader(
    _Inout_ PDMP_CONTEXT Context,
    _Inout_ bool* IsRawDumpHeaderValid
)
/*++

Routine Description:

This function validates the RAW_DUMP_HEADER. The following are checked.

1. RAW_DUMP_HEADER.Signature
2. RAW_DUMP_HEADER.Version
3. RAW_DUMP_HEADER.Flags
4. RAW_DUMP_HEADER.DumpSize
5. RAW_DUMP_HEADER.SectionCount

If things check out, it will allocate and read the complete dump table
to memory.

Arguments:

Context - Pointer to DmpContext

IsRawDumpHeaderValid - Returned to caller. TRUE if RAW_DUMP_HEADER is valid. FALSE if
no valid dump found.

Return Value:

HRESULT

--*/
{
    HRESULT             result = E_FAIL;
    RAW_DUMP_HEADER     dumpHeader;
    size_t              bytesProcessed;

    //
    // Read in the header and validate the fields.
    //

    if ( FAILED(result = Context->hDisk.SetPos(0))
         || FAILED(result = Context->hDisk.Read((PCHAR)&dumpHeader, sizeof(dumpHeader), &bytesProcessed))
       )
    {
        TraceHRESULT("Read RAW_Dump Partition failed.", result);
        goto Exit;
    }

    //
    // Check the signature.
    //
    if (dumpHeader.Signature != RAW_DUMP_HEADER_SIGNATURE)
    {
        TraceExpectedActual("Signature validation failed", RAW_DUMP_HEADER_SIGNATURE, dumpHeader.Signature);
        result = E_UNEXPECTED;
        goto Exit;
    }

    //
    // Check the version.
    //
    if (dumpHeader.Version != RAW_DUMP_HEADER_VERSION) {
        TraceExpectedActual("Version validation failed", RAW_DUMP_HEADER_VERSION, dumpHeader.Version);
        result = E_NOTIMPL;
        goto Exit;
    }

    //
    // Check for expected flags.
    //
    if (!ValidateRawDumpHeaderFlags(dumpHeader.Flags)) {
        result = E_UNEXPECTED;
        TraceHRESULT1("Invalid Dump Header flags", "Flags", dumpHeader.Flags, result);
        goto Exit;
    }

    //
    // Check dump size. We don't know before hand the size of the dump
    // so we'll check for a non-zero value.
    //
    if (dumpHeader.DumpSize == 0) {
        result = E_UNEXPECTED;
        TraceHRESULT("Dumpsize is 0", result);
        goto Exit;
    }

    //
    // There should be at least one section.
    //
    if (dumpHeader.SectionsCount == 0) {
        result = E_UNEXPECTED;
        TraceHRESULT("SectionsCount is 0", result);
        goto Exit;
    }

    //
    // Header looks good and looks like we have a dump.
    // Get the table size so as to allocate memory for rawdump header.
    //
    Context->RawDumpTableSize = sizeof(RAW_DUMP_HEADER) +
        ((dumpHeader.SectionsCount - 1) *
        (sizeof(RAW_DUMP_SECTION_HEADER)));

    //
    // Need to free up this space when we exit.
    //
    Context->RawDumpHeader = (PRAW_DUMP_HEADER)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Context->RawDumpTableSize);
    if (Context->RawDumpHeader == nullptr) {
        result = E_OUTOFMEMORY;
        TraceHRESULT1("Failed to allocate required amount of memory for dump table.", "Required Memory", Context->RawDumpTableSize, result);
        goto Exit;
    }

    if ( FAILED(result = Context->hDisk.SetPos(0))
        || FAILED(result = Context->hDisk.Read((PCHAR)Context->RawDumpHeader, Context->RawDumpTableSize, &bytesProcessed))
       )
    {
        TraceHRESULT("Read RAW_Dump Partition failed.", result);
        goto Exit;
    }

    *IsRawDumpHeaderValid = TRUE;
    TraceInfo("Dump header validated.");
    DumpRawDumpHeader(Context->RawDumpHeader);

Exit:
    return result;
}


HRESULT
VerifyRawDumpSectionTable(
    PDMP_CONTEXT Context,
    bool* IsRawDumpSectionTableValid
)
/*++

Routine Description:

This function validates the RAW_DUMP_HEADER.SectionTable. The following are checked.

1. At least one DDR section.
2. No sections with unrecognized section type.
3. SectionsCount matches number of valid sections.
4. Only the last section should have the RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE flag.

Arguments:

Context - Pointer to DmpContext

IsRawDumpSectionTableValid - Returned to the caller to indicate whether we have a good
section table or not.

Return Value:

HRESULT


--*/
{
    PRAW_DUMP_SECTION_HEADER sectionTable = nullptr;
    PRAW_DUMP_HEADER         dumpHeader = nullptr;
    HRESULT                  result = E_FAIL;

    //
    // Initialize the various counters.
    //
    Context->DDRSectionCount = 0;
    Context->SVSectionCount = 0;
    Context->CPUContextSectionCount = 0;
    Context->InvalidSectionCount = 0;
    Context->DDRSectionFragmentationCount = 0;
    Context->DDRSectionsOverlapCount = 0;
    Context->MissingFlagsCount = 0;
    Context->InsufficientStorageSectionsCount = 0;
    Context->InvalidVersionCount = 0;
    Context->LargestSVSpecificSectionSize = 0;

    dumpHeader = Context->RawDumpHeader;
    sectionTable = (PRAW_DUMP_SECTION_HEADER)&dumpHeader->SectionTable[0];

    for (UINT32 index = 0; index < dumpHeader->SectionsCount; index++)  {
        //
        // Check version
        //
        if (sectionTable[index].Version != RAW_DUMP_SECTION_HEADER_VERSION) {
            TraceWarn2("A Section has invalid version.", "Index", index,"Version",sectionTable[index].Version);
            Context->InvalidVersionCount++;
        }

        //
        // Has the right flags been set?
        //
        if (!ValidateRawDumpHeaderFlags(sectionTable[index].Flags))
        {
            TraceWarn1("Expected flags are not set", "Flags",sectionTable[index].Flags);
            Context->MissingFlagsCount++;
        }

        //
        // Check if RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE flag is
        // set. If so, it should only be set for the last section.
        //
        if ((sectionTable[index].Flags & RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE) ==
            RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE) {
            if (index != (dumpHeader->SectionsCount - 1)) {
                TraceWarn1("A Section has an insufficient storage flag and it is not the last section!", "Index", index);
                Context->InsufficientStorageSectionsCount++;
            }
            else  {
                TraceWarn1("The last section has an incomplete flag.", "Index", index);
            }
        }

        //
        // Section type-specific checks.
        //
        switch (sectionTable[index].Type) {
            case RAW_DUMP_SECTION_TYPE_DDR_RANGE:
            {
                //
                // Save the pointer to the first DDR section.
                // This will make it easier to access stuff
                // in DDR sections.
                //
                if (Context->DDRSectionCount == 0) {
                    Context->DDRSections = &sectionTable[index];
                }

                Context->DDRSectionCount++;
            }
            break;

            case RAW_DUMP_SECTION_TYPE_CPU_CONTEXT:
            {
                Context->CPUContextSectionCount++;
                Context->TotalCpuContextSizeInBytes += sectionTable[index].Size;
                //
                // NOTE: As of now, we dont expect more than one CPU context regions to be present.
                //
                Context->CPUContextAddress.QuadPart = sectionTable[index].Offset;
            }
            break;

            case RAW_DUMP_SECTION_TYPE_SV_SPECIFIC:
            {
                Context->SVSectionCount++;
                Context->TotalSVSpecificSizeInBytes += sectionTable[index].Size;

                if (Context->LargestSVSpecificSectionSize < sectionTable[index].Size) {
                    Context->LargestSVSpecificSectionSize = sectionTable[index].Size;
                }
            }
            break;

            default:
            {
                Context->InvalidSectionCount++;
            }
            break;
        } //switch (sectionTable[index].Type)
    } //for dumpHeader->SectionsCount

    TraceInfo1("DDR Sections", "Count", Context->DDRSectionCount);

    TraceInfo1("SV Specific Sections", "Count", Context->SVSectionCount);

    TraceInfo1("CPU Context Sections", "Count", Context->CPUContextSectionCount);

    if (Context->DDRSectionCount == 0) {
        result = E_BAD_DATA;
        TraceHRESULT("Zero DDR Sections found.", result);
        goto Exit;
    }

    if (Context->InvalidVersionCount > 0) {
        result = E_UNEXPECTED;
        TraceHRESULT1("Sections with invalid versions detected.", "Count", Context->InvalidVersionCount, result);
        goto Exit;
    }

    if (Context->MissingFlagsCount > 0) {
        result = E_BAD_DATA;
        TraceHRESULT1("Sections with no flags set detected.", "Count", Context->MissingFlagsCount, result);
        goto Exit;
    }

    if (Context->InsufficientStorageSectionsCount > 0)  {
        result = E_UNEXPECTED;
        TraceHRESULT1("Unexpected sections with insufficient storage flag.", "Count", Context->InsufficientStorageSectionsCount, result);
        goto Exit;
    }

    if (Context->InvalidSectionCount > 0) {
        result = E_BAD_DATA;
        TraceHRESULT1("InvalidSectionCount is not zero.", "Count", Context->InvalidSectionCount, result);
        goto Exit;
    }

    TraceInfo("Section table validated");
    *IsRawDumpSectionTableValid = TRUE;
    result = S_OK;

Exit:
    return result;
}

HRESULT

CreateRawDumpDotBin(
    _Inout_ PDMP_CONTEXT Context
)
/*++

Routine Description:

This function creates a rawdump.bin on the disk when the rawdump is located in SVRawPartition.

Arguments:

Context - Pointer to the global context structure.

Return Value:

NT status code.

--*/
{
    HRESULT result = E_FAIL;

    //
    // Creating the rawdump.bin at a temporary location in the SD card.
    // TODO: Some devices does not have SD cards. Size of Rawdump.bin
    // will be approximately similar to the size RAM on the device.
    //
    TraceInfo("Starting to write the rawdump file ... \n\r");
    result = ReadRawDumpPartitionToFile(Context, Context->RawDumpPath);

    return result;
}


HRESULT
BuildDDRMemoryMap(
    PDMP_CONTEXT Context
)
/*++

Routine Description:

This routine builds a DDRMemoryMap table.

Arguments:

Context - Pointer to DmpContext

Return Value:

HRESULT

--*/
{
    UINT32  allocationSize;
    UINT32  index;
    UINT64  previousEnd;
    UINT64  currentStart;
    HRESULT result = E_FAIL;

    DDR_MEMORY_MAP tmpDdrRecord;

    Context->TotalDDRSizeInBytes = 0;
    Context->DDRMemoryMapCount = 0;
    Context->DDRMemoryMap = nullptr;
    allocationSize = sizeof(DDR_MEMORY_MAP)* Context->DDRSectionCount;

    Context->DDRMemoryMap = (PDDR_MEMORY_MAP)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, allocationSize);

    if (Context->DDRMemoryMap == nullptr) {
        result = E_OUTOFMEMORY;
        TraceHRESULT("Failed to allocate memory for DDR memory map.", result);
        goto Exit;
    }

    //
    // Fill in the table.
    //
    for (index = 0; index < Context->DDRSectionCount; index++)  {
        if (Context->DDRSections[index].Type != RAW_DUMP_SECTION_TYPE_DDR_RANGE) {
            result = E_BAD_DATA;
            TraceHRESULT("DDR Sections are not adjacent to each other", result);
            goto Exit;
        }

        Context->DDRMemoryMapCount++;
        Context->DDRMemoryMap[index].Base = Context->DDRSections[index].u.DDRInformation.Base;
        Context->DDRMemoryMap[index].Size = Context->DDRSections[index].Size;
        Context->DDRMemoryMap[index].Offset = Context->DDRSections[index].Offset;
        Context->DDRMemoryMap[index].End = Context->DDRMemoryMap[index].Base +
                                                Context->DDRMemoryMap[index].Size -
                                                1;
        Context->TotalDDRSizeInBytes += Context->DDRMemoryMap[index].Size;
    }

    //
    // DDR sections layout may be random,
    // sort by base address in ascending order
    //
    TraceInfo("Sorting DDR sections");
    for (UINT32 i = 0; i < Context->DDRMemoryMapCount - 1; i++)
    {
        for (UINT32 j = 0; j < Context->DDRMemoryMapCount - 1 - i; j++)
        {
            if (Context->DDRMemoryMap[j].Base > Context->DDRMemoryMap[j+1].Base)
            {
                memcpy(&tmpDdrRecord, &Context->DDRMemoryMap[j], sizeof(tmpDdrRecord));
                memcpy(&Context->DDRMemoryMap[j], &Context->DDRMemoryMap[j+1], sizeof(tmpDdrRecord));
                memcpy(&Context->DDRMemoryMap[j+1], &tmpDdrRecord, sizeof(tmpDdrRecord));
            }
        }
    }

    //
    // Check for sane DDR sections (no overlap, no negative size, no swapped boundaries)
    //
    TraceInfo("Checking DDR section sanity");
    for (index = 0; index < Context->DDRMemoryMapCount; index++)
    {
        if (Context->DDRMemoryMap[index].End < Context->DDRMemoryMap[index].Base)
        {
            result = E_BAD_DATA;
            TraceHRESULT1("DDR Section end before start", "Index", index, result);
            goto Exit;
        }

        if (Context->DDRMemoryMap[index].Size <= 0)
        {
            result = E_BAD_DATA;
            TraceHRESULT1("DDR Section size invalid", "Index", index, result);
            goto Exit;
        }

        if (index > 0)
        {
            previousEnd     = Context->DDRMemoryMap[index - 1].End;
            currentStart    = Context->DDRMemoryMap[index].Base;

            //
            // Ideal layout is each new segment starts at the previous end + 1
            // Anything else is a hole (harmless) or overlap (fatal)
            //
            if (previousEnd < currentStart - 1)
            {
                Context->DDRSectionFragmentationCount++;
            }
            else if (previousEnd >= currentStart)
            {
                Context->DDRSectionsOverlapCount++;
            }
        }
    }

    if (Context->DDRSectionFragmentationCount != 0)
    {
        TraceInfo1("Holes in DDR Sections are harmless", "Holes:", Context->DDRSectionFragmentationCount);
    }

    if (Context->DDRSectionsOverlapCount != 0)
    {
        result = E_BAD_DATA;
        TraceHRESULT1("Overlap in DDR Sections is fatal", "Overlap", Context->DDRSectionsOverlapCount, result);
        goto Exit;
    }

    result = S_OK;

Exit:
    return result;
}

HRESULT
GetDumpInstance(
    _Out_ PULARGE_INTEGER DumpInstance
)
/*++

Routine Description:
    This function uses variable services to get the DumpInstance value.
    The Reg Value is created and populated by function IopCachePreviousBootData()
    in "\minkernel\ntos\io\iomgr\dumpctl.c" which copies the NVRAM value "DumpInstance"
    at boot time.  If the NVRAM variable does not exist, the function never creates this
    reg key.
    The fallback is to use the default value of zero if the reg key is not present.

Arguments:
    DumpInstance - pointer to instance Id

Return Value:
    HRESULT


--*/
{
    HRESULT hr = E_FAIL;
    HKEY    hk;

    if (ERROR_SUCCESS == RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          TEXT(DUMP_REGKEY),
                          0,
                          KEY_QUERY_VALUE,
                          &hk))
    {
        DWORD  regValSize = sizeof(ULARGE_INTEGER);
        if (ERROR_SUCCESS != RegQueryValueExW(hk,
                             TEXT(L"DumpInstancePrvBoot"),
                             NULL,
                             NULL,
                             (LPBYTE) DumpInstance,
                             &regValSize))
        {
            TraceInfo("Failed to read the value of DumpInstance from the registry");
            DumpInstance->QuadPart = 0;
        }

        hr = S_OK;
        RegCloseKey(hk);
    }
    else
    {
        TraceInfo("Failed to get the registry key for CrashControl");
    }

    return hr;
}

HRESULT
ReadRawDumpPartitionToFile(
    _Inout_ PDMP_CONTEXT Context,
    _In_    LPCWSTR FilePath
)
/*++

Routine Description:

    This function Read Raw Dump Partition to a single file.
    On successful read, it makes Context->hDisk = the handle of
    of the newly created file.

Arguments:

    Context - Pointer to PDMP_CONTEXT
    FilePath - the bin file name

Return Value:

    HRESULT

--*/
{
    HRESULT         result;
    DEVICE_IO       hFile;
    PCHAR           buffer;

    if( nullptr == (buffer = (PCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, DEFAULT_DMP_BUF_SZ)) )
    {
        result = E_OUTOFMEMORY;
        TraceHRESULT("Could not allocate memory for buffer", result);
    }
    else if ( FAILED(result = Context->hDisk.SetPos(0)) )
    {
        TraceHRESULT("FAILED: SetPos(0) - cannot go to beginning of partition", result);
    }
    else if ( FAILED(result = hFile.Open(FilePath)) )
    {
        TraceHRESULT("FAILED: cannot open destination file", result);
    }
    else
    {
        ULONGLONG   RemainingSize = Context->hDisk.GetCurrentPartitionSize();

        while ( (RemainingSize > 0)
                && (DEVICE_IO::IO_ERROR_EOF != Context->hDisk.GetError())
              )
        {
            size_t bytesRead = 0;
            size_t bytesWritten = 0;

            if ( FAILED(result = Context->hDisk.Read( buffer, DEFAULT_DMP_BUF_SZ, &bytesRead))
                 || (0 == bytesRead)
               )
            { // Failed to read data from disk
                TraceHRESULT("ReadRawDumpPartitionToFile() - Failed on Read() partition!", result);
                break;
            }
            else if ( FAILED(result = hFile.Write( buffer, bytesRead, &bytesWritten))
                      || (0 == bytesWritten)
                    )
            {  // Failed to write data to file
                result = HRESULT_FROM_WIN32(GetLastError());
                TraceHRESULT("\nReadRawDumpPartitionToFile() - Failed to Write() (file)", result);
                break;

            }
            else
            { // update the bytes remainig and continue
                RemainingSize -= bytesWritten;
            }

        }

        // exchange original handle with the new file
        if ( FAILED(hFile.Close())
             || FAILED(Context->hDisk.Close())
             || FAILED(Context->hDisk.Open(FilePath))
           )
        {
            result = HRESULT_FROM_WIN32(GetLastError());
            TraceHRESULT("ERROR: Failed to re-open file!", result);
        }
    }

    if (nullptr != buffer)
    {
        HeapFree(GetProcessHeap(), 0, buffer);
    }

    if (SUCCEEDED(result))
    {
        TraceInfo("Writing raw memory data to file completed successfully!");
    }
    else
    {
        TraceInfo("ERROR: Writing raw memory data to file failed.");
    }

    return result;
}


VOID
CleanupContext(
_In_ PDMP_CONTEXT Context
)
/*++

Routine Description:
This function cleans up the memory allocated by the library.

Arguments:
Context - PDMP_CONTEXT

Return Value:
NTSTATUS

--*/
{
    TraceInfo("Deleting the temporary files");

    //
    //  Since this is cleanup, we delete the files on a  best effort basis.
    //

    if (Context->RawDumpPath != nullptr) {
        if (!DeleteFileW(Context->RawDumpPath)) {
            TraceWIN32("DeleteFile Context->RawDumpPath returned error ", GetLastError());
        }
        HeapFree(GetProcessHeap(), 0, Context->RawDumpPath);
    }

    if (Context->RawDumpInfoPath != nullptr) {
        if (!DeleteFileW(Context->RawDumpInfoPath)) {
            TraceWIN32("DeleteFile Context->RawDumpInfoPath returned error", GetLastError());
        }
        HeapFree(GetProcessHeap(), 0, Context->RawDumpInfoPath);
    }

    TraceInfo("Cleaning up the context now");
    //
    // Free the dump header
    //
    if (Context->DumpHeader != nullptr){
        HeapFree(GetProcessHeap(), 0, Context->DumpHeader);
    }

    //
    // Free the dump header
    //
    if (Context->RawDumpHeader != nullptr){
        HeapFree(GetProcessHeap(), 0, Context->RawDumpHeader);
    }

    //
    // Free the DDRMemoryMap
    //
    if (Context->DDRMemoryMap != nullptr){
        HeapFree(GetProcessHeap(), 0, Context->DDRMemoryMap);
    }

    //
    // Clean up the handles.
    //
    Context->hDisk.Close();
    Context->SDCardRawDumpDotBinFileHandle.Close();
    Context->SDCardDeviceHandle.Close();
    Context->hInfoFileHandle.Close();
}




//
// ------------------------- helper function --------------------------------
//


HRESULT
ReadFromDDRSectionByPhysicalAddress(
    _In_ PDMP_CONTEXT Context,
    _In_ LARGE_INTEGER PhysicalAddress,
    _In_ UINT64 Length,
    _Out_ PVOID Buffer
)
/*++

Routine Description:
    This function reads the contents of memory in DDR sections.

Arguments:
    Context - Dmp_CONTEXT
    PhysicalAddress - Physical address of memory in DDR sections which we want
    to read from.
    Length - Number of bytes to read.
    Buffer - Buffer holding the contents of the read.

Return Value:
    HRESULT

--*/
{
    UINT64                      addressStart;
    UINT64                      addressEnd;
    UINT32                      bytesToRead = 0;
    UINT64                      bytesRemain;
    UINT32                      ddrSectionsCount = 0;
    LARGE_INTEGER               offset;
    PDDR_MEMORY_MAP             ddrMap = nullptr;
    UINT32                      index = 0;
    UINT64                      sectionStart;
    UINT64                      sectionEnd;
    UINT32                      sectionSpanCount = 0;
    HRESULT                     result = E_FAIL;
    PVOID                       temp;

    ddrSectionsCount = Context->DDRMemoryMapCount;
    ddrMap = Context->DDRMemoryMap;
    addressStart = PhysicalAddress.QuadPart;
    addressEnd = addressStart + Length - 1;
    bytesRemain = Length;
    offset.QuadPart = 0;
    temp = Buffer;

    //
    // Determine the right section.
    //
    for (index = 0; index < ddrSectionsCount; index++) {
        sectionStart = ddrMap[index].Base;
        sectionEnd = ddrMap[index].End;

        if ((sectionSpanCount != 0) &&
                (bytesRemain != 0) &&
                (!ddrMap[index].Contiguous)) {

            TraceInfo2("Attempting to read spanning discontiguous sections.","Section Index", index, "Bytes Remaining", bytesRemain);
            result = HRESULT_FROM_NT(STATUS_INVALID_PARAMETER);
            goto Exit;
        }

        //
        // Determine if address falls within this section.
        //
        if ((sectionStart <= addressStart) &&
            (sectionEnd >= addressStart)) {

            size_t  bytesRead = 0;

            //
            // Figure out how many bytes to read.
            // Need to figure this out because we may
            // cross section boundaries.
            //
            bytesToRead = (sectionEnd >= addressEnd) ?
                (UINT32)(addressEnd - addressStart + 1) :
                (UINT32)(sectionEnd - addressStart + 1);

            offset.QuadPart = Context->diskoffset.QuadPart + (addressStart - sectionStart) + ddrMap[index].Offset;

            TraceInfo2("Reading bytes at offset", "Number of BytestoRead", bytesToRead, "Offset", offset.QuadPart);

            if ( FAILED(Context->hDisk.SetPos(offset))
                 || FAILED(Context->hDisk.Read((PCHAR)temp, bytesToRead, &bytesRead))
                 || (bytesToRead != bytesRead)
               )
            {
                TraceHRESULT("ReadFromDDRSectionByPhysicalAddress:ReadDisk failed", result);
            }

            //
            // Update bytesRemain.
            //
            bytesRemain = bytesRemain - bytesToRead;

            if (bytesRemain != 0)
            {
                //
                // Time to move to next section.
                // Update temp.
                //
                temp = (PVOID)((UINT32)(temp)+bytesToRead);

                //
                // Update addressStart.
                // AddressEnd should not be updated.
                //
                addressStart = addressStart + bytesToRead;
                sectionSpanCount++;
            }
            else
            {
                //
                // No more bytes to read. Time to leave.
                //
                result = S_OK;
                break;
            }
        }//(sectionStart <= addressStart)
    }//for (index = 0; index < ddrSectionsCount; index++)

    //
    // We end up here either because bytesRemain is zero or
    // we went through all the DDR sections and still couldn't
    // read all the bytes.
    //
    if ((index == ddrSectionsCount) &&
        (bytesRemain != 0))
    {
        result = E_FAIL;
        TraceHRESULT2("Read incomplete.", "Bytes Yet To Be Read", bytesRemain, "Total Number Of Bytes", Length, result);
        goto Exit;
    }

Exit:

    return result;
}

HRESULT
AppendDeviceSpecificInfoToRawDump(
    _Inout_ PDMP_CONTEXT Context,
    _In_ SvSpecific* SVData
)
{
    HRESULT hr = S_OK;

    DEVICE_SPECIFIC_INFO devSpeInfo;
    if (FAILED(hr = SVData->BuildInfoBuffer(&devSpeInfo)))
    {
        TraceHRESULT("AppendDeviceSpecificInfoToRawDump:Failed to build Device Specific Info", hr);
    }
    else if (FAILED(hr = WriteDeviceSpecificInfo( &Context->hDisk, &devSpeInfo, Context->hDisk.GetCurrentFileSize())))
    {
        TraceHRESULT("AppendDeviceSpecificInfoToRawDump:Failed to append raw dump data to file", hr);
    }

    return hr;
}



HRESULT SubmitReportToWER(
    _Inout_ PDMP_CONTEXT Context
)
{
    HRESULT hr = E_FAIL;
    WCHAR   pszDest[30];

    HREPORT Report = NULL;
    //
    // Create a new report
    //

   hr = WerReportCreate(L"WindowsOfflineCrash", WerReportCritical, NULL, &Report);
    if(!SUCCEEDED(hr)) {
        // Log an error.
        goto Exit;
    }


    //
    //  Add the rawdump file as 'other' files.
    //  MSFT:9212720 - task for enhancing this call
    //
    hr = WerReportAddFile(Report,
                          Context->RawDumpPath,
                          WerFileTypeOther,
                          WER_FILE_ANONYMOUS_DATA);
    if(!SUCCEEDED(hr)) {
        // Log an error
        goto Exit;
    }


    //
    //  Add the rawdump infor file as 'other' files.
    //
    hr = WerReportAddFile(Report,
                          Context->RawDumpInfoPath,
                          WerFileTypeOther,
                          WER_FILE_ANONYMOUS_DATA);
    if(!SUCCEEDED(hr)) {
        // Log an error
        goto Exit;
    }

    //
    //  Add the rawdump infor file as 'other' files.
    //
    hr = WerReportAddFile(Report,
                          Context->LogFilePath,
                          WerFileTypeOther,
                          WER_FILE_ANONYMOUS_DATA);
    if (!SUCCEEDED(hr)) {
        // Log an error
        goto Exit;
    }

    //
    // Add report parameters
    // TODO: How do you get the build?
    //
     hr = WerReportSetParameter(Report,
                               WER_P0,
                               L"Build",
                               L"0000");
    if(!SUCCEEDED(hr)) {
        // Log an error
        goto Exit;
    }

    hr = WerReportSetParameter(Report,
                               WER_P1,
                               L"Bugcheck code",
                               L"14C");
    if(!SUCCEEDED(hr)) {
        // Log an error
        goto Exit;
    }


    hr = StringCchPrintfW(pszDest,
                        ARRAYSIZE(pszDest),
                        L"0x%x",
                        Context->DumpHeader->BugCheckParameter1);
    if(!SUCCEEDED(hr)) {
         goto Submit;
    }
    hr = WerReportSetParameter(Report,
                               WER_P2,
                               L"Bugcheck Parameter 1",
                               pszDest);

     if(!SUCCEEDED(hr)) {
        goto Submit;
    }

    hr = StringCchPrintfW(pszDest,
                    ARRAYSIZE(pszDest),
                    L"0x%x",
                    Context->DumpHeader->BugCheckParameter2);
    if(!SUCCEEDED(hr)) {
        goto Submit;
    }

    hr = WerReportSetParameter(Report,
                               WER_P3,
                               L"Bugcheck Parameter 2",
                               pszDest);
    if(!SUCCEEDED(hr)) {
        goto Submit;
    }

    hr = StringCchPrintfW(pszDest,
                    ARRAYSIZE(pszDest),
                    L"0x%x",
                    Context->DumpHeader->BugCheckParameter3);
    if(!SUCCEEDED(hr)) {
            // Log an error
            goto Submit;
    }

    hr = WerReportSetParameter(Report,
                               WER_P4,
                               L"Bugcheck Parameter 3",
                               pszDest);
    if(!SUCCEEDED(hr)) {
        goto Submit;
    }

    hr = StringCchPrintfW(pszDest,
                    ARRAYSIZE(pszDest),
                    L"0x%x",
                    Context->DumpHeader->BugCheckParameter4);
    if(!SUCCEEDED(hr)) {
            // Log an error
            goto Submit;
    }

    hr = WerReportSetParameter(Report,
                               WER_P5,
                               L"Bugcheck Parameter 4",
                               pszDest);

     if(!SUCCEEDED(hr)) {
            // Log an error
            goto Submit;
    }

Submit:
    //
    // Submit the report
    //
    WER_SUBMIT_RESULT Result;
    hr = WerReportSubmit(Report,
                         WerConsentNotAsked,
                         WER_SUBMIT_OUTOFPROCESS,
                         &Result);
Exit:
    return hr;
}



BOOLEAN
ValidateRawDumpHeaderFlags(
_In_ UINT32 Flags
)
/*++

Routine Description:

This function validates the RAW_DUMP_HEADER. The following are checked.

We're going to check the flags. The things we are looking for are:
1. No reserved bits are set.
2. Only one of the defined bits are set.

Arguments:

Flags - Flags in the RAW_DUMP_HEADER.Flags or RAW_DUMP_SECTION_HEADER.Flags.

Return Value:

NT status code.

--*/
{
    UINT32 expectedBits = 0;

    //
    // Verify that none of the reserved bits are set.
    //
    expectedBits = RAW_DUMP_HEADER_FLAGS_VALID |
        RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE;

    if ((Flags & ~expectedBits) != 0)
    {
        TraceInfo1("Dump Header Flags invalid! No bits set.","Flags", Flags);
        return FALSE;
    }

    //
    // Now check if all defined bits are set.
    //
    if ((Flags & expectedBits) == expectedBits)
    {
        TraceInfo1("Dump Header Flags! All defined bits are set.", "Flags", Flags);
        return FALSE;
    }

    //
    // Now check if only one bit is set.
    //
    if ((Flags & (Flags - 1)) != 0)
    {
        TraceInfo1("Dump Header Flags! More than one bit is set.", "Flags", Flags);
        return FALSE;
    }

    return TRUE;
}


VOID
DumpRawDumpHeader(
_In_ const PRAW_DUMP_HEADER Header
)
{
    ULONG index;
    UINT32 sectionCount;
    size_t namelength;
    PRAW_DUMP_SECTION_HEADER    sectionTable = nullptr;

    CHAR name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
    WCHAR sectionName[RAW_DUMP_SECTION_HEADER_NAME_LENGTH + 1];

    DmpLog("\tSignature: 0x%I64x\r\n", Header->Signature);
    DmpLog("\tVersion: 0x%x\r\n", Header->Version);
    DmpLog("\tFlags: 0x%x\r\n", Header->Flags);
    DmpLog("\tOsData: 0x%I64x\r\n", Header->OsData);
    DmpLog("\tCpuContext: 0x%I64x\r\n", Header->CpuContext);
    DmpLog("\tResetTrigger: 0x%x\r\n", Header->ResetTrigger);
    DmpLog("\tDumpSize: 0x%I64x\r\n", Header->DumpSize);
    DmpLog("\tTotalDumpSizeRequire: 0x%I64x\r\n", Header->TotalDumpSizeRequired);
    DmpLog("\tSectionsCount: 0x%x\r\n", Header->SectionsCount);

    sectionCount = Header->SectionsCount;

    sectionTable = (PRAW_DUMP_SECTION_HEADER)&Header->SectionTable[0];

    for (index = 0; index < sectionCount; index++)
    {
        DmpLog("Section[%d]: 0x%p\r\n", index, &sectionTable[index]);
        DmpLog("\t\tFlags: 0x%x\r\n", sectionTable[index].Flags);
        DmpLog("\t\tVersion: 0x%x\r\n", sectionTable[index].Version);

        if (sectionTable[index].Type <  RAW_DUMP_SECTION_TYPE_Maximum)
        {
            DmpLog("\t\tType: 0x%x => %ls\r\n",
                sectionTable[index].Type,
                RawDumpSectionTypeStringTable[sectionTable[index].Type]);
        }
        else
        {
            DmpLog("\t\tType: 0x%x => UNKNOWN\r\n", sectionTable[index].Type);
        }

        DmpLog("\t\tOffset: 0x%I64x\r\n", sectionTable[index].Offset);
        DmpLog("\t\tSize: 0x%I64x\r\n", sectionTable[index].Size);

       switch (sectionTable[index].Type)
       {
       case RAW_DUMP_SECTION_TYPE_RESERVED:
           DmpLog("\t\tType is reserved!!\r\n");
           break;

        case RAW_DUMP_SECTION_TYPE_DDR_RANGE:
            DmpLog("\t\tDumping DDR_INFORMATION...\r\n");
            DmpLog("\t\tBase: 0x%I64x\r\n", sectionTable[index].u.DDRInformation.Base);
            break;

        case RAW_DUMP_SECTION_TYPE_CPU_CONTEXT:
            DmpLog("\t\tDumping CPU_CONTEXT_INFORMATION...\r\n");
            break;

        case RAW_DUMP_SECTION_TYPE_SV_SPECIFIC:
            DmpLog("\t\tDumping SV_SPECIFIC_INFORMATION...\r\n");
            DmpLog("\t\t");
            DumpGUID((GUID*)sectionTable[index].u.SVSpecificInformation.SVSpecificData);
            break;
        default:
            DmpLog("\t\tUNKNOWN section type. 0x%x\r\n", sectionTable[index].Type);
            break;
        }

        memset(name, 0, RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
        memcpy(name, (void*)&(sectionTable[index].Name), RAW_DUMP_SECTION_HEADER_NAME_LENGTH);
        namelength = strlen(name);
        sectionName[namelength] = 0;
        mbstowcs(sectionName, name, namelength);
        DmpLog("\t\tName: %ls\r\n", sectionName);
    }

}

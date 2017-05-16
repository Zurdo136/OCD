/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   DEVICE_IO.cpp

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/
#include <SDKDDKVer.h>
#include <Strsafe.h>

// Need to include GUIDDEF.H with INITGUID defined so that the
// GUIDS defined in OCD_Common.h are explicitly created here.
// Every other consumer of OCD_COMMON.H will have external
// references to these GUIDS.
#define INITGUID
#include <guiddef.h>
#include <GUIDDefs.h>
#undef INITGUID
#include <assert.h>

#include <DEVICE_IO.h>

#define     EXPECTED_PARTITION_COUNT        20
#define     MAX_RETRY                       5

#define MAX_TYPE(t)     ((t)-1)
#define MAX_DWORD       MAX_TYPE(DWORD)
#define MAX_ULONG       MAX_TYPE(ULONG)
#define MAX_SIZE_T      MAX_TYPE(size_t)

typedef enum {
    IO_TYPE_INVALID = 0,
    IO_TYPE_READ,
    IO_TYPE_WRITE,
    IO_TYPE_MAX
} IO_TYPE;

using namespace std;

// // // // // // // // // // // // // // // // // // // // // // // // // // // // // // //
// Constructors and Destructor
// // // // // // // // // // // // // // // // // // // // // // // // // // // // // // //
/*************************************************************************************************
** DEVICE_IO(_In_ const DEVICE_IO &obj)
**    Copy constructor - should be invalid to use externally and will assert if used in any
**    of the class methods.
*************************************************************************************************/
DEVICE_IO::DEVICE_IO(_In_ const DEVICE_IO &obj)
{
    UNREFERENCED_PARAMETER(obj);
    assert(0);
}


/*************************************************************************************************
** DEVICE_IO(_In_ wstring devName)
**    Constructor using full device or file name
*************************************************************************************************/
DEVICE_IO::DEVICE_IO(_In_ wstring devName)
{
    Init(devName, INVALID_DEVICE_ID);
}


/*************************************************************************************************
** DEVICE_IO(_In_ UINT devID)
**    Constructor using device index (ID)
*************************************************************************************************/
DEVICE_IO::DEVICE_IO(_In_ UINT devID)
{
    Init(L"", devID);
}


/*************************************************************************************************
** DEVICE_IO(void)
**    Default Constructor
*************************************************************************************************/
DEVICE_IO::DEVICE_IO(void)
{
    Init(L"", INVALID_DEVICE_ID);
}


/*************************************************************************************************
** ~DEVICE_IO(void)
**    Destructor
*************************************************************************************************/
DEVICE_IO::~DEVICE_IO(void)
{
    Close();
    if (m_pCache != nullptr)
    {
        free(m_pCache);
        m_pCache = nullptr;
    }

    return;
}


// // // // // // // // // // // // // // // // //
// // //  Disk/Device Geometry functions  // // //
// // // // // // // // // // // // // // // // //
/*************************************************************************************************
** HRESULT ReadDiskGeometry(void)
**    Obtain the disk geometry;
**        1. disk's geometry is a static table
**        2. Geometry table includes the device block size, used to perform block IO operations
**        3. failing this function returns an error and causes the device type to be unsupported
**        4. only "FixedMedia" and "RemovableMedia" are currently supported
*************************************************************************************************/
HRESULT
DEVICE_IO::ReadDiskGeometry(void)
{
    HRESULT ret = E_FAIL;

    if (m_Type == PLAIN_FILE_DEVICE_TYPE)
    {
        m_LastError = IO_ERROR_INVALID_METHOD_USED;
    }
    else
    {
        ULONG          ReturnedLength;
        DISK_GEOMETRY  diskGeometry;

        if (FALSE == DeviceIoControl(
                            m_Handle,
                            IOCTL_DISK_GET_DRIVE_GEOMETRY,
                            nullptr,
                            0,
                            &diskGeometry,
                            sizeof(DISK_GEOMETRY),
                            &ReturnedLength,
                            nullptr)
            )
        { // failed to IOCTL the geometry, report the failure
            m_LastError = IO_ERROR_GEOMETRY_FAILURE;
            ret = HRESULT_FROM_WIN32(GetLastError());
        }
        else
        { // have Geometry, Classify the device type and save the block size
            ret = S_OK;
            switch (diskGeometry.MediaType)
            {
                case FixedMedia:
                    m_Type = RAW_DEVICE_TYPE;
                    SetIOGeometry(diskGeometry);
                    break;

                case RemovableMedia:
                    m_Type = REMOVABLE_MEDIA_DEVICE_TYPE;
                    SetIOGeometry(diskGeometry);
                    break;

                case Unknown:
                default:
                    m_Type = UNSUPPORTED_DEVICE_TYPE;
                    m_LastError = IO_ERROR_UNSUPPORTED_DEVICE_TYPE;
                    ret = E_FAIL;
                    break;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
** HRESULT ReadDiskLayout(void)
**    Obtain the disk layout;
**        1. allocates space for disks partition table
**        2. table allocation will start with best guess and increase the size "MAX_RETRY" times
**        3. failing this function returns an error and causes the I/O class to be uninitialized
**        4. if the partition type is not GPT, the I/O class is unsupported
**************************************************************************************************/
HRESULT
DEVICE_IO::ReadDiskLayout(void)
{
    HRESULT     ret             = E_FAIL;
    UINT        LayoutSize      = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + (EXPECTED_PARTITION_COUNT * sizeof(PARTITION_INFORMATION_EX));

    for (UINT retryCount = MAX_RETRY; (retryCount > 0); retryCount--)
    {
        ULONG ReturnedLength;

        m_pDriveLayout = (PDRIVE_LAYOUT_INFORMATION_EX)malloc(LayoutSize);
        if (m_pDriveLayout == nullptr)
        {
            m_LastError = IO_ERROR_NO_MEMORY;
            ret = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        ZeroMemory(m_pDriveLayout, LayoutSize); // Size not exact so ensure extra bytes are zero
        if (FALSE != DeviceIoControl(
                        m_Handle,
                        IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                        nullptr,
                        0,
                        m_pDriveLayout,
                        LayoutSize,
                        &ReturnedLength,
                        nullptr)
            )
        { // Device information read!
            break;
        }

        // buffer size problem, increase the allocation size and retry
        free(m_pDriveLayout);
        m_pDriveLayout = nullptr;

        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        { // before we loop again, be sure this is really a buffer size issue
            m_LastError = IO_ERROR_CANNOT_READ_DRIVE_LAYOUT;
            ret = HRESULT_FROM_WIN32(GetLastError());
            break;
        }

        LayoutSize += (EXPECTED_PARTITION_COUNT * sizeof(PARTITION_INFORMATION_EX));
    }

    if (m_pDriveLayout != nullptr)
    { // Device information correctly read, ensure we have a GPT partition
        switch (m_pDriveLayout->PartitionStyle)
        {
            case PARTITION_STYLE_GPT:
                m_LastError = IO_OK;
                ret = S_OK;
                break;

            case PARTITION_STYLE_MBR:
                m_Type = UNSUPPORTED_DEVICE_TYPE;
                m_LastError = IO_ERROR_UNSUPPORTED_PARTITION_MBR;
                break;

            case PARTITION_STYLE_RAW:
                m_Type = UNSUPPORTED_DEVICE_TYPE;
                m_LastError = IO_ERROR_UNSUPPORTED_PARTITION_RAW;
                break;

            default:
                m_Type = UNSUPPORTED_DEVICE_TYPE;
                m_LastError = IO_ERROR_UNSUPPORTED_PARTITION_TYPE;
                break;
        }

    }

    return ret;
}


// // // // // // // // // // // // // // // // //
// // // //    Init for Constructors   // // // //
// // // // // // // // // // // // // // // // //
/*************************************************************************************************
** VOID Init(_In_ wstring devName, _In_ UINT devID)
**    Init class data to a known state - called by all constructors
**************************************************************************************************/
VOID
DEVICE_IO::Init(_In_ wstring devName, _In_ UINT devID)
{
    m_Name = devName;
    m_LastError = IO_ERROR_INVALID_HANDLE;
    m_Type = UNINITIALIZED_DEVICE_TYPE;

    m_ID = devID;
    m_BlockSize = DEFAULT_BLOCK_SIZE;

    m_pDriveLayout = nullptr;
    m_pCurrentPartition = nullptr;
    m_ndxCurrentPartition = INVALID_INDEX;
    m_CurrentPartitionBlockCount = { 0 };

    m_Handle = INVALID_HANDLE_VALUE;
    m_IOCurPos = { 0 };
    m_IOCurBlock = { 0 };
    m_IOBlockCount = { 0 };
    m_IOSize = { 0 };

    m_CacheCurBlock.QuadPart = INVALID_BLOCK;
    m_CacheCurOffset = 0;
    m_CacheBlockCount = DEFAULT_CACHE_BLOCK_COUNT;
    m_CacheSize = 0;
    m_pCache = nullptr;

    return;
}


/*************************************************************************************************
** VOID SetIOGeometry(DISK_GEOMETRY diskGeometry)
**    set the device data
**************************************************************************************************/
VOID
DEVICE_IO::SetIOGeometry(DISK_GEOMETRY diskGeometry)
{
    m_BlockSize = diskGeometry.BytesPerSector;
    m_IOSize.QuadPart = diskGeometry.Cylinders.QuadPart;
    m_IOSize.QuadPart *= diskGeometry.TracksPerCylinder
                      *diskGeometry.SectorsPerTrack
                      *diskGeometry.BytesPerSector;
    SetIOBlockCount();
}


/*************************************************************************************************
** VOID SetPartitionGeometry(void)
**    make the partition data valid
**************************************************************************************************/
VOID
DEVICE_IO::SetPartitionGeometry(void)
{
    m_IOCurPos.QuadPart = m_pCurrentPartition->PartitionLength.QuadPart;
    m_CurrentPartitionBlockCount.QuadPart = m_IOCurPos.QuadPart / m_BlockSize;
    m_CurrentPartitionBlockCount.QuadPart += (m_IOCurPos.QuadPart % m_BlockSize) ? 1 : 0;
//    m_IOCurBlock = m_CurrentPartitionBlockCount;
}


// // // // // // // // // // // // // // // // //
// // // //  Check for Ready functions // // // //
// // // // // // // // // // // // // // // // //
/**************************************************************************************************
** BOOL IsDeviceReady(void)
**    Validate if the device paramaters are set, including;
**        1. is there a valid handle
**        2. if a RAW device - make sure the layout was read
**        3. sets m_LastError appropriately
**************************************************************************************************/
BOOL
DEVICE_IO::IsDeviceReady(void)
{
    BOOL ret = FALSE;

    if (m_Handle == INVALID_HANDLE_VALUE)
    {
        m_LastError = IO_ERROR_INVALID_HANDLE;
    }
    else if ( (RAW_DEVICE_TYPE == m_Type) || (REMOVABLE_MEDIA_DEVICE_TYPE == m_Type) )
    {
        if (m_pDriveLayout == nullptr)
        {
            m_LastError = IO_ERROR_INVALID_DRIVE_LAYOUT;
        }
        else
        {
            ret = TRUE;
        }

    }
    else
    {
        ret = TRUE;
    }

    return ret;
}


/**************************************************************************************************
** BOOL    IsIoReady(void)
**    Validate the device and IO paramaters are set, including;
**        1. IsDeviceReady() == TRUE
**        2. if a RAW device - make sure a partition is selected
**************************************************************************************************/
BOOL
DEVICE_IO::IsIoReady(void)
{
    BOOL ret = IsDeviceReady();

    if ( ret &&
         ((RAW_DEVICE_TYPE == m_Type) || (REMOVABLE_MEDIA_DEVICE_TYPE == m_Type)) &&
         (m_pCurrentPartition == nullptr)
       )
    {
        m_LastError = IO_ERROR_PARTITION_NOT_SET;
        ret = FALSE;
    }

    return ret;
}


// // // // // // // // // // // // // //
// // //   Cache Functionality   // // //
// // // // // // // // // // // // // //
/**************************************************************************************************
** BOOL AllocateCache(_In_ UINT blockSizeMult)
**   Allocate a new cache for reading from the device partition.  Typically, reading from a device
**   is done in units of blocks. The block size, for a device, is dictated by the filesystem used
**   and is read from the disk geometry. Device reads (for UFS) must be done in blocks. Thus,
**   reading large chunks of blocks is more efficient than single blocks.
**   Having large chunks of the disk in the cache (pre-read) will faciliate some of the
**   operations we perform on phones (and other devices) to retrieve data from certain partitions.
**   Cache activity is relevant to a selected partition.  These allocations are done from the
**   partition selection functions and so a partition is presumed to be selected.  When a new
**   partition is selected, the an allocation is performed and so we need to ensure that the new
**   cache will be sized appropriately for small partitions.
**************************************************************************************************/
BOOL
DEVICE_IO::AllocateCache(_In_ UINT blockSizeMult)
{ // Create a buffer to perform IO reads
    BOOL    ret = FALSE;

    if (m_IOBlockCount.QuadPart < 1)
    { // Fail if the disk has no space (blocks) allocated
        m_LastError = IO_ERROR_DEVICE_NO_BLOCKS;
    }
    else if (IsIoReady())
    { // allocate a cache based on selected partition
        FreeCache();
        m_CacheBlockCount = (0 == blockSizeMult) ? DEFAULT_CACHE_BLOCK_COUNT : blockSizeMult;
        if ( (0 == m_CurrentPartitionBlockCount.HighPart) &&
             (m_CacheBlockCount > m_CurrentPartitionBlockCount.LowPart ) )
        { // This clamps the allcoation for small partitions
            m_CacheBlockCount = (ULONG)m_CurrentPartitionBlockCount.LowPart;
        }

        m_LastError = IO_ERROR_NO_MEMORY;
        do
        { // Try and allocate the largest cache possible
            m_CacheSize =  m_BlockSize * ( ((ULONGLONG)(m_BlockSize * m_CacheBlockCount) > (ULONGLONG)MAX_ULONG)
                                             ? (MAX_ULONG / m_BlockSize) 
                                             : m_CacheBlockCount
                                         );

            m_pCache = (PCHAR)malloc(m_CacheSize);
            if (m_pCache != nullptr)
            {
                ZeroMemory(m_pCache, m_CacheSize);
                m_LastError = IO_OK;
                ret = TRUE;
            }
            else
            { // if the allocation failed - try a smaller cache size
                m_CacheBlockCount--;
                m_CacheSize = 0;
            }

        } while ((nullptr == m_pCache) && (m_CacheBlockCount > 0));

    }

    return ret;
}


/*************************************************************************************************
**  HRESULT UpdateCache(_In_reads_bytes_(bufferSize) PCHAR pBuffer, _In_ size_t bufferSize)
**    Function to write bytes from the buffer provided to the cache.
**    A valid cache should be present before calling this function. If the cache is not valid,
**    the function fails with IO_ERROR_CACHE_INVALID. The cache must have enough free space
**    (bytes) to accommodate the provided buffer. If the cache cannot accommodate the buffer,
**    the function fails with IO_ERROR_CACHE_WRITE_SIZE.
**    If the update (memcpy_s()) fails, the last error is set to IO_ERROR_CACHE_MODIFY.
*************************************************************************************************/
HRESULT
DEVICE_IO::UpdateCache(_In_reads_bytes_(bufferSize) PCHAR pBuffer, _In_ size_t bufferSize)
{
    HRESULT    ret = E_FAIL;

    if (nullptr == pBuffer)
    {
        m_LastError = IO_ERROR_NULL_POINTER;
    }
    else if ( (0 == bufferSize) || (bufferSize > m_CacheSize) )
    { // Fail if buffer size is (1) zero or (2) larger than m_CacheSize
            m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
    }
    else if (!IsCacheValid())
    {
        m_LastError = IO_ERROR_CACHE_INVALID;
    }
    else if (bufferSize > GetCacheRemaining())
    {
            m_LastError = IO_ERROR_CACHE_WRITE_SIZE;
    }
    else if (FALSE != memcpy_s((m_pCache + m_CacheCurOffset), bufferSize, pBuffer, bufferSize))
    {
        m_LastError = IO_ERROR_CACHE_MODIFY;
        ret = HRESULT_FROM_WIN32(GetLastError());
    }
    else
    {
        m_CacheCurOffset += bufferSize;
        m_IOCurPos.QuadPart += bufferSize;
        m_LastError = IO_OK;
        ret = S_OK;
    }

    return ret;
}


/**************************************************************************************************
** BOOL FreeCache(void)
**    Clear all cache variables and release the allocated memory
**************************************************************************************************/
VOID
DEVICE_IO::FreeCache(void)
{
    m_CacheCurBlock.QuadPart = INVALID_BLOCK;
    m_CacheBlockCount = 0;
    m_CacheSize = 0;
    m_LastError = IO_OK;
    if (nullptr != m_pCache)
    {
        free(m_pCache);
        m_pCache = nullptr;
    }

    return;
}


/**************************************************************************************************
** BOOL DEVICE_IO::IsPosValid (_In_ ULONGLONG newPos) const
**   Given a position, check if it is in the partition
**************************************************************************************************/
BOOL
DEVICE_IO::IsPositionValid (_In_ ULONGLONG newPos)
{
    BOOL ret = TRUE;

    if (!IsIoReady())
    {
        ret = FALSE;
    }
    else if (newPos >= GetCurrentPartitionSize())
    {
        m_LastError = IO_ERROR_EOF;
        ret = FALSE;
    }

    return ret;
}


/**************************************************************************************************
** BOOL IsCacheValid(void)
**    Validate that the cache paramaters are set, including;
**        1. is it allocated?
**        2. is cache of proper size?
**        3. does it contain the proper blocks?
**        4. does it extend past the end of the partition?
**************************************************************************************************/
BOOL
DEVICE_IO::IsCacheValid(void) const
{
    if (nullptr == m_pCache)
    { // No cache allocated means an invalid cache
        return FALSE;
    }
    else if (INVALID_BLOCK == m_CacheCurBlock.QuadPart)
    { // The currently cached block is invalid
        return FALSE;
    }
    else if ((m_CacheCurBlock.QuadPart + m_CacheBlockCount) != m_IOCurBlock.QuadPart)
    { // Ensure the next I/O block is correctly situated, just after the cached data
        return FALSE;
    }
    else if (m_CacheCurOffset >= m_CacheSize)
    { // Ensure the cache's I/O position is inside the cache boundary
        return FALSE;
    }
    else if ( ((m_IOCurPos.QuadPart / m_BlockSize) < m_CacheCurBlock.QuadPart) ||
              ((m_IOCurPos.QuadPart / m_BlockSize) >= (m_CacheCurBlock.QuadPart + m_CacheBlockCount))
            )
    { // Ensure the position is within the cache boundary
        return FALSE;
    }
    else if (((m_CacheCurBlock.QuadPart * m_BlockSize) + m_CacheCurOffset) >= m_IOSize.QuadPart)
    { // Ensure the cache start is within the full disk boundary
        return FALSE;
    }
    else if ((m_CacheCurBlock.QuadPart + m_CacheBlockCount) > m_IOBlockCount.QuadPart)
    { // Ensure the cache's end is within the full disk boundary
        return FALSE;
    }
    else
    {
        return TRUE;
    }

}


// // // // // // // // // // // // // // // // //
// // // //     Partition functions    // // // //
// // // // // // // // // // // // // // // // //
/**************************************************************************************************
** UINT GetPartitionCount(void)
**    Returns the number of partitions in a RAW device.
**    If return is zero, user should call GetError() for the appropriate failuire code.
**************************************************************************************************/
UINT
DEVICE_IO::GetPartitionCount(void)
{
    UINT ret = 0;

    m_LastError = IO_OK;
    if ( (RAW_DEVICE_TYPE != m_Type)
        && (REMOVABLE_MEDIA_DEVICE_TYPE != m_Type) )
    {
        m_LastError = IO_ERROR_INVALID_METHOD_USED;
    }
    else if (IsDeviceReady())
    {
        ret = m_pDriveLayout->PartitionCount;
        if (0 == ret)
        {
            m_LastError = IO_ERROR_DISK_NO_PARTITIONS;
        }

    }

    return ret;
}


/**************************************************************************************************
** PARTITION_TYPE GetCurrentPartitionType(void)
**    Returns the partition type of the selected partition on a physical device.  Physical devices
**    typically contain one or more partitions. A partition type will be one of the enumerated
**    PATITION_STYLE constants, defined in "\minkernel\published\sdk\NTDDDISK.W".
**      If the currently patition is not a valid type then the function fails with
**        {IO_ERROR_UNSUPPORTED_PARTITION_LAYOUT}.
**      If the instantiated class is not a physical device, then the type will be {PLAIN_FILE_DEVICETYPE},
**        which results in failure with error {IO_ERROR_INVALID_METHOD_USED}.
**      If there is no partition set, the function fails with error {IO_ERROR_PARTITION_NOT_SET}
**************************************************************************************************/
PARTITION_STYLE
DEVICE_IO::GetCurrentPartitionType(void)
{
    PARTITION_STYLE ret = PARTITION_STYLE_RAW;

    if ( (RAW_DEVICE_TYPE != m_Type)
        && (REMOVABLE_MEDIA_DEVICE_TYPE != m_Type) )
    {
        m_LastError = IO_ERROR_INVALID_METHOD_USED;
    }
    else if (IsIoReady())
    {
        m_LastError = IO_OK;
        ret = m_pCurrentPartition->PartitionStyle;
        if (ret != PARTITION_STYLE_GPT)
        {
            m_LastError = IO_ERROR_UNSUPPORTED_PARTITION_LAYOUT;
        }

    }

    return ret;
}


/**************************************************************************************************
** wstring GetCurrentPartitionName(void)
**    Returns the "text" name of the current (selected) partion on a device.
**    The return is always a wstring with some text which includes:
**      "unsupported" - the device type is not supported
**      "unselected" - there is no partition selected
**      "invalid" - the selected partition is not a valid or a supported type
**************************************************************************************************/
wstring
DEVICE_IO::GetCurrentPartitionName(void) const
{
    wstring ret;

    if ( (RAW_DEVICE_TYPE != m_Type) && (REMOVABLE_MEDIA_DEVICE_TYPE != m_Type) )
    {
        ret = L"unsupported";
    }
    else if ( (m_Handle != INVALID_HANDLE_VALUE) && (m_pDriveLayout != nullptr) )
    {
        if (m_pCurrentPartition == nullptr)
        {
            ret = L"unselected";
        }
        else
        {
            switch (m_pCurrentPartition->PartitionStyle)
            {
                case PARTITION_STYLE_GPT:
                    ret = m_pCurrentPartition->Gpt.Name;
                    break;

                case PARTITION_STYLE_MBR:
                    { // Since MBR partitions have no names, create one using a prefix string and the partition number (field)
                        WCHAR val[MAX_PARTITION_INDEX_STRING_SIZE] = { 0 };

                        ret = MBR_PARTITION_NAME_STRING;
                        _itow(m_ndxCurrentPartition, val, 10);
                        ret.append(val);
                    }
                    break;

                default:
                    ret = L"invalid";
                    break;
            }

        }

    }

    return ret;
}


/**************************************************************************************************
** wstring GetCurrentPartitionGUID(void)
**    Returns the "text" GUID of the current (selected) partion on a RAW device.
**    The return is always a wstring with some text, not always a GUID.
**    If the IO type is not RAW, we get a string with a null GUID value.
**************************************************************************************************/
wstring
DEVICE_IO::GetCurrentPartitionGUID(void) const
{
    wstring ret = L"unsupported";

    if ((REMOVABLE_MEDIA_DEVICE_TYPE == m_Type) || (RAW_DEVICE_TYPE == m_Type) )
    {
        if (m_pCurrentPartition == nullptr)
        {
            ret = GUID_STRING_TEMPLATE;
        }
        else if (m_pCurrentPartition->PartitionStyle == PARTITION_STYLE_GPT)
        {
            const size_t GUID_STRING_TEMPLATE_SIZE = sizeof GUID_STRING_TEMPLATE;
            WCHAR guid[GUID_STRING_TEMPLATE_SIZE];

            StringCchPrintfW(guid, _countof(guid),
                GUID_STRING_FORMAT,
                m_pCurrentPartition->Gpt.PartitionType.Data1,
                m_pCurrentPartition->Gpt.PartitionType.Data2,
                m_pCurrentPartition->Gpt.PartitionType.Data3,
                m_pCurrentPartition->Gpt.PartitionType.Data4[0],
                m_pCurrentPartition->Gpt.PartitionType.Data4[1],
                m_pCurrentPartition->Gpt.PartitionType.Data4[2],
                m_pCurrentPartition->Gpt.PartitionType.Data4[3],
                m_pCurrentPartition->Gpt.PartitionType.Data4[4],
                m_pCurrentPartition->Gpt.PartitionType.Data4[5],
                m_pCurrentPartition->Gpt.PartitionType.Data4[6],
                m_pCurrentPartition->Gpt.PartitionType.Data4[7]
            );
            ret = guid;
        }

    }

    return ret;
}


// // // // // // // // // // // // // // //
// // //    Open/Close functions    // // //
// // // // // // // // // // // // // // //
/**************************************************************************************************
** HRESULT Open(void)
**    Public wrapper for opening a physical device or file, using the m_Name string. The m_Name
**    string should be set before making this call using SetFileNmae() or SetFileID() or via the
**    constructor.
** NOTE: If the device or file is already opened, this call will fail and set the error code to
**       IO_ERROR_ALREADY_OPENED.
**************************************************************************************************/
HRESULT
DEVICE_IO::Open(void)
{
    if (IsDeviceReady())
    {
        m_LastError = IO_ERROR_ALREADY_OPENED;
        return E_FAIL;
    }

    return OpenPhysicalDisk();
}


/**************************************************************************************************
** HRESULT Open(wstring fName)
**    Public wrapper for opening a physical device or file, using a string.  The string should be
**    a fully qualified file name since the IO will assume the current directory which is not
**    always known at run-time.
**************************************************************************************************/
HRESULT
DEVICE_IO::Open(_In_ wstring fName)
{
    HRESULT ret = E_FAIL;

    if (IsDeviceReady())
    {
        m_LastError = IO_ERROR_ALREADY_OPENED;
    }
    else if (!fName.empty())
    {
        if (SUCCEEDED(SetDeviceName(fName)))
        {
            ret = OpenPhysicalDisk();
        }
    }
    else
    {
        m_LastError = IO_ERROR_INVALID_DEVICE_NAME;
    }

    return ret;
}


/**************************************************************************************************
** HRESULT Open(UINT devID)
**    Public wrapper for opening a physical device using an ID.  This function converts the ID
**    value into a physical device name using the PHYSICIAL_DEVICE_STRING and sppending the value.
**************************************************************************************************/
HRESULT
DEVICE_IO::Open(_In_ UINT devID)
{
    HRESULT ret = E_FAIL;

    if (IsDeviceReady())
    {
        m_LastError = IO_ERROR_ALREADY_OPENED;
    }
    else if (INVALID_DEVICE_ID != devID)
    {
        if (SUCCEEDED(SetDeviceID(devID)))
        {
            ret = OpenPhysicalDisk();
        }
    }
    else
    {
        m_LastError = IO_ERROR_INVALID_DEVICE_ID;
    }

    return ret;
}


/**************************************************************************************************
** HRESULT  Close(void)
**    Function for closing a physical device or file.  Closing means to free allocated memory
**    used to track the drive layout and to close the open handle to the device or file.
**    Closing a device does not require releasing the cache since it is possible to re-open and
**    require the same size cache. It is more efficient to retain this large chunk of memory and
**    if a larger cache is needed it can be reallocated.
**************************************************************************************************/
HRESULT
DEVICE_IO::Close(void)
{
    HRESULT ret = S_OK;

    FreeCache();
    m_pCurrentPartition = nullptr;
    m_ndxCurrentPartition = INVALID_INDEX;
    m_CurrentPartitionBlockCount = { 0 };
    m_IOSize = { 0 };
    m_BlockSize = DEFAULT_BLOCK_SIZE;

    if (nullptr != m_pDriveLayout)
    {
        free(m_pDriveLayout);
        m_pDriveLayout = nullptr;
    }

    if (INVALID_HANDLE_VALUE != m_Handle)
    {
        if (FALSE == CloseHandle(m_Handle))
        { // Failed to close
            ret = HRESULT_FROM_WIN32(GetLastError());
        }
        else
        {
            m_Handle = INVALID_HANDLE_VALUE;
        }

    }

    return ret;
}


/**************************************************************************************************
** HRESULT  OpenPhysicalDisk(void)
**    Create an I/O handle and read the device geometry or file size
**    Return is relevant to obtaining a handle and getting the geometry or size
**    This helper fuction assumes the device name/id is set before it is called.
**************************************************************************************************/
HRESULT
DEVICE_IO::OpenPhysicalDisk(void)
{
    HRESULT  ret = E_FAIL;

    if (m_Type == UNINITIALIZED_DEVICE_TYPE)
    {
        if (m_ID != INVALID_DEVICE_ID)
        {
            ret = SetDeviceID(m_ID);
        }
        else if (!m_Name.empty())
        {
            ret = SetDeviceName(m_Name);
        }

    }
    else if (m_Type != UNSUPPORTED_DEVICE_TYPE)
    {
        m_LastError = IO_OK;
        ret = S_OK;
    }

    if (SUCCEEDED(ret))
    {
        m_Handle = CreateFileW(
            m_Name.c_str(),
            FILE_GENERIC_READ | FILE_GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            ( ((RAW_DEVICE_TYPE == m_Type) || (REMOVABLE_MEDIA_DEVICE_TYPE == m_Type)) ? OPEN_EXISTING : OPEN_ALWAYS),
            0,
            NULL);

        if (m_Handle == INVALID_HANDLE_VALUE)
        {
            m_LastError = IO_ERROR_INVALID_HANDLE;
            ret = HRESULT_FROM_WIN32(GetLastError());
        }
        else
        {
            switch (m_Type)
            {
                case RAW_DEVICE_TYPE:
                case REMOVABLE_MEDIA_DEVICE_TYPE:
                    if (FAILED(ReadDiskGeometry()))
                    { // ReadDiskGeometry() sets m_LastError
                        ret = E_FAIL;
                    }
                    else if (FAILED(ReadDiskLayout()))
                    { // ReadDiskLayout() sets m_LastError
                        ret = E_FAIL;
                    }
                    break;

                case PLAIN_FILE_DEVICE_TYPE:
                    if (FALSE == GetFileSizeEx(m_Handle, (PLARGE_INTEGER)&m_IOSize))
                    { // Failed
                        m_LastError = IO_ERROR_INVALID_FILE_SIZE;
                        ret = HRESULT_FROM_WIN32(GetLastError());
                    }
                    else if (m_IOSize.QuadPart > 0)
                    {
                        SetIOBlockCount();
                    }
                    break;

                default:
                    ret = E_FAIL;
                    break;
            }

        }

    }

    return ret;
}


// // // // // // // // // // // // // // // //
// // //   Public Partition Control   // // //
// // // // // // // // // // // // // // // //
/**************************************************************************************************
** HRESULT  SetName(_In_ wstring fName)
**    Sets the name property from a wstring - based on the name (string) it can be determined
**    if the device type is disk or file.
**************************************************************************************************/
HRESULT
DEVICE_IO::SetDeviceName(_In_ wstring fName)
{
    HRESULT ret = E_FAIL;

    m_LastError = IO_ERROR_SET_NAME_ON_OPENED_DEVICE;

    if (INVALID_HANDLE_VALUE == m_Handle)
    {
        m_ID = INVALID_DEVICE_ID;
        if (fName.empty())
        {
            m_Name.erase();
            m_LastError = IO_ERROR_INVALID_DEVICE_NAME;
        }
        else
        {
            m_Name = fName;
            ret = S_OK;
            if ( 0 == _wcsnicmp( m_Name.c_str(), PHYSICAL_DEVICE_STRING, wcslen(PHYSICAL_DEVICE_STRING)) )
            {
                m_Type = RAW_DEVICE_TYPE;
            }
            else
            {
                m_Type = PLAIN_FILE_DEVICE_TYPE;
            }

        }

    }

    return ret;
}


/**************************************************************************************************
** HRESULT  SetDeviceID(_In_ UINT devID)
**    Sets the ID property from an UINT - disk devices use a number at the end of a string
**    Appending a value to a string (class) should never fail.
**************************************************************************************************/
HRESULT
DEVICE_IO::SetDeviceID(_In_ UINT devID)
{
    HRESULT ret = E_FAIL;

    m_LastError = IO_ERROR_SET_ID_ON_OPENED_DEVICE;

    if (INVALID_HANDLE_VALUE == m_Handle)
    {
        m_Name.clear();
        m_LastError = IO_ERROR_INVALID_DEVICE_ID;

        if( devID < MAX_DEV_ID_VALUE )
        {
            WCHAR val[MAX_DEV_ID_STRING_SIZE] = { 0 };

            m_Name = PHYSICAL_DEVICE_STRING;
            _itow_s(devID, val, MAX_DEV_ID_STRING_SIZE, 10);
            m_Name.append(val);

            m_LastError = IO_OK;
            m_ID = devID;
            m_Type = RAW_DEVICE_TYPE;
            ret = S_OK;
        }
        else
        {
            m_ID = INVALID_DEVICE_ID;
        }

    }

    return ret;
}


// // // // // // // // // // // // // // // // //
// // // //     Position functions     // // // //
// // // // // // // // // // // // // // // // //
// HRESULT        GetPos(_Inout_ ULONGLONG *ullPos)
HRESULT
DEVICE_IO::GetPos(_Inout_ ULONGLONG *ullPos)
{
    HRESULT ret = E_FAIL;

    m_LastError = IO_ERROR_GET_POSITION_FAILED;

    if (nullptr == ullPos)
    {
        m_LastError = IO_ERROR_NULL_POINTER;
    }
    else
    {
        *ullPos = 0;
        m_LastError = IO_OK;
        switch (m_Type)
        {
            case RAW_DEVICE_TYPE:
            case REMOVABLE_MEDIA_DEVICE_TYPE:
                if (IsIoReady())
                {
                    *ullPos = m_IOCurPos.QuadPart;
                    ret = S_OK;
                }
                break;

            case PLAIN_FILE_DEVICE_TYPE:
                if (IsDeviceReady())
                {
                    *ullPos = m_IOCurPos.QuadPart;
                    ret = S_OK;
                }
                break;

            default:
                m_LastError = IO_ERROR_UNSUPPORTED_DEVICE_TYPE;
                break;

        }

    }

    return ret;
}


//  HRESULT            GetIoPos(_Inout_ ULONGLONG *curPos)
HRESULT
DEVICE_IO::GetIoPos(_Inout_ ULONGLONG *curPos)
{
    HRESULT ret = E_FAIL;
    LARGE_INTEGER filePos = { 0 };

    if (nullptr == curPos)
    {
        m_LastError = IO_ERROR_NULL_POINTER;
    }
    else
    {
        *curPos = 0;
        m_LastError = IO_ERROR_GET_POSITION_FAILED;
        if (IsDeviceReady())
        { // Only make the call if there is a sucessful open and ready
            if (FALSE != SetFilePointerEx(m_Handle, LARGE_INTEGER{ 0 }, &filePos, FILE_CURRENT))
            { // success is non-zero
                *curPos = filePos.QuadPart;
                m_LastError = IO_OK;
                ret = S_OK;
            }

        }

    }

    return ret;
}


// // // // // // // // // // // // // // // // //
// // //  Partition selection functions   // // //
// // // // // // // // // // // // // // // // //
/**************************************************************************************************
** PRIVATE - return a pointer to the "ndx" partition entry. This function
** works for both GPT and MBR since both partition schemes use an array of
** partition entries for a partition table.  Since devices do not use MBR,
** it is unsupported here.
**   Success, a reference to a partition table entry is returned
**   Failure, a null pointer is returned and m_LastError is set to the cause.
***************************************************************************************************/
PPARTITION_INFORMATION_EX
DEVICE_IO::GetPartitionIndex(_In_ UINT ndx)
{
    PPARTITION_INFORMATION_EX ret = nullptr;

    if ( IsDeviceReady() )
    {
        if ((RAW_DEVICE_TYPE != m_Type) 
            && (REMOVABLE_MEDIA_DEVICE_TYPE != m_Type))
        {
            m_LastError = IO_ERROR_INVALID_METHOD_USED;
        }
        else if (0 == m_pDriveLayout->PartitionCount)
        {
            m_LastError = IO_ERROR_DISK_NO_PARTITIONS;
        }
        else if (ndx >= m_pDriveLayout->PartitionCount)
        {
            m_LastError = IO_ERROR_INVALID_PARTITION_INDEX;
        }
        else
        {
            ret = m_pDriveLayout->PartitionEntry;
            if (nullptr == ret)
            {
                m_LastError = IO_ERROR_INVALID_PARTITION_TABLE;
            }
            else
            {
                m_LastError = IO_OK;
                ret += ndx;
                switch (m_pDriveLayout->PartitionStyle)
                {
                    case PARTITION_STYLE_GPT:
                        break;

                    case PARTITION_STYLE_MBR:
                    default:
                        ret = nullptr;
                        m_LastError = IO_ERROR_UNSUPPORTED_PARTITION_TYPE;
                        break;
                }

            }

        }

    }

    return ret;
}

/***************************************************************************************************
** PUBLIC - set partition by index, indexes begin at zero.
**  Keep in mind that the partition entry (struct) may be non-zero based.
***************************************************************************************************/
HRESULT
DEVICE_IO::SetPartition(_In_ UINT ndx)
{
    HRESULT ret = E_FAIL;

    m_CacheCurBlock.QuadPart = INVALID_BLOCK;
    m_CacheCurOffset = 0;
    m_pCurrentPartition = nullptr;
    m_CurrentPartitionBlockCount = { 0 };
    m_ndxCurrentPartition = INVALID_INDEX;

    m_pCurrentPartition = GetPartitionIndex(ndx);
    if (nullptr != m_pCurrentPartition)
    {
        m_ndxCurrentPartition = ndx;
        SetPartitionGeometry();
        SetIoPosition(0);
        if (FALSE != AllocateCache(DEFAULT_CACHE_BLOCK_COUNT))
        {
            ret = S_OK;
        }

    }

    return ret;
}

/***************************************************************************************************
** PUBLIC - On GUID devices, set the current partition and get a pointer to
** the entry in the partition array.  GPT partitions use GUID and name to
** identify the partition while MBR has no GUID or name.  This function uses
** the PARTITION_NAME enum for well known partitions.
****************************************************************************************************/
HRESULT
DEVICE_IO::SetPartition(_In_ PARTITION_NAME nameEnum)
{
    HRESULT ret = E_FAIL;

    switch (nameEnum)
    {
        case SVRAWDUMP:
            ret = SetPartitionEntry(nullptr, SVRAWDUMP_PARTITION_GUID, PARTITION_FIELD_TYPE_GUID);
            break;

        case CRASHDUMP:
            ret = SetPartitionEntry(L"CrashDump", (GUID) { 0 }, PARTITION_FIELD_NAME);
            break;

        case EFIESP:
            ret = SetPartitionEntry(L"EFIESP", (GUID) { 0 }, PARTITION_FIELD_NAME);
            break;

        case MAINOS:
            ret = SetPartitionEntry(L"MainOS", (GUID) { 0 }, PARTITION_FIELD_NAME);
            break;

        case DATA:
            ret = SetPartitionEntry(L"Data", (GUID) { 0 }, PARTITION_FIELD_NAME);
            break;

        case NONE:
            m_CacheCurBlock.QuadPart = INVALID_BLOCK;
            m_CacheCurOffset = 0;
            m_pCurrentPartition = nullptr;
            m_CurrentPartitionBlockCount = { 0 };
            m_ndxCurrentPartition = INVALID_INDEX;
            m_LastError = IO_ERROR_PARTITION_NOT_SET;
            ret = S_OK;
            break;

        default:
            m_LastError = IO_ERROR_INVALID_METHOD_USED;
            break;
    }

    return ret;
}

/***************************************************************************************************
** PUBLIC - set partition by name (PCHAR). This is only for GPT partitions.
****************************************************************************************************/
HRESULT
DEVICE_IO::SetPartition(_In_z_ PCHAR pName)
{
    HRESULT ret = E_FAIL;

    if (nullptr == pName)
    {
        m_LastError = IO_ERROR_NULL_POINTER;
    }
    else if ( IsDeviceReady() )
    {
        if ((RAW_DEVICE_TYPE != m_Type) 
             && (REMOVABLE_MEDIA_DEVICE_TYPE != m_Type))
        {
            m_LastError = IO_ERROR_INVALID_METHOD_USED;
        }
        else
        {
            const size_t PartitionNameSize = _countof(PARTITION_INFORMATION_EX::Gpt.Name);
            WCHAR partName[PartitionNameSize + 1] = { 0 };

            if ( 0 != mbstowcs_s(NULL, partName, PartitionNameSize, pName, strlen(pName)) )
            {
                m_LastError = IO_ERROR_INVALID_PARTITION_NAME;
            }
            else
            {
                ret = SetPartitionEntry(partName, (GUID) { 0 }, PARTITION_FIELD_NAME);
            }

        }

    }

    return ret;
}

/***************************************************************************************************
** PUBLIC - set partition by GUID. This is only for GPT partitions.
****************************************************************************************************/
HRESULT
DEVICE_IO::SetPartition(_In_ GUID guid)
{
    return SetPartitionEntry(nullptr, guid, PARTITION_FIELD_TYPE_GUID);
}

/***************************************************************************************************
** PRIVATE - set partition by name or type (GUID).  This is commmon code
** for GPT based partitions.
****************************************************************************************************/
HRESULT
DEVICE_IO::SetPartitionEntry(_In_z_ PWCHAR pName, _In_ GUID guid, _In_ PARTITION_FIELD partitionField)
{
    HRESULT ret = E_FAIL;

    if ( IsDeviceReady() )
    {
        m_CacheCurBlock.QuadPart = INVALID_BLOCK;
        m_CacheCurOffset = 0;
        m_pCurrentPartition = nullptr;
        m_CurrentPartitionBlockCount = { 0 };
        m_ndxCurrentPartition = INVALID_INDEX;

        if ((RAW_DEVICE_TYPE != m_Type) 
             && (REMOVABLE_MEDIA_DEVICE_TYPE != m_Type))
        {
            m_LastError = IO_ERROR_INVALID_METHOD_USED;
        }
        else if ((PARTITION_FIELD_TYPE_GUID == partitionField) && ((GUID) { 0 } == guid))
        {
            m_LastError = IO_ERROR_INVALID_PARTITION_TYPE_GUID;
        }
        else if ( (PARTITION_FIELD_NAME == partitionField) && (nullptr == pName) )
        {
            m_LastError = IO_ERROR_INVALID_PARTITION_NAME;
        }
        else
        {
            UINT lPartitionCount = GetPartitionCount();

            if (0 == lPartitionCount)
            {
                m_LastError = IO_ERROR_DISK_NO_PARTITIONS;
            }
            else
            {
                m_LastError = IO_ERROR_PARTITION_NOT_SET;
                for (UINT i = 0; i < lPartitionCount; i++)
                {
                    m_pCurrentPartition = GetPartitionIndex(i);
                    if (nullptr == m_pCurrentPartition)
                    {
                        break;
                    }

                    if ((PARTITION_FIELD_NAME == partitionField)
                        && (0 == wcscmp(m_pCurrentPartition->Gpt.Name, pName)))
                    {
                        m_ndxCurrentPartition = i;
                        break;
                    }
                    else if ((PARTITION_FIELD_TYPE_GUID == partitionField)
                        && (0 == memcmp(&m_pCurrentPartition->Gpt.PartitionType, &guid, sizeof(GUID))))
                    {
                        m_ndxCurrentPartition = i;
                        break;
                    }
                    else
                    {
                        m_pCurrentPartition = nullptr;
                    }

                }

                if (nullptr != m_pCurrentPartition)
                {
                    SetPartitionGeometry();
                    SetIoPosition(0);
                    if (FALSE != AllocateCache(DEFAULT_CACHE_BLOCK_COUNT))
                    {
                        ret = S_OK;
                    }

                }

            }

        }

    }

    return ret;
}



// // // // // // // // // // // // // //
// // //    Seek Functionality   // // //
// // // // // // // // // // // // // //
/*************************************************************************************************
** HRESULT SetFileOffset(_In_ ULONGLONG newPos)
**    Function to set the position in a regular file.  This adjusts the file (I/O) pointer and
**    sets the m_IOCurPos.
**************************************************************************************************/
HRESULT
DEVICE_IO::SetFileOffset(_In_ ULONGLONG newPos)
{
    ULONGLONG       currPosition;
    HRESULT         ret = E_FAIL;

    if (FAILED(GetIoPos(&currPosition)))
    {
        m_LastError = IO_ERROR_SET_POSITION_FAILED;
    }
    else if (newPos == currPosition)
    {
        ret = S_OK;
    }
    else
    {
        LARGE_INTEGER   moveOffset;
        DWORD           dwMoveMethod = FILE_CURRENT;

        moveOffset.QuadPart = newPos;
        if (newPos >= m_IOSize.QuadPart)
        { // Moves to the end (or past the end) of the file
            dwMoveMethod = FILE_END;
            moveOffset.QuadPart -= m_IOSize.QuadPart;
        }
        else
        { // Move beyond the current position
            if (newPos > currPosition)
            { // Move forward, towards the file end
                moveOffset.QuadPart -= currPosition;
            }
            else
            { // Move from the beginning
                dwMoveMethod = FILE_BEGIN;
            }
        }

        if (FALSE == SetFilePointerEx(
            m_Handle,                       // _In_      HANDLE         hFile,
            moveOffset,                     // _In_      LARGE_INTEGER  liDistanceToMove,
            (PLARGE_INTEGER)&m_IOCurPos,    // _Out_opt_ PLARGE_INTEGER lpNewFilePointer,
            dwMoveMethod))                  // _In_      DWORD          dwMoveMethod
        {
            m_LastError = IO_ERROR_SET_POSITION_FAILED;
        }
        else
        {
            ret = S_OK;
            if (m_IOCurPos.QuadPart >= m_IOSize.QuadPart)
            { // Do not fail the call, but note the position is past the file's end
                m_LastError = IO_ERROR_EOF;
            }
            else
            {
                m_LastError = IO_OK;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
** HRESULT  DEVICE_IO::SetIoPosition(ULONGLONG newPos)
**    This function adjusts the file pointer to the new position (newPos) where
**    this value is taken as the absolute value of the position on the device/file
**    from the beginning. Therefore, this function must calculate a relative move
**    in order to use the FILE_CURRENT method of pointer adjustment.
**************************************************************************************************/
HRESULT
DEVICE_IO::SetIoPosition(_In_ ULONGLONG newPos)
{
    HRESULT ret = E_FAIL;

    m_LastError = IO_ERROR_INVALID_POSITION;
    if ( IsDeviceReady() )
    {
        switch (m_Type)
        { // change position
            case RAW_DEVICE_TYPE:
            case REMOVABLE_MEDIA_DEVICE_TYPE:
                if (!IsPositionValid(newPos))
                {
                    if (m_LastError == IO_ERROR_EOF)
                    { // OK to set if position is invalid because we are moving to EOF
                        m_IOCurPos.QuadPart = m_CurrentPartitionBlockCount.QuadPart;
                    }

                }
                else
                { // Propsoed position is valid, set the position
                    m_IOCurPos.QuadPart = newPos;
                    m_LastError = IO_OK;
                    ret = S_OK;
                }
                break;

            case PLAIN_FILE_DEVICE_TYPE:
                ret = SetFileOffset(newPos);
                break;

            default:
                m_LastError = IO_ERROR_UNSUPPORTED_DEVICE_TYPE;
                break;
        }

    }

    return ret;
}

/*************************************************************************************************
** HRESULT SetPos(ULONGLONG fPos)
**     Position (fPos) is an absolute value from the beginning of the partition
**************************************************************************************************/
HRESULT
DEVICE_IO::SetPos(_In_ ULONGLONG fPos)
{
    return SetIoPosition(fPos);
}

/*************************************************************************************************
** HRESULT SetPos(_In_ LARGE_INTEGER fPos)
**     Position (fPos) is an absolute value from the beginning of the partition
**************************************************************************************************/
HRESULT
DEVICE_IO::SetPos(_In_ LARGE_INTEGER fPos)
{
    return SetIoPosition((ULONGLONG)fPos.QuadPart);
}


// // // // // // // // // // // // // //
// // //   Read Functionality    // // //
// // // // // // // // // // // // // //
/*************************************************************************************************
** HRESULT DEVICE_IO::CacheRawBlocks(void)
**    Refresh the cache with new data based on the I/O pointer (m_IOCurBlock).
**    A valid cache should contain the blocks immediately preceedeing the I/O pointer.
**    The function prevents reading data beyond the end of the selected partition.
**    By using MoveToDeviceBLock(), the cache read is clamped to data from block offset
**    (m_CurrentPartitionSize - m_CacheSize) for a size of m_CacheSize. The cache offset
**    (m_CacheOffset) is adjusted to reflect the clamping operation.
**************************************************************************************************/
HRESULT
DEVICE_IO::CacheRawBlocks (void)
{
    HRESULT ret = S_OK;

    m_LastError = IO_OK;
    if ((nullptr == m_pCache) && !AllocateCache (0))   // allocate a cache if none is present
    { // cache allocation failure
        m_LastError = IO_ERROR_CACHE_READ;
        ret = E_FAIL;
    }
    else if (m_IOCurPos.QuadPart >= GetCurrentPartitionSize ())
    { // Boundary condition - at the end of the partition
        m_LastError = IO_ERROR_EOF;
        ret = E_FAIL;
    }
    else if (!IsCacheValid())
    { // Perform a cache refresh if cache is not valid
        size_t       bytesRead;
        ULONGLONG    IOBlock = m_IOCurPos.QuadPart / m_BlockSize;

        // Set the I/O pointer
        ret = MoveToDeviceBlock (IOBlock);
        if (SUCCEEDED (ret))
        { // Cache the blocks if everything is set correctly
            ret = ReadBlocksFromDevice (m_pCache, m_CacheSize, &bytesRead);
            if (SUCCEEDED(ret) && (bytesRead == m_CacheSize))
            {
                m_CacheCurBlock.QuadPart = m_IOCurBlock.QuadPart;
                m_IOCurBlock.QuadPart += m_CacheBlockCount;
                m_CacheCurOffset = GetCacheOffset();
            }
            else
            {
                ret = E_FAIL;
            }

        }

    }
    else
    { // If the cache is valid, only need to reset the offset
        m_CacheCurOffset = GetCacheOffset();
    }

    return ret;
}


/*************************************************************************************************
** HRESULT  MoveToDeviceBlock(_In_ ULONGLONG newPartitionBlock)
**    Function to set the I/O pointer to a new location which must be an even block boundary.
**    This is a UFS accomodation since that file system cannot be distinguished from other
**    device file systems, UFS only allows for read/write operations at block boundaries.
**    Blocks size is determined by the GetDiskGeometry() method.
**    To ensure we are not moving the I/O pointer beyond the partition boundary, several
**    checks are used to clamp the block position.  The objective is to ensure we are not
**    trying to move before the beginning of the partition (block 0) or past the end of
**    the partition (m_CurrentPartitionBlockCount and/or m_pCurrentPartition->PartitionLength).
**    Because a cache is implemented, this function clamps the block move operation to block
**    (m_CurrentPartitionSize - m_CacheSize) becasue a cache read operation will always read
**    m_CacheSize bytes (or m_CacheBlockCOunt blocks).  Reads beyond the end of the partition
**    are prevented by this clamping functionality.
*************************************************************************************************/
HRESULT
DEVICE_IO::MoveToDeviceBlock (_In_ ULONGLONG newPartitionBlock)
{
    HRESULT        ret = E_FAIL;

    if (newPartitionBlock > m_CurrentPartitionBlockCount.QuadPart)
    { // invalid offset - cannot go past the end of the partition
        m_LastError = IO_ERROR_INVALID_POSITION;
    }
    else if (newPartitionBlock == m_CurrentPartitionBlockCount.QuadPart)
    { // Special case - set position to the end of the partition
      // Moving to the end of the partition is allowed but will return an error,
      // there is no need to set device I/O position since this is detected by read/write.
        m_LastError = IO_ERROR_EOF;        // Mark this move as an EOF - by default
        m_IOCurBlock = m_CurrentPartitionBlockCount;  // last block on device
        m_IOCurPos = m_IOSize;             // position to the end of the partition
    }
    else if (IsIoReady())
    { // the requested block is within the partition's boundaries
        ULONGLONG   currBlockOffset;

        m_LastError = IO_OK;
        if (SUCCEEDED(GetIoPos(&currBlockOffset)))
        {
            LARGE_INTEGER   newBlockOffset = m_pCurrentPartition->StartingOffset;

            if (newPartitionBlock > (m_CurrentPartitionBlockCount.QuadPart - m_CacheBlockCount))
            { // Clamp the partition to the last block such that a cache read never goes beyond the partition's end
                newBlockOffset.QuadPart += (m_pCurrentPartition->PartitionLength.QuadPart - m_CacheSize);
            }
            else
            {
                newBlockOffset.QuadPart += newPartitionBlock * m_BlockSize;
            }

            if ((ULONGLONG)newBlockOffset.QuadPart != currBlockOffset)
            { // move the I/O pointer if the requested block is not at the current position
                DWORD dwMoveMethod = FILE_BEGIN;    // default is to move from the beginning of the partition

                if (!(currBlockOffset < (ULONGLONG)m_pCurrentPartition->StartingOffset.QuadPart) &&
                    ((ULONGLONG)newBlockOffset.QuadPart > currBlockOffset)
                    )
                { // Make this relative to the current position if the curent position is within the partition and the new
                  // position is beyond the current position.  Clamping (above) prevents going beyond the partition's end.
                    newBlockOffset.QuadPart -= currBlockOffset;
                    dwMoveMethod = FILE_CURRENT;
                }

                if (FALSE != SetFilePointerEx (
                    m_Handle,                       // _In_      HANDLE         hFile,
                    newBlockOffset,                 // _In_      LARGE_INTEGER  liDistanceToMove,
                    (PLARGE_INTEGER)&m_IOCurBlock,  // _Out_opt_ PLARGE_INTEGER lpNewFilePointer,
                    dwMoveMethod)                   // _In_      DWORD          dwMoveMethod
                    )
                { // Position moved successfully
                    m_IOCurBlock.QuadPart -= m_pCurrentPartition->StartingOffset.QuadPart;
                    m_IOCurBlock.QuadPart /= m_BlockSize;
                    ret = S_OK;
                }
                else
                { // Falied to mode I/O position
                    m_LastError = IO_ERROR_SET_POSITION_FAILED;
                }

            }
            else
            { // I/O position is already at the requested block
                m_IOCurBlock.QuadPart = newPartitionBlock;
                ret = S_OK;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
**  static HRESULT
**    SafeIO( _In_ HANDLE hdl,
**            _Inout_updates_bytes_(bufferSize) PCHAR buffer,
**            _In_ size_t bufferSize,
**            _In_ ULONG IoBlockSize,
**            _In_ IO_TYPE IO_FLAG,
**            _Out_ size_t* bytesProcessed
**          )
**  Function to wrap the Read/WriteFile() functions to protect the bufferSize parameter. For
**  both functions, the bufferSize is DWORD in size while IO operations support size_t (larger
**  size) parameters.
**
**  The callers to this (private) function use size_t which may be 64 bits (or larger) thus any
**  size other than DWORD will cause a compile warning.  If the parameter is cast down, the
**  larger value is truncated and read failures may occur where short reads or zero size reads
**  are possible for very large values.
**
**  To be safe, the input parameter to this function is size_t and a check for a value larger
**  than DWORD is performed, to prevent truncation or overflow errors. If the size is greater
**  than DWORD, this function performs multiple smaller reads using a size of MAX_DWORD or a
**  size less than DWORD (in blocks).
**
**  For devices that exist today, block sizes are of size less than DWORD and ULONG (both 4 bytes).
**  Thus, we are safe until the block size becomes larger than 4 bytes.
**
**  The algorithm is as follows:
**    1) determine the maximum size for any IO operation, using the block size
**    2) for each iteration, determine how many bytes to process (read/write)
**       a) if the required/remaining bytes is greater than the maximum
**          IO size (maxIOSize), only read maxIOSize
**       b) if the required/remaining bytes is less than the maximum
**          IO size (maxIOSize), read the remaining bytes
**
**  Parameter caveats:
**    1) there needs to be a valid handle, buffer and bufferSize.
**       If any are invalid, the function returns with ERROR_INVALID_PARAMETER.
**    2) IoBlockSize == 0 implies we are reading from a file or non-block device
**    3) IoBlockSize != 0 implies we are reading from block device where the buffer
**       needs to be a multiple of IoBlockSize.
**    4) IO_FLAG must be one of the IO_FLAG (type enum) which determines te operation herein,
**       Read or Write. If it is not one of the valid types, the function returns with
**       ERROR_INVALID_PARAMETER.
**************************************************************************************************/
static
HRESULT
SafeIO( _In_ HANDLE hdl,
        _Inout_updates_bytes_(bufferSize) PCHAR buffer,
        _In_ size_t bufferSize,
        _In_ ULONG IoBlockSize,
        _In_ IO_TYPE IO_FLAG,
        _Out_ size_t* bytesProcessed
      )
{
    HRESULT ret = S_OK;

    if(nullptr != bytesProcessed)
    {
        *bytesProcessed = 0; // always init to zero
    }

    if ( (INVALID_HANDLE_VALUE == hdl) ||
         (nullptr == buffer) ||
         ((0 != IoBlockSize) && (0 != (bufferSize % IoBlockSize))) ||
         ((IO_TYPE_READ != IO_FLAG) && (IO_TYPE_WRITE != IO_FLAG))
       )
    { // One of the paramteres is bad (see caveats above)
        ret = HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER);
    }
    else
    {
        // maxIOSize is used to determine the maximum IO size based on the IoBlockSize parameter.
        //  0 -> means use the largest possible size
        //  non-0 -> means use the largest size, in blocks
        const DWORD maxIOSize = (DWORD)( (0 == IoBlockSize) ? MAX_DWORD : (IoBlockSize * (MAX_DWORD / IoBlockSize)) );
        size_t bytesToProcess = bufferSize;
        PCHAR pBuffer = buffer;

        do
        {
            DWORD bProcessed = 0;

            // Read using the smallest chunk size possible or cap at MAX_DWORD
            DWORD bytesThisRead = (bytesToProcess > (size_t)maxIOSize) ? maxIOSize : (DWORD)bytesToProcess;

            if (
                 ( (IO_TYPE_READ == IO_FLAG)  &&    // Process a read
                   (FALSE == ReadFile(hdl, buffer, bytesThisRead, &bProcessed, NULL))
                 ) ||
                 ( (IO_TYPE_WRITE == IO_FLAG) &&    // Process a write
                   (FALSE == WriteFile(hdl, buffer, bytesThisRead, &bProcessed, NULL)) &&
                   (FALSE == FlushFileBuffers(hdl)) // Always flush after a write
                 ) ||
                 (0 == bProcessed)
               )
            { // Stop reading if any IO fails
                ret = HRESULT_FROM_WIN32(GetLastError());
                break;
            }
            else
            {
                bytesToProcess -= bProcessed;
                pBuffer += bProcessed;
            }

        } while (bytesToProcess > 0);

        if (nullptr != bytesProcessed)
        {
            *bytesProcessed = (bufferSize - bytesToProcess);
        }

    }

    return ret;
}


/*************************************************************************************************
** HRESULT ReadBlocksFromDevice(
**                       _Out_writes_bytes_(bufferSize) PCHAR buffer,
**                       _In_   size_t bufferSize,
**                       _Out_  size_t* bytesRead)
**    Function to read raw blocks into the buffer provided. This is a UFS accomodation
**    because the file system being used (on a device) cannot be distinguished and UFS only
**    allows for read/write operations from block boundaries, in multiplles of the block size.
**    The device block size is determined by the GetDiskGeometry() method.
**    This function assumes that the I/O pointer and the class property m_IOCurBlock are correct.
**    To ensure we are not reading beyond the partition boundaries, this function checks and
**    fails if the read I/O pointer is at EOF.
**    A sucessful read is when all the requested blocks (bytes) were read.
**    Partial reads, where the blocks read are less than what was requested (in bufferSize), will
**    fail with IO_ERROR_PARTIAL_READ and the bytesRead will return non-zero.
*************************************************************************************************/
HRESULT
DEVICE_IO::ReadBlocksFromDevice (_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_  size_t* bytesRead)
{
    HRESULT ret = E_FAIL;

    if(nullptr == bytesRead)
    { // fail if missing this required parameter
        m_LastError = IO_ERROR_INVALID_PARAMETER;
    }
    else
    {
        *bytesRead = 0; // always init to zero
        if (nullptr == buffer)
        { // WATCH - this buffer (pointer) may come from the caller to Read() - could be null
            m_LastError = IO_ERROR_NULL_POINTER;
        }
        else if ((0 == bufferSize) || (0 != (bufferSize % m_BlockSize)))
        { // WATCH - bufferSize may come from the caller to Read() - could be zero or not full blocks
            m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
        }
        else if (m_IOCurBlock.QuadPart >= m_CurrentPartitionBlockCount.QuadPart)
        { // no reads when I/O pointer is at (or beyond) EOF
            m_LastError = IO_ERROR_EOF;
        }
        else
        { // Process the read
            size_t bytesToRead = bufferSize; // Tracks how many bytes to read, ideally all bytes requested

            if (bytesToRead > (m_BlockSize * (m_CurrentPartitionBlockCount.QuadPart - m_IOCurBlock.QuadPart)))
            { // If reading past the end of partition, clamp the read to the remaining blocks in the partition
                bytesToRead = size_t(m_BlockSize * (m_CurrentPartitionBlockCount.QuadPart - m_IOCurBlock.QuadPart));
            }

            if (FAILED(ret = SafeIO(m_Handle, buffer, bytesToRead, m_BlockSize, IO_TYPE_READ, bytesRead)))
            { // Read failed
                m_LastError = IO_ERROR_READ_FILE;
                ret = HRESULT_FROM_WIN32 (GetLastError ());
            }
            else if (*bytesRead != bufferSize)
            { // If a full buffer was not read OR the read was short
                m_LastError = IO_ERROR_READ_PARTIAL;
            }
            else
            { // Read successful
                m_LastError = IO_OK;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
** HRESULT ReadFromBlockDevice(
**                      _Out_writes_bytes_(bufferSize) PCHAR buffer,
**                      _In_    size_t bufferSize,
**                      _Out_   size_t *bytesRead)
**    This function simulates the ReadFile() function for block based file systems (and UFS).
**    Large reads are divided into three phases;
**        Phase (1) reads any available cached data.
**        Phase (2) reads a large number of blocks which are more than the cache size.
**        Phase (3) reads any ramining data, less than the cache size.
**    In Phase 1, in order to minimize I/O, if phase 1 reads begin inside of a block
**    (block offset != 0), a cache read (refresh) is done to cache as much data as possible,
**    where the first chunk of data can be read from cache.  Subsequent reads to this phase
**    will be block aligned.  If the inital read is block aligned, this phase is skipped.
**    In the case of block aligned reads, phase 1 is skipped.
**    Phase 2 will read large blocks of data directly into the input buffer, in order to minimize
**    unnecessary cahce use.
**    In phase 3, any remaining bytes will be less than the cache size.  In this pahse, there
**    will be a cache read to obtain any remaining bytes.  The final chunk of bytes will be
**    copied from the cache and the cache shall remain valid.
**************************************************************************************************/
HRESULT
DEVICE_IO::ReadFromBlockDevice(_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesRead)
{
    HRESULT ret = E_FAIL;

    if(nullptr == bytesRead)
    { // fail if missing this required parameter
        m_LastError = IO_ERROR_INVALID_PARAMETER;
    }
    else
    {
        *bytesRead = 0;
        if (nullptr == buffer)
        { // WATCH - this buffer (pointer) comes from the caller to Read() - could be null
            m_LastError = IO_ERROR_NULL_POINTER;
        }
        else if (0 == bufferSize)
        { // WATCH - this value comes from the caller to Read() - could be zero
            m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
        }
        else if (m_IOCurPos.QuadPart >= (ULONGLONG)m_pCurrentPartition->PartitionLength.QuadPart)
        { // FAIL if attempting to read when I/O is at or past the partition's end
            m_LastError = IO_ERROR_EOF;
        }
        else
        { // 3 Phase read
            PCHAR pBuffer = buffer;
            size_t bytesRemaining = (bufferSize <= GetPartitionRemainingReadBytes()) ?  size_t(bufferSize) : size_t(GetPartitionRemainingReadBytes());

            m_LastError = IO_OK;
            ret = S_OK;

            // Phase 1 - the read begins inside of a block (not at an even block bundary).
            //   This case will first obtain a new cache then read bytes from cache. The call
            //   to refresh the cache returns with some data in the cache.  If the cache is valid,
            //   the function retuns with the cache unchanged. If the cahce is not valid, the
            //   function will read new data into the cache.
            if ( (0 != (m_IOCurPos.QuadPart % m_BlockSize)) &&
                 (SUCCEEDED(ret = CacheRawBlocks()))
               )
            { // At this point, we have a valid cache, read as many bytes as possible
                // varialbe to ensure that we do not read past the end of the cach, clamp if necessary
                size_t bytesToRead = size_t((bytesRemaining <= GetCacheRemaining()) ? bytesRemaining : GetCacheRemaining());
                if (FALSE != memcpy_s (pBuffer, bytesToRead, (m_pCache + m_CacheCurOffset), bytesToRead))
                { // in the unlikely event that copying bytes fails
                    m_LastError = IO_ERROR_READ_COPY;
                    ret = HRESULT_FROM_WIN32 (GetLastError ());
                }
                else
                { // on success, adjust pointers and counts
                    bytesRemaining -= bytesToRead;
                    pBuffer += bytesToRead;
                    m_CacheCurOffset += bytesToRead;
                    m_IOCurPos.QuadPart += bytesToRead;
                    *bytesRead = bytesToRead;
                }

            }

            // At this point, there are three possibilities (two resulting from the previous read);
            //  (1) the read begins at a block boundary (the previous operation was skipped),
            //  (2) the read was fully satisfied from cahce, in which case the remaing phases
            //      will not be performed,
            //  (3) the read exhausted the cache and there are more bytes to read, in which case
            //      the next read will continue at an even block boundary and one or both of the
            //      remaining phases will be performed.


            // Phase 2 - if there are more byte to read, the next read will check the [remaining] read size.
            //   This phase will handle a very large read where the number of bytes is in excess of the cache
            //   size.  In this case, the remaining bytes (to read) will be larger than the cahce. For this
            //   very large read, the maximum number of blocks are computed and that size is read into the
            //   buffer.  The cache is not used here since reading directly to buffer is more efficient.
            if (SUCCEEDED(ret) && (bytesRemaining > m_CacheSize))
            { // to handle a large number of blocks and they will be read directly into the buffer.
                size_t bRead;                                                      // track what was read
                size_t bytesToRead = m_BlockSize * (bytesRemaining / m_BlockSize); // determine blocks need

                if (bytesToRead > GetPartitionRemainingReadBytes())
                { // check to ensure a large read does not go beyond the partition boundary, clamp if necessary
                    bytesToRead = size_t(GetPartitionRemainingReadBytes());
                }

                // in case the Phase 1 read was skipped, make sure the I/O pointer is in the right place
                if ( SUCCEEDED(ret = MoveToDeviceBlock(m_IOCurPos.QuadPart / m_BlockSize)) &&
                     SUCCEEDED(ret = ReadBlocksFromDevice (pBuffer, bytesToRead, &bRead))
                   )
                { // Reading chunks should succeed, unless there was a partial read.
                    bytesRemaining -= bRead;
                    pBuffer += bRead;
                    m_IOCurPos.QuadPart += bRead;
                    m_IOCurBlock.QuadPart += bRead / m_BlockSize;
                    *bytesRead += bRead;
                    if (bytesToRead != bRead)
                    {
                        ret = E_FAIL;
                    }
                    // At this point, a partial read (from above) is a fatal error because of the check
                    // (and clamp) to ensure the partition boundary is respected.  If the read failed, we
                    // return with failure with the number of bytes were read in this phase.
                }
            }

            // At this point, we should be at an even block boundary becasue the reads in phase 1 or 2 leave
            // the I/O pointer at a block boundary.  Now, there is less than a cacheSize amount of data to read.

            // Phase 3 - if there are remaining bytes to be read, trigger a new cache read and copy whatever is needed from cache
            if (SUCCEEDED(ret) && (bytesRemaining > 0))
            {
                size_t bytesToRead = (bytesRemaining <= GetPartitionRemainingReadBytes()) ? size_t(bytesRemaining) : size_t(GetPartitionRemainingReadBytes());

                if ( FAILED(ret = CacheRawBlocks()) )
                { // Cache is not valid and cannot read anything into it
                    if (m_LastError != IO_ERROR_EOF)
                    { // Do not change the EOF error from the cache read
                        m_LastError = IO_ERROR_CACHE_READ;
                    }

                }
                else if (FALSE != memcpy_s (pBuffer, bytesToRead, (m_pCache + m_CacheCurOffset), bytesToRead))
                { // Falure to copy
                    m_LastError = IO_ERROR_READ_COPY;
                    ret = HRESULT_FROM_WIN32 (GetLastError ());
                }
                else
                {
                    m_CacheCurOffset += bytesToRead;
                    m_IOCurPos.QuadPart += bytesToRead;
                    *bytesRead += bytesToRead;
                }

            }

            if (SUCCEEDED(ret) && (*bytesRead != bufferSize))
            { // All the reads were successful but this resulted in a partial read
                m_LastError = IO_ERROR_READ_PARTIAL;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
** HRESULT ReadFromFile(
**                  _Out_writes_bytes_(bufferSize) PCHAR buffer,
**                  _In_ size_t     bufferSize,
**                  _Out_ size_t    *bytesRead)
**    This is a wrapper for the ReadFile() function.
**************************************************************************************************/
HRESULT
DEVICE_IO::ReadFromFile (_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesRead)
{
    HRESULT hr = E_FAIL;

    if(nullptr == bytesRead)
    { // fail if missing this required parameter
        m_LastError = IO_ERROR_INVALID_PARAMETER;
    }
    else
    {
        *bytesRead = 0;
        if (nullptr == buffer)
        { // WATCH - this buffer (pointer) comes from the caller to Read() - could be null
            m_LastError = IO_ERROR_NULL_POINTER;
        }
        else if (0 == bufferSize)
        { // WATCH - this value comes from the caller to Read() - could be zero
            m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
        }
        else
        {
            if (FAILED(hr = SafeIO(m_Handle, buffer, bufferSize, 0, IO_TYPE_READ, bytesRead)))
            {
                m_LastError = IO_ERROR_READ_FILE;
                hr = HRESULT_FROM_WIN32 (GetLastError ());
            }
            else if (0 == *bytesRead)
            {
                m_LastError = IO_ERROR_EOF;
            }
            else
            {
                m_IOCurPos.QuadPart += *bytesRead;
                if (*bytesRead != bufferSize)
                {
                    m_LastError = IO_ERROR_READ_PARTIAL;
                }
                else
                {
                    m_LastError = IO_OK;
                }

            }

        }

    }

    return hr;
}


/*************************************************************************************************
** HRESULT Read(
**            _Out_writes_bytes_(bufferSize) PCHAR buffer,
**            _In_ size_t bufferSize,
**            _Out_opt_ size_t *bytesRead)
**    Public method for users to be able to read files or block based file systems. This provides
**    a standard (uniform) interface for reading data.  The offset into the file (m_IOCurPos)
**    allows for byte orinted I/O by SetPos(), which is used to set the position.
**************************************************************************************************/
HRESULT
DEVICE_IO::Read (_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesRead)
{
    HRESULT hr = E_FAIL;

    if (nullptr != bytesRead)
    { // Always set this to zero, when not a null pointer
        *bytesRead = 0;
    }

    if (nullptr == buffer)
    { // Make sure there is a buffer to receive the read data
        m_LastError = IO_ERROR_NULL_POINTER;
    }
    else if (0 == bufferSize)
    { // Make sure there is a buffer size
        m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
    }
    else if (IsIoReady ())
    {
        size_t bRead = 0;

        m_LastError = IO_OK;
        switch (m_Type)
        {
            case RAW_DEVICE_TYPE:
            case REMOVABLE_MEDIA_DEVICE_TYPE:
                hr = ReadFromBlockDevice (buffer, bufferSize, &bRead);
                break;

            case PLAIN_FILE_DEVICE_TYPE:
                hr = ReadFromFile (buffer, bufferSize, &bRead);
                break;

            default:
                m_LastError = IO_ERROR_UNSUPPORTED_DEVICE_TYPE;
                break;
        }

        if (nullptr != bytesRead)
        { // Return a value if a pointer was passed
            *bytesRead = bRead;
        }

    }

    return hr;
}

/*************************************************************************************************
** HRESULT ReadAtOffset(
**            _Out_writes_bytes_(bufferSize) PCHAR buffer,
**            _In_ size_t bufferSize,
**            _In_ LARGE_INTEGER offset,
**            _In_ READ_EXACT_OPTIONS readExact)
**  Public method to efficiently read data from a specific offset.
**  The flag "readExact" will check for a complete read of bufferSize is read, else the call 
**  will fail as a partial read.
**************************************************************************************************/
HRESULT
DEVICE_IO::ReadAtOffset(_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _In_ LARGE_INTEGER offset, _In_ READ_EXACT_OPTIONS readExact)
{
    HRESULT hr;
    size_t  bytesRead = 0;

    if ( SUCCEEDED(hr = SetPos(offset)) &&
         SUCCEEDED(hr = Read((PCHAR)buffer, bufferSize, &bytesRead)) &&
         (READ_EXACT == readExact) &&
         (bufferSize != bytesRead)
       )
    {
        m_LastError = IO_ERROR_READ_PARTIAL;
        hr = E_FAIL;
    }

    return hr;
}

                



// // // // // // // // // // // // // //
// // //   Write Functionality   // // //
// // // // // // // // // // // // // //
/*************************************************************************************************
**  HRESULT WriteBlocksToDevice(
**                               _In_reads_bytes_(bufferSize) PCHAR buffer,
**                               _In_ size_t bufferSize,
**                               _Out_opt_ size_t *bytesWritten )
**    Function to write raw blocks, to a device, from the buffer provided. This is a UFS
**    accomodation because the file system being used (on a device) cannot be distinguished and
**    UFS only allows for read/write operations from block boundaries, in multiples of the block
**    size. The device block size is determined by the GetDiskGeometry() method.
**    It is up to the caller to correctly set the I/O pointer and m_IOCurBlock (class property).
**    To ensure we are not writing beyond the partition boundaries, this function checks and
**    fails if the read I/O pointer is at EOF.
**    A sucessful write is when all the requested blocks (bytes) were written.
**    Partial writes, where the blocks written are less than what was provided (in bufferSize),
**    will fail with IO_ERROR_PARTIAL_WRITE and the bytesWritten will return non-zero.
*************************************************************************************************/
HRESULT
DEVICE_IO::WriteBlocksToDevice(_In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesWritten)
{
    HRESULT ret = E_FAIL;

    if (nullptr == bytesWritten)
    { // fail if missing this required parameter
        m_LastError = IO_ERROR_INVALID_PARAMETER;
    }
    else
    {
        *bytesWritten = 0;  // always init to zero
        if (nullptr == buffer)
        { // WATCH - this buffer (pointer) may come from the caller to Write() - could be null
            m_LastError = IO_ERROR_NULL_POINTER;
        }
        else if ((0 == bufferSize) || (0 != (bufferSize % m_BlockSize)))
        { // WATCH - bufferSize may come from the caller to Write() - could be zero or not full blocks
            m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
        }
        else if (m_IOCurBlock.QuadPart >= m_CurrentPartitionBlockCount.QuadPart)
        { // no writes when I/O pointer is at (or beyond) EOF
            m_LastError = IO_ERROR_EOF;
        }
        else
        { // Process the write
            size_t bytesToWrite = bufferSize;

            // TODO: Bug 10033908 - to address optimizations for these calculations, in future.
            if (bytesToWrite > m_BlockSize * (m_CurrentPartitionBlockCount.QuadPart - m_IOCurBlock.QuadPart))
            {// If writing past the end of partition, clamp the write to the blocks remaing in the partition
                bytesToWrite = size_t(m_BlockSize * (m_CurrentPartitionBlockCount.QuadPart - m_IOCurBlock.QuadPart));
            }

            // Write buffer to device and check that some bytes were written.
            // SafeIO() performs a flush after write.
            if (FAILED(ret = SafeIO(m_Handle, buffer, bytesToWrite, m_BlockSize, IO_TYPE_WRITE, bytesWritten)))
            { // Failed write
                m_LastError = IO_ERROR_WRITE_FILE;
                ret = HRESULT_FROM_WIN32(GetLastError());
            }
            else if (*bytesWritten != bufferSize)
            { // PARTIAL - write was clamped OR less than requested were written
                m_LastError = IO_ERROR_WRITE_PARTIAL;
            }
            else
            {
                m_LastError = IO_OK;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
**  HRESULT WriteCacheAndFlush(_In_reads_bytes_(bufferSize) PCHAR pBuffer, _In_ size_t bufferSize)
**    Function to write bytes to the cache, from the buffer provided, and updates the
**    corresponding blocks on disk (the device). A valid cache should be present before calling
**    this function. If the cache is invalid, the function fails with IO_ERROR_CACHE_INVALID.
**    The cache must have enough free space (bytes) to accommodate the buffer provided. If the
**    cache cannot accommodate the buffer, the function fails with IO_ERROR_CACHE_WRITE.
**    Before writing, the range of (number of) blocks modified will to be calculated, based on
**    the cache block/offset and the number of bytes being modified. As an optimization, the
**    write operation moves the I/O pointer to the block on disk where the write begins.
**    Subsequently, the write operation will only write the blocks that were modified, in cache.
*************************************************************************************************/
HRESULT
DEVICE_IO::WriteCacheAndFlush(_In_reads_bytes_(bufferSize) PCHAR pBuffer, _In_ size_t bufferSize)
{
    HRESULT    ret = E_FAIL;

    if (nullptr == pBuffer)
    {
        m_LastError = IO_ERROR_NULL_POINTER;
    }
    else if (0 == bufferSize)
    {
        m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
    }
    else if (!IsCacheValid())
    {
        m_LastError = IO_ERROR_CACHE_INVALID;
    }
    else
    {
        ULONGLONG    cacheFinalSize = m_CacheCurOffset + bufferSize;
        ULONGLONG    cacheWriteBeginBlock = (ULONGLONG)m_CacheCurBlock.QuadPart + (m_CacheCurOffset / m_BlockSize);
        ULONGLONG    cacheWriteEndBlock;
        size_t       cacheWriteSize;

        if ( 0 == (cacheFinalSize % m_BlockSize) )
        { // no adjustment when wriing to a block boundary
            cacheWriteEndBlock = cacheWriteBeginBlock + (cacheFinalSize / m_BlockSize);
        }
        else
        { // need to adjust when write ends inside of a block
            cacheWriteEndBlock = cacheWriteBeginBlock + ((cacheFinalSize + m_BlockSize) / m_BlockSize);
        }

        cacheWriteSize = (ULONG)(m_BlockSize  * (cacheWriteEndBlock - cacheWriteBeginBlock));

        if (SUCCEEDED(ret = UpdateCache(pBuffer, bufferSize)))
        {
            size_t       bytesProcessed;

            if ( FAILED(ret = MoveToDeviceBlock(cacheWriteBeginBlock)) ||
                 FAILED(ret = WriteBlocksToDevice(m_pCache, cacheWriteSize, &bytesProcessed))
               )
            {
                m_LastError = IO_ERROR_CACHE_MODIFY;
            }
            else if (cacheWriteSize != bytesProcessed)
            {
                m_LastError = IO_ERROR_CACHE_WRITE;
                ret = E_FAIL;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
** HRESULT WriteToBlockDevice(
**                             _Out_writes_bytes_(bufferSize) PCHAR buffer,
**                             _In_ size_t bufferSize,
**                             _Out_  size_t *bytesWritten)
**    This function simulates the WriteFile() function for block based file systems (and UFS).
**    Large writes are divided into three phases;
**        Phase (1) non block aligned - handles beginning portion of data, up to available cache
**        Phase (2) block aligned - largest possible number of blocks (larger than the cache size)
**        Phase (3) any ramining data, less than the cache size
**    where each phase will trigger write events, to the device (or file).
**
**    For non block aligned writes, Phase 1, will write as many bytes as possible to the cache.
**      An unaligned write is when the current byte (offset) is not at an even block boundary. In
**      this case, the cahe is used as the temporary buffer for the first write.  If the write
**      is initiated with an invalid cache, a cache refresh is performed [CacheRawBlocks()] to
**      obtain a valid cache.  Bytes from the beginning of the input buffer are written to the
**      cache, from the current cache offset to the cache end. At the end of this phase, the I/O
**      will either (1) consume the input buffer or (2) fill the cahce where byte remain unwritten.
**      The former will signal the end of the write and the later will continue to phase 2 and 3
**      where the I/O buffer will be at a block boundary.
**    For block aligned writes, Phase 1 is skipped and the write commences with Phase 2.  In this
**      phase, the write operation must conntain a write buffer larger than a cache (bytes to write
**      are larger than the cache size). For this write, the maximum number of blocks are computed
**      and the data is written to the devicce (file) directly from the buffer provided.  The write
**      operation is done via WriteBlocksToDevice() which prevents block writes beyond the partition
**      boundary. The reuslt of a partial write is (1) data is written up to the partition boundary,
**      (2) WriteBlocksToDevice() returns success and the value of bytesWritten will be less than
**      the buffeSize, (3) the last error will be set to IO_ERROR_WRITE_PARTIAL.  A partial write
**      in phase 2 signals a fatal error because a full write will extend beyond the partition
**      boundary.
**    The third write (phase 3) is intended to write any remaining bytes that are less than a
**      block size.  This "tail" write will use the cache to write blocks and, on completion, will
**      leave the cache in a valid state.  This phase my trigger after either of the first two
**      phases where either phase leave the I/O pointer at an even block boundary if the input
**      buffer is not fully consumed.
**************************************************************************************************/
HRESULT
DEVICE_IO::WriteToBlockDevice(_In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesWritten)
{
    HRESULT ret = E_FAIL;

    if (nullptr == bytesWritten)
    { // fail if missing this required parameter
        m_LastError = IO_ERROR_INVALID_PARAMETER;
    }
    else
    {
        *bytesWritten = 0; // Write() will always pass in a valid pointer for this value
        if (nullptr == buffer)
        { // WATCH - this buffer (pointer) comes from the caller to Write() - could be null
            m_LastError = IO_ERROR_NULL_POINTER;
        }
        else if (0 == bufferSize)
        { // WATCH - this value comes from the caller to Write() - could be zero
            m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
        }
        else if (m_IOCurPos.QuadPart >= (ULONGLONG)m_pCurrentPartition->PartitionLength.QuadPart)
        { // FAIL if attempting to write when I/O is at or past the partition's end
            m_LastError = IO_ERROR_EOF;
        }
        else
        { // 3 phase write
            PCHAR     pBuffer = buffer;
            ULONGLONG bytesRemaining = (bufferSize <= GetPartitionRemainingReadBytes()) ? bufferSize : GetPartitionRemainingReadBytes();

            m_LastError = IO_OK;
            ret = S_OK;

            // Phase 1A - the write begins inside of a block (not at an even block bundary).
            //   This case will first obtain a valid cache to write bytes into. The call
            //   to CacheRawBlocks() returns with a valid cache when (1) existing cache is
            //   valid, (2) existing the cahce is invalid and a the cache is refreshed.
            if ((0 != (m_IOCurPos.QuadPart % m_BlockSize)) &&
                SUCCEEDED(ret = CacheRawBlocks())
                )
            { // At this point, we have a valid cache, will write as many bytes as possiblle to cache
                size_t writeSize = (bytesRemaining <= GetCacheRemaining()) ? bytesRemaining : GetCacheRemaining();
                if (SUCCEEDED(ret = WriteCacheAndFlush(pBuffer, writeSize)))
                {
                    bytesRemaining -= writeSize;
                    pBuffer += writeSize;
                    *bytesWritten = writeSize;
                }

            }
            // Phase 1B - the write begins at an even block boundary but the remaing data, to 
            //  be written, is more than the remaining cache.  In this case, we need set the 
            //  I/O to the next block and start writing blocks.
            //  Use and I/O pointer move (which also invalidates the cache) and allow the 
            //  remaining phases to proceede.
            //  Phase 2 will write chunks, provided they are larger than tha cache.
            //  Phase 3 will wrtie the chunk smaller than a full cahce
            else if ( (GetCacheRemaining() > 0) &&
                      (GetCacheRemaining() < bytesRemaining) &&
                      FAILED(ret = MoveToDeviceBlock(m_IOCurPos.QuadPart / m_BlockSize))
                    )
            {
                ret = E_FAIL;
            }
            //  At this point, the current write will begin at an even block boundary.


            // At this point, there are three possibilities;
            //  (1) the was fully satisfied via the cache, in which case the remaining phases
            //      will not be performed,
            //  (2) the write was partially complete via the cache and there are more bytes to be
            //      written. In this case, the next write will continue at an even block boundary.
            //  (3) one of the writes (functions) in phase 1 failed and no bytes were written;
            //      (a) CacheRawBlocks() operation failed (which is essentially a read operation)
            //          indicating a catastrophic system failure.  Should this cache read operation
            //          fail, no bytes will be written in phase 1,
            //      (b) WriteCacheAndFlush() operation failed whereby phase 1 was not able to write
            //          to the device via the cache. Failure of a cache mediated write indicates
            //          no bytes were written to the device (in phase 1).


            // Phase 2 - if there are more bytes to write, the next write will check the [remaining] bytes to
            //   be written. This phase will handle a very large write where the number of bytes exceeds
            //   the cache size.  In this case, the remaining bytes (to write) will be larger than as single
            //   cache. For this, very large write, the maximum number of blocks are computed and that size is
            //   written directly from the buffer (provided).  The cache is not used here since writing directly
            //   from the buffer provided is more efficient.
            if (SUCCEEDED(ret) && (bytesRemaining > m_CacheSize))
            { // Write to the device directly from the input buffer, consume as many full blocks as possible.
                size_t bytesProcessed;
                ULONGLONG  writeSize = m_BlockSize * (bytesRemaining / m_BlockSize);

                if (writeSize > GetPartitionRemainingReadBytes())
                { // ensure a large write does not go beyond the partition boundary
                    writeSize = GetPartitionRemainingReadBytes();
                }

                if ( SUCCEEDED(ret = MoveToDeviceBlock(m_IOCurPos.QuadPart / m_BlockSize)) &&
                     SUCCEEDED(ret = WriteBlocksToDevice(pBuffer, size_t(writeSize), &bytesProcessed))
                   )
                {
                    bytesRemaining -= bytesProcessed;
                    pBuffer += bytesProcessed;
                    m_IOCurPos.QuadPart += bytesProcessed;
                    m_IOCurBlock.QuadPart += bytesProcessed /m_BlockSize;
                    *bytesWritten += bytesProcessed;
                    if (writeSize != bytesProcessed)
                    {
                        ret = E_FAIL;
                    }
                }

            }

            // At this point, we should be a an even block boundary because the writes in phase 1 or 2 leave
            // the I/O pointer at a block boundary.  Now, there is less than a cahceSize amount of data to write.

            // Phase 3 - if there are remaining bytes to be write, trigger a new cache read and copy whatever
            // remains into the cache.
            if (SUCCEEDED(ret) &&
                (bytesRemaining > 0) &&
                SUCCEEDED(ret = CacheRawBlocks())
                )
            {
                assert(bytesRemaining >= m_CacheSize);
                size_t writeSize = size_t(bytesRemaining);

                if (SUCCEEDED(ret = WriteCacheAndFlush(pBuffer, writeSize)) &&
                    SUCCEEDED(ret = MoveToDeviceBlock(m_CacheCurBlock.QuadPart + m_CacheBlockCount))
                    )
                {
                    *bytesWritten += writeSize;
                }

            }

            if (SUCCEEDED(ret) && (*bytesWritten != bufferSize))
            { // All the reads were successful but this resulted in a partial read
                m_LastError = IO_ERROR_WRITE_PARTIAL;
            }

        }

    }

    return ret;
}


/*************************************************************************************************
** HRESULT  WriteToFile(
**                       _In_reads_bytes_(bufferSize) PCHAR buffer,
**                       _In_ size_t bufferSize,
**                       _Out_opt_ size_t *bytesWritten )
**    This is a wrapper for the WriteFile() function.
**************************************************************************************************/
HRESULT
DEVICE_IO::WriteToFile ( _In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesWritten)
{
    HRESULT hr = E_FAIL;

    if (nullptr == bytesWritten)
    { // fail if missing this required parameter
        m_LastError = IO_ERROR_INVALID_PARAMETER;
    }
    else
    {
        *bytesWritten = 0; // Write() will always pass in a valid pointer for this value
        if (nullptr == buffer)
        { // WATCH - this buffer (pointer) comes from the caller to Write() - could be null
            m_LastError = IO_ERROR_NULL_POINTER;
        }
        else if (0 == bufferSize)
        { // WATCH - this value comes from the caller to Write() - could be zero
            m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
        }
        else
        {
            // Write buffer to device and check that some bytes were written.
            // SafeIO() performs a flush after write.
            if (FAILED(hr = SafeIO(m_Handle, buffer, bufferSize, 0, IO_TYPE_WRITE, bytesWritten)) )
            {
                m_LastError = IO_ERROR_WRITE_FILE;
                hr = HRESULT_FROM_WIN32(GetLastError());
            }
            else
            {
                m_IOCurPos.QuadPart += *bytesWritten;
                if (m_IOCurPos.QuadPart > m_IOSize.QuadPart)
                { // If this was an append, adjust the file size
                    m_IOSize.QuadPart = m_IOCurPos.QuadPart;
                }

                if (*bytesWritten != bufferSize)
                {
                    m_LastError = IO_ERROR_WRITE_PARTIAL;
                }
                else
                {
                    m_LastError = IO_OK;
                }

            }

        }

    }

    return hr;
}

/*************************************************************************************************
**  HRESULT Write(
**            _In_reads_bytes_(bufferSize) PCHAR buffer,
**            _In_ size_t bufferSize,
**            _Out_opt_ size_t *bytesWritten)
**  Public method for users to be able to write files or blocks based file systems. This provides
**  a standard (uniform) interface for writing data.  The offset into the file (m_IOCurPos)
**  allows for byte orinted I/O by SetPos(), which is used to set the position.
**************************************************************************************************/
HRESULT
DEVICE_IO::Write (_In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesWritten)
{
    HRESULT hr = E_FAIL;

    if (bytesWritten != nullptr)
    { // Always set this to zero, when provided
        *bytesWritten = 0;
    }

    if (nullptr == buffer)
    { // Make sure there is a buffer to receive the read data
        m_LastError = IO_ERROR_NULL_POINTER;
    }
    else if (0 == bufferSize)
    { // Make sure there is a buffer size
        m_LastError = IO_ERROR_INVALID_BUFFER_SIZE;
    }
    else if (IsIoReady ())
    {
        size_t bWritten = 0;

        m_LastError = IO_OK;
        switch (m_Type)
        {
            case RAW_DEVICE_TYPE:
            case REMOVABLE_MEDIA_DEVICE_TYPE:
                hr = WriteToBlockDevice(buffer, bufferSize, &bWritten);
                break;

            case PLAIN_FILE_DEVICE_TYPE:
                hr = WriteToFile(buffer, bufferSize, &bWritten);
                break;

            default:
                m_LastError = IO_ERROR_UNSUPPORTED_DEVICE_TYPE;
                break;
        }

        if (bytesWritten != nullptr)
        { // Return a value if a pointer was passed
            *bytesWritten = bWritten;
        }

    }

    return hr;
}

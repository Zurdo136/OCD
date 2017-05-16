/*++

    Copyright (C) Microsoft. All rights reserved.

Module Name:
   DEVICE_IO.h

Environment:
   User Mode

Author:
   José Pagán (jopagan)
--*/

// TODO: Bug: 10033908 - to address optimizations for these calculations, in future.
// TODO: bug: 10034859 - update read/write internal (helper) functions to _Out_ rather than _Out_opt_

#pragma once

#include "GUIDDefs.h"

using namespace std;

#define  GUID_STRING_TEMPLATE                   L"{00000000-0000-0000-0000-000000}"
#define  GUID_STRING_FORMAT                     L"{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"
#define  PHYSICAL_DEVICE_STRING                 L"\\\\.\\PhysicalDrive"
#define  MBR_PARTITION_NAME_STRING              L"MBRPartition"
#define  MAX_PARTITION_INDEX_STRING_SIZE        3           // use 2 digits plus null, since number of partitions should be less than 100
#define  INVALID_BLOCK                          (ULONGLONG)(-1)
#define  INVALID_INDEX                          (UINT)(-1)
#define  INVALID_DEVICE_ID                      (UINT)(-1)
#define  MAX_DEV_ID_STRING_SIZE                 4           // use 3 digit value since disk count should be less than 1000
#define  MAX_DEV_ID_VALUE                       1000        // number of disks should be less than 1000
#define  DEFAULT_BLOCK_SIZE                     0x1000      // Taking the current UFS block size as the default
#define  DEFAULT_CACHE_BLOCK_COUNT              0x2000      // Default number of blocks in the cache

class DEVICE_IO
{
    public:
        typedef enum _READ_EXACT_OPTIONS {
            READ_ANY = FALSE,
            READ_EXACT = TRUE,
        } READ_EXACT_OPTIONS;

        typedef enum _PARTITION_NAME {
            SVRAWDUMP = 0,
            CRASHDUMP,
            EFIESP,
            MAINOS,
            DATA,
            NONE
        } PARTITION_NAME;

        typedef enum _PARTITION_FIELD {
            PARTITION_FIELD_NAME = 0,
            PARTITION_FIELD_TYPE_GUID
        } PARTITION_FIELD;

        // Device types
        typedef enum _IO_DEVICE_TYPE {
            UNINITIALIZED_DEVICE_TYPE = -1,
            UNSUPPORTED_DEVICE_TYPE   = 0,
            RAW_DEVICE_TYPE,
            REMOVABLE_MEDIA_DEVICE_TYPE,
            PLAIN_FILE_DEVICE_TYPE
        } IO_DEVICE_TYPE;

        // Error codes to be returned by GetError()
        typedef enum _IO_ERROR {
            IO_OK = 0,
            IO_FAIL = 0xC0000001,
            IO_ERROR_PARTITION_NOT_FOUND,
            IO_ERROR_INVALID_DEVICE_NAME,
            IO_ERROR_INVALID_DEVICE_ID,
            IO_ERROR_INVALID_METHOD_USED,
            IO_ERROR_INVALID_HANDLE,
            IO_ERROR_INVALID_FILE_SIZE,
            IO_ERROR_INVALID_BLOCK,
            IO_ERROR_INVALID_POSITION,
            IO_ERROR_INVALID_PARAMETER,
            IO_ERROR_GET_POSITION_FAILED,
            IO_ERROR_SET_POSITION_FAILED,
            IO_ERROR_DEVICE_NO_BLOCKS,
            IO_ERROR_PARTITION_NO_BLOCKS,
            IO_ERROR_NO_MEMORY,
            IO_ERROR_NULL_POINTER,
            IO_ERROR_INVALID_BUFFER_SIZE,
            IO_ERROR_READ_FILE,
            IO_ERROR_READ_PARTIAL,
            IO_ERROR_READ_INVALID_BLOCK_RETURNED,
            IO_ERROR_READ_INVALID_BLOCK_POSITION,
            IO_ERROR_READ_COPY,
            IO_ERROR_WRITE_FILE,
            IO_ERROR_WRITE_PARTIAL,
            IO_ERROR_WRITE_INVALID_BLOCK_RETURNED,
            IO_ERROR_WRITE_INVALID_BLOCK_POSITION,
            IO_ERROR_WRITE_COPY,
            IO_ERROR_CACHE_NOT_ALLOCATED,
            IO_ERROR_CACHE_READ,
            IO_ERROR_CACHE_MODIFY,
            IO_ERROR_CACHE_WRITE,
            IO_ERROR_CACHE_WRITE_SIZE,
            IO_ERROR_CACHE_INVALID,
            IO_ERROR_EOF,
            IO_ERROR_GEOMETRY_FAILURE,
            IO_ERROR_INVALID_DRIVE_LAYOUT,
            IO_ERROR_CANNOT_READ_DRIVE_LAYOUT,
            IO_ERROR_UNSUPPORTED_DEVICE_TYPE,
            IO_ERROR_UNSUPPORTED_PARTITION_LAYOUT,
            IO_ERROR_UNSUPPORTED_PARTITION_MBR,
            IO_ERROR_UNSUPPORTED_PARTITION_RAW,
            IO_ERROR_UNSUPPORTED_PARTITION_TYPE,
            IO_ERROR_DISK_NO_PARTITIONS,
            IO_ERROR_PARTITION_NOT_SET,
            IO_ERROR_INVALID_PARTITION_TABLE,
            IO_ERROR_INVALID_PARTITION_INDEX,
            IO_ERROR_INVALID_PARTITION_TYPE_GUID,
            IO_ERROR_INVALID_PARTITION_ID_GUID,
            IO_ERROR_INVALID_PARTITION_NAME,
            IO_ERROR_MBR_PARTITION_UNUSED,
            IO_ERROR_SET_NAME_ON_OPENED_DEVICE,
            IO_ERROR_SET_ID_ON_OPENED_DEVICE,
            IO_ERROR_ALREADY_OPENED,
            IO_ERROR_MAX_ERROR_VALUE
        } IO_ERROR;

        // Constructors -  all use the private Init() to set up the device
        DEVICE_IO(_In_ std::wstring devName);
        DEVICE_IO(_In_ UINT devID);
        DEVICE_IO(void);
        // Destructor
        ~DEVICE_IO(void);

        // Methods
        IO_ERROR                        GetError(void) const { return m_LastError; };
        IO_DEVICE_TYPE                  GetDeviceType(void) const { return m_Type; };
        PARTITION_STYLE                 GetDevicePartitionType(void) const { return ((m_pDriveLayout==nullptr) ? PARTITION_STYLE_RAW : (PARTITION_STYLE)m_pDriveLayout->PartitionStyle); };
        PARTITION_STYLE                 GetCurrentPartitionType(void);

        wstring                         GetDeviceName(void) const { return m_Name; };
        ULONG                           GetBlockSize(void) const { return m_BlockSize; };
        UINT                            GetPartitionCount(void);
        wstring                         GetCurrentPartitionName(void) const;
        wstring                         GetCurrentPartitionGUID(void) const;
        UINT                            GetCurrentPartitionNumber(void) { return ((IsIoReady() && (m_pCurrentPartition != nullptr)) ? (UINT)(m_pCurrentPartition->PartitionNumber) : INVALID_INDEX); };
        ULONGLONG                       GetCurrentPartitionSize(void) { return ((IsIoReady() && (m_pCurrentPartition != nullptr)) ? (ULONGLONG)(m_pCurrentPartition->PartitionLength.QuadPart) : 0); };

        ULONGLONG                       GetCurrentFileSize(void) const { return m_IOSize.QuadPart; };
        HRESULT                         GetPos(_Inout_ ULONGLONG *curPos);
        HRESULT                         GetIoPos(_Inout_ ULONGLONG *curPos);

        HRESULT                         SetDeviceName(_In_ wstring strFileName);
        HRESULT                         SetDeviceID(_In_ UINT devID);

        HRESULT                         Open(void);
        HRESULT                         Open(_In_ wstring fName);
        HRESULT                         Open(_In_ UINT devID);
        HRESULT                         Close(void);

        HRESULT                         SetPartition(_In_ UINT ndx);
        HRESULT                         SetPartition(_In_ PARTITION_NAME nameEnum);
        HRESULT                         SetPartition(_In_z_ PCHAR name);
        HRESULT                         SetPartition(GUID guid = SVRAWDUMP_PARTITION_GUID);

        HRESULT                         SetPos(_In_opt_ LARGE_INTEGER fPos);
        HRESULT                         SetPos(_In_ ULONGLONG fPos);

        HRESULT                         Read(_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t* bytesRead);
        HRESULT                         ReadAtOffset(_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _In_ LARGE_INTEGER offset, _In_ READ_EXACT_OPTIONS readExact);
        HRESULT                         Write(_In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t* bytesWritten);

    private:
        // Object variables
        wstring                         m_Name;
        IO_ERROR                        m_LastError;
        IO_DEVICE_TYPE                  m_Type;
        UINT                            m_ID;
        UINT                            m_ndxCurrentPartition;
        ULONG                           m_BlockSize;

        PDRIVE_LAYOUT_INFORMATION_EX    m_pDriveLayout;
        PPARTITION_INFORMATION_EX       m_pCurrentPartition;
        ULARGE_INTEGER                  m_CurrentPartitionBlockCount;

        HANDLE                          m_Handle;
        ULARGE_INTEGER                  m_IOCurPos;
        ULARGE_INTEGER                  m_IOCurBlock;
        ULARGE_INTEGER                  m_IOBlockCount;
        ULARGE_INTEGER                  m_IOSize;

        ULARGE_INTEGER                  m_CacheCurBlock;
        ULONGLONG                       m_CacheCurOffset;
        ULONG                           m_CacheBlockCount;
        ULONG                           m_CacheSize;
        PCHAR                           m_pCache;

        // Copy Constructor -  making this private makes it a compile time error to pass by value
        DEVICE_IO(_In_ const DEVICE_IO &obj);

        // Helpers
        BOOL                            AllocateCache(_In_ UINT blockSizeMult);
        HRESULT                         UpdateCache(_In_reads_bytes_(bufferSize) PCHAR pBuffer, _In_ size_t bufferSize);
        BOOL                            IsCacheValid(void) const;
        BOOL                            IsPositionValid (_In_ ULONGLONG newPos);
        ULONGLONG                       GetCacheRemaining() const { return (m_CacheSize > 0) ? (m_CacheSize - m_CacheCurOffset) : 0; }
        inline ULONGLONG                GetCacheOffset() { return (ULONGLONG)(m_IOCurPos.QuadPart - (m_CacheCurBlock.QuadPart * m_BlockSize)); }
        VOID                            FreeCache(void);

        HRESULT                         CacheRawBlocks(void);
        HRESULT                         MoveToDeviceBlock (_In_ ULONGLONG newPartitionBlock);
        HRESULT                         ReadBlocksFromDevice(_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesRead);
        HRESULT                         ReadFromBlockDevice(_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesRead);
        HRESULT                         ReadFromFile(_Out_writes_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesRead);

        HRESULT                         WriteCacheAndFlush(_In_reads_bytes_(bufferSize) PCHAR pBuffer, _In_ size_t bufferSize);
        HRESULT                         WriteBlocksToDevice(_In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesWritten);
        HRESULT                         WriteToBlockDevice(_In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesWritten);
        HRESULT                         WriteToFile(_In_reads_bytes_(bufferSize) PCHAR buffer, _In_ size_t bufferSize, _Out_opt_ size_t *bytesWritten);

        HRESULT                         OpenPhysicalDisk(void);
        HRESULT                         ReadDiskGeometry(void);
        HRESULT                         ReadDiskLayout(void);
        VOID                            Init(_In_ wstring devName, _In_ UINT devID);
        VOID                            SetIOBlockCount(void){ m_IOBlockCount.QuadPart = (m_IOSize.QuadPart / m_BlockSize) + ((m_IOSize.QuadPart % m_BlockSize) ? 1 : 0); }
        VOID                            SetIOGeometry(DISK_GEOMETRY diskGeometry);
        VOID                            SetPartitionGeometry(void);
        BOOL                            IsDeviceReady(void);
        BOOL                            IsIoReady(void);
        ULONGLONG                       GetPartitionRemainingReadBytes() { return (IsIoReady() && (0 != GetCurrentPartitionSize())) ?  (GetCurrentPartitionSize() - m_IOCurPos.QuadPart) : 0; }
        HRESULT                         SetPartitionEntry(_In_z_ PWCHAR pName, _In_ GUID guid, _In_ PARTITION_FIELD flield);
        PPARTITION_INFORMATION_EX       GetPartitionIndex(_In_ UINT n);
        HRESULT                         SetIoPosition(_In_opt_ ULONGLONG fPos);
        HRESULT                         SetFileOffset(_In_ ULONGLONG newPos);
};

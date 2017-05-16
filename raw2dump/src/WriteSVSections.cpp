/*++

Copyright (c) Microsoft Corporation, All Rights Reserved

Module Name:
    WriteSVSections.cpp

Environment:
    User Mode

--*/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <initguid.h>
#include "dumputil.h"


//
// SV Specific name to GUID mapping.
//
SV_SPECIFIC_NAME_TO_GUID GUIDToName[] = {
    {0xAB3A051F, 0xEF0B, 0x4A5F, {0xA7, 0x9A, 0x80, 0xC2, 0x43, 0xBA, 0x08, 0x48}, "AP_REG"},
    {0xD0A267A1, 0x9CA5, 0x471D, {0x8E, 0x9C, 0x79, 0xC9, 0x86, 0xBE, 0x77, 0x77}, "OCIMEM.BIN"},
    {0x100B990B, 0x0F9B, 0x40B3, {0x82, 0xEF, 0x06, 0x61, 0x4F, 0x53, 0x05, 0xFE}, "CODERAM.BIN"},
    {0x82233308, 0xCE47, 0x4D52, {0x92, 0x11, 0xF4, 0x2E, 0x89, 0x61, 0x8A, 0xF4}, "DATARAM.BIN"},
    {0x91A8C35C, 0xA340, 0x4F2E, {0xB7, 0x27, 0x65, 0x39, 0x47, 0xDB, 0x9C, 0x76}, "MSGRAM.BIN"}, 
    {0x877F61E0, 0xA870, 0x4635, {0x9F, 0x41, 0x33, 0x00, 0x53, 0x20, 0x26, 0x05}, "LPM.BIN"},
    {0x10D25EDD, 0x1558, 0x4B88, {0xAB, 0x5C, 0xE8, 0x1E, 0x7F, 0x47, 0xDA, 0xD9}, "PMIC_PON.BIN"}, 
    {0xD0352E48, 0xE359, 0x459E, {0x9B, 0xBF, 0x2E, 0x16, 0xE6, 0x28, 0xAC, 0xFB}, "RST_STAT.BIN"}, 
    {0x066A56C8, 0xCE2A, 0x4686, {0xB6, 0x10, 0x5B, 0xFC, 0x22, 0xD0, 0xC7, 0xAB}, "load.cmm"},
    {0x0df632e9, 0x5c48, 0x43aa, {0xb8, 0xbd, 0x5f, 0xf6, 0x18, 0x05, 0x02, 0x5f}, "rawdump.bin"},
    {0x62fb2678, 0x933f, 0x4177, {0x86, 0x29, 0xff, 0x3f, 0x70, 0x55, 0x02, 0xe3}, "DDR_DATA.BIN"},
    {0x6901D825, 0x0E25, 0x4D6C, {0x8C, 0x11, 0xE0, 0xAB, 0x2E, 0x98, 0xCA, 0xEF}, "UNKNOWN"}}; //Must be last element.



NTSTATUS
WriteFileAtOffset (
    __in HANDLE hFile,
    __in ULONG Size,
    __in PLARGE_INTEGER ByteOffset,
    __in_bcount(Size) PVOID Buffer,
    __out_opt PULONG BytesWritten
    )
{
    IO_STATUS_BLOCK statusBlock;
    NTSTATUS status = NtWriteFile(
                          hFile,
                          nullptr,
                          nullptr,
                          nullptr,
                          &statusBlock,
                          Buffer,
                          Size,
                          ByteOffset,
                          nullptr
                          );
    if (FAILED(status)) {
        TraceNTSTATUS("WriteFileAtOffset failed", status);
    } else {
        NtFlushBuffersFile(hFile, &statusBlock);
        if (BytesWritten) {
            *BytesWritten = Size;
        }
        ByteOffset->QuadPart += Size;
    }

    return status;
}

NTSTATUS
WriteBlobHeader (
    _In_ HANDLE FileId,
    _In_ GUID Guid,
    _In_ UINT64 DumpBlobSize,
    _Inout_ PLARGE_INTEGER FileOffset
    )

/*++

Routine Description:

    This function populates DUMP_BLOB_HEADER and writes it to a crash dump
    file.

Arguments:

    FileId - Provides id of a file that DUMP_BLOB_HEADER needs to be written
        to.

    Guid - GUID of a memory descriptor.

    DumpBlobSize - Size of a memory region used by the subsystem.

    FileOffset - Points to a location in a file where DUMP_BLOB_HEADER needs to
        be written to. Upon successful write, the FileOffset is advanced by the
        number of bytes written.

Return Value:

    NT status code.

--*/

{
    NTSTATUS Status;
    DUMP_BLOB_HEADER DumpBlobHeader;
    ULONG BytesWritten;

    //
    // DumpBlobSize is 0 when the blob header to marks the of tagged
    // secondary data
    //
    DumpBlobHeader.HeaderSize = sizeof(DUMP_BLOB_HEADER);

    RtlCopyMemory(&DumpBlobHeader.Tag,
                  &Guid,
                  sizeof(GUID));

    //
    // Asserting if downcasting DumpBlobSize would result in data loss
    //
    ASSERT(DumpBlobSize <= ULONG_MAX);

    DumpBlobHeader.DataSize = (ULONG)DumpBlobSize;
    DumpBlobHeader.PrePad = 0;
    DumpBlobHeader.PostPad = 0;

    Status = WriteFileAtOffset(
                 FileId,
                 sizeof(DUMP_BLOB_HEADER),
                 FileOffset,
                 &DumpBlobHeader,
                 &BytesWritten);
    if (FAILED(Status)) {
        TraceNTSTATUS("WriteFileAtOffset failed", Status);
        goto Exit;
    }

Exit:

    return Status;
}


NTSTATUS
WpDmppReadFromRawDump(
    _In_ PDMP_CONTEXT Context,
    _In_ ULONGLONG Offset,
    _In_ UINT32 BytesToRead,
    _Out_ PVOID Buffer
    )
    /*++

    Routine Description:

        This function handles the reading of data from raw dump
        This routine will return failure if it was unable to read in all the bytes requested.

    Arguments:

        Context - DMP_CONTEXT

        Offset - Offset from device or file. Used only for raw dump.

        BytesToRead -Number of bytes to read.

        Buffer - Buffer to hold the read bytes.

    Return Value:
    
        NT status code.
    
    --*/
{
    NTSTATUS    status = STATUS_SUCCESS;
    size_t      bytesProcessed = 0;

    if ( FAILED(status = Context->hRawFile.SetPos(Offset))
         || FAILED(status = Context->hRawFile.Read((PCHAR)Buffer, BytesToRead, &bytesProcessed))
         || (0 == bytesProcessed)
       )
    {
        TraceNTSTATUS("ReadDisk failed", status);
        goto Exit;
    }

Exit:

    return status;
}


NTSTATUS
WpDmppCopyDDRFromRawDumpToDumpFileByOffset(
    _Inout_ PDMP_CONTEXT Context,
    _In_ UINT64 RawDumpOffset,
    _In_ LARGE_INTEGER DumpFileOffset,
    _In_ UINT32 BytesToCopy,
    _Out_opt_ PUINT32 BytesCopied
    )
/*++

Routine Description:

    This function reads the data from the raw dump to the end of the dump file.

Arguments:

    Context - Pointer to the global context structure.

    RawDumpOffset - Byte offset into the raw dump.

    DumpFileOffset - Byte offset into the Windows crash dump file.

    BytesToCopy - Specifies the number of bytes to copy from raw dump to the 
                          Windows crash dump.

    BytesCopied - Returns the amount of bytes copied. Most likely to be used to 
                         update offsets.

Return Value:

    NT status code.

--*/
{
    ULONG       bytesToCopy = 0;
    ULONG       bytesWritten = 0;
    ULONG       bytesRemain = 0;
    LARGE_INTEGER   dumpFileOffset;
    PVOID       ioBuffer = nullptr;
    UINT32      iterationsRequired = 0;
    UINT32      iteration = 0;
    ULONGLONG   rawDumpOffset = 0;
    ULONG       totalBytesCopied = 0;
    NTSTATUS    status = STATUS_SUCCESS;

    iterationsRequired = (UINT32)(BytesToCopy / IO_BUFFER_SIZE);

    ioBuffer = Context->IoBuffer;

    if ((BytesToCopy % IO_BUFFER_SIZE) != 0) {
        iterationsRequired += 1;
    }

    TraceInfo2("Number of iterations required to copy", "Iterations", iterationsRequired, "Bytes", BytesToCopy);

    bytesRemain = BytesToCopy;
    rawDumpOffset = RawDumpOffset;
    dumpFileOffset = DumpFileOffset;

    for (iteration = 0; iteration < iterationsRequired; iteration++) {
        bytesToCopy = (bytesRemain < IO_BUFFER_SIZE) ? bytesRemain : IO_BUFFER_SIZE;

        //
        // Read into the buffer.
        //
        status = WpDmppReadFromRawDump(
                     Context,
                     rawDumpOffset,
                     bytesToCopy,
                     ioBuffer
                     );
        if (FAILED(status)) {
            TraceNTSTATUS("Failed to read from raw dump", status);
            goto Exit;
        }

        rawDumpOffset += bytesToCopy;

        //
        // Write to dump file.
        //
        status = WriteFileAtOffset(
                     Context->WindowsDumpHandle,
                     bytesToCopy,
                     &dumpFileOffset,
                     ioBuffer,
                     &bytesWritten
                     );
        if (FAILED(status)) {
            TraceNTSTATUS("Failed to write to dump file", status);
            goto Exit;
        }

        if (bytesWritten != bytesToCopy) {
            TraceInfo2("Partial write to dump file", "Bytes Expected", bytesToCopy, "Actual Bytes", bytesWritten);
            status = STATUS_UNSUCCESSFUL;
            goto Exit;
        }

        bytesRemain -= bytesWritten;
        totalBytesCopied += bytesWritten;
    }

    if (bytesRemain != 0) {
        status = STATUS_UNSUCCESSFUL;
        TraceNTSTATUS("Sanity check failed: bytesRemain is not zero!", status);
        goto Exit;
    }

Exit:

    if (BytesCopied != nullptr) {
        *BytesCopied = totalBytesCopied;
    }

    return status;

}


NTSTATUS
WpDmppWriteNonOSDDR(
    _Inout_ PDMP_CONTEXT Context
    )
/*++

Routine Description:

    This function writes the memory map and the nonOS DDR regions to 
    secondary dump data.

Arguments:

    Context - Pointer to the global context structure.

Return Value:

    NT status code.

--*/
{
    UINT32      bytesCopied = 0;
    ULONG       bytesWritten = 0;
    ULONG       bytesToWrite = 0;
    UINT32      index = 0;
    NTSTATUS    status = STATUS_SUCCESS;
    UINT64      totalBytesCopied = 0;

    TraceInfo("Writing complete memory map to dump file.\n");

    //
    // Write the memory map to its own area.
    // Write the blob header first.
    //
    bytesToWrite = Context->CompleteMemoryMapCount * sizeof(DDR_MEMORY_MAP);

    status = WriteBlobHeader(
                 Context->WindowsDumpHandle,
                 MEMORY_MAP_GUID,
                 bytesToWrite,
                 &Context->WindowsDumpFileOffset
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to write the blob header", status);
        goto Exit;
    }

    status = WriteFileAtOffset(
                 Context->WindowsDumpHandle,
                 (UINT32)bytesToWrite,
                 &Context->WindowsDumpFileOffset,
                 Context->CompleteMemoryMap,
                 &bytesWritten
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to write complete memory map to dump", status);
        goto Exit;
    }

    if (bytesWritten != bytesToWrite) {
        TraceNTSTATUS("Failed to write all the contents of memory map to dump", status);
        goto Exit;
    }

    TraceInfo("Writing NonOS memory to dump.\n");

    //
    // Now write the NonOS regions. 
    // Write blob header for NonOS DDR.
    //
    status = WriteBlobHeader(
                 Context->WindowsDumpHandle,
                 NON_OS_DDR_GUID,
                 Context->TotalNonOSDDRSizeInBytes,
                 &Context->WindowsDumpFileOffset
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to write the blob header", status);
        goto Exit;
    }

    if (Context->CompleteMemoryMapCount == 0) {
        TraceInfo("Memory map count is zero. This is unexpected.\n");
    }

    for (index = 0 ; index < Context->CompleteMemoryMapCount; index++) {
        if (Context->CompleteMemoryMap[index].Type == MEMORY_NONOS) {
            TraceInfo("Copying nonOS memory");
            TraceInfo1("  ", "Map Index", index);
            TraceInfo1("  ", "RawDumpOffset",  Context->CompleteMemoryMap[index].Offset);
            TraceInfo1("  ", "DumpFileOffset", Context->WindowsDumpFileOffset.QuadPart);
            TraceInfo1("  ", "Size",           Context->CompleteMemoryMap[index].Size);

            status = WpDmppCopyDDRFromRawDumpToDumpFileByOffset(
                         Context,
                         Context->CompleteMemoryMap[index].Offset,
                         Context->WindowsDumpFileOffset,
                         (UINT32)Context->CompleteMemoryMap[index].Size,
                         &bytesCopied
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to copy section from raw dump to dump file", status);
                goto Exit;
            }

            Context->WindowsDumpFileOffset.QuadPart += bytesCopied;
            totalBytesCopied += bytesCopied;
        }
    }

    TraceInfo1("Total nonOS bytes copied", "Bytes copied", totalBytesCopied);

Exit: 

    return status;
}


NTSTATUS
WpDmppMemWriteDumpBlobFileHeader (
    _In_ HANDLE FileId,
    _Inout_ PLARGE_INTEGER FileOffset
    )

/*++

Routine Description:

    This function populates DUMP_BLOB_FILE_HEADER and writes it to a crash dump
    file.

Arguments:

    FileId - Provides id of a file that DUMP_BLOB_FILE_HEADER needs to be
        written to.

    FileOffset - Points to a location in a file where DUMP_BLOB_FILE_HEADER
        needs to be written to. Upon successful write, the FileOffset is
        advanced by the number of bytes written.

Return Value:

    NT status code.

--*/

{
    NTSTATUS Status;
    DUMP_BLOB_FILE_HEADER DumpBlobFileHeader;
    ULONG BytesWritten;

    DumpBlobFileHeader.Signature1 = DUMP_BLOB_SIGNATURE1;
    DumpBlobFileHeader.Signature2 = DUMP_BLOB_SIGNATURE2;
    DumpBlobFileHeader.HeaderSize = sizeof(DUMP_BLOB_FILE_HEADER);
    DumpBlobFileHeader.BuildNumber = 1205;

    Status = WriteFileAtOffset(FileId,
                                 sizeof(DUMP_BLOB_FILE_HEADER),
                                 FileOffset,
                                 &DumpBlobFileHeader,
                                 &BytesWritten);
    if (FAILED(Status)) {
        TraceNTSTATUS("WriteFileAtOffset failed", Status);
        goto Exit;
    }

Exit:

    return Status;
}


HRESULT WriteSVSpecific(_Inout_ PDMP_CONTEXT Context)
/*++

Routine Description:

    This function writes AP_REG and SV Specific sections based on the 
    DUMP_HEADER's physical memory descriptor to the DedicatedDumpFile.

Arguments:

    Context - Pointer to the global context structure.

Return Value:

    NT status code.

--*/
{
    UINT32                   blobSize = 0;
    ULONG                    bytesWritten = 0;
    BOOLEAN                  foundName = FALSE;
    UINT32                   guidToNameIndex = 0;
    UINT32                   guidToNameTableSize = 0;
    UINT32                   sectionsCount = Context->RawDumpHeader.SectionsCount;
    UINT32                   sectionIndex = 0;
    NTSTATUS                 status = STATUS_SUCCESS;
    PVOID                    tempBuffer = nullptr;

    Context->SecondaryDataOffset.QuadPart = 0;

    if (Context->SecondaryDataBlobCount ==  0) {
        TraceInfo("No need for secondary data. Leaving...\n");
        goto Exit;
    }

    Context->SecondaryDataOffset = Context->WindowsDumpFileOffset;

    status = WpDmppMemWriteDumpBlobFileHeader(
                 Context->WindowsDumpHandle, 
                 &Context->WindowsDumpFileOffset
                 );
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to write the blob file header to the DedicateDumpFile", status);
        goto Exit;
    }

    // Need to write raw dump table (Header and SectionsTable) - there will always be one if we reach this point.
    if (FAILED(status = WriteBlobHeader(Context->WindowsDumpHandle, RAW_DUMP_TABLE_GUID, (sizeof RAW_DUMP_HEADER + RawDumpTableSize(sectionsCount)), &Context->WindowsDumpFileOffset)))
    {
        TraceNTSTATUS("Failed to write the blob header for raw dump table", status);
        goto Exit;
    }
    else if (FAILED(status = WriteFileAtOffset(Context->WindowsDumpHandle, sizeof RAW_DUMP_HEADER, &Context->WindowsDumpFileOffset, &Context->RawDumpHeader, &bytesWritten)))
    {
        TraceNTSTATUS("Failed to write raw dump header to DedicatedDumpFile", status);
        goto Exit;
    }
    else if (FAILED(status = WriteFileAtOffset(Context->WindowsDumpHandle, RawDumpTableSize(sectionsCount), &Context->WindowsDumpFileOffset, Context->RawDumpSectionTable, &bytesWritten)))
    {
        TraceNTSTATUS("Failed to write raw dump sections table to DedicatedDumpFile", status);
        goto Exit;
    }
    else
    {
        TraceInfo("Wrote raw dump table to secondary data.\n");
    }

    //
    // Second write AP Reg if any...
    //
    if (Context->ApReg != nullptr) {
        //
        // Write Blob header.
        //
        status = WriteBlobHeader(
                     Context->WindowsDumpHandle,
                     CPU_CONTEXT_GUID,
                     Context->TotalCpuContextSizeInBytes,
                     &Context->WindowsDumpFileOffset
                     );
        if (FAILED(status)) {
            TraceNTSTATUS("Failed to write the blob header for AP_REG", status);
            goto Exit;
        }

        status = WriteFileAtOffset(
                     Context->WindowsDumpHandle,
                     (UINT32)Context->TotalCpuContextSizeInBytes,
                     &Context->WindowsDumpFileOffset,
                     Context->ApReg,
                     &bytesWritten
                     );
        if (FAILED(status)) {
            TraceNTSTATUS("Failed to write AP_REG to DedicatedDumpFile", status);
            goto Exit;
        }

        TraceInfo("Wrote AP_REG to secondary data.\n");
    }

    guidToNameTableSize = sizeof(GUIDToName) / sizeof(GUIDToName[0]);

    tempBuffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (UINT32)Context->LargestSVSpecificSectionSize);

    if (tempBuffer == nullptr) {
        status = STATUS_NO_MEMORY;
        TraceNTSTATUS("Failed to allocate intermediate buffer for writing SV specific sections", status);
        goto Exit;
    }

    //
    // Now go through the section table and copy each section to a secondary blob.
    //
    for (sectionIndex = 0; sectionIndex < sectionsCount; sectionIndex++) {
        PRAW_DUMP_SECTION_HEADER section = &(Context->RawDumpSectionTable[sectionIndex]);

        if (section->Type == RAW_DUMP_SECTION_TYPE_SV_SPECIFIC) {
            foundName = FALSE;

            //
            // Find the matching GUID for the section name.
            //
            for (guidToNameIndex = 0; guidToNameIndex <  guidToNameTableSize; guidToNameIndex++) {
                if (RtlEqualMemory(section->Name, 
                                   GUIDToName[guidToNameIndex].Name,
                                   RAW_DUMP_SECTION_HEADER_NAME_LENGTH))
                {
                    foundName = TRUE;
                    break;
                }
            }//for guidToNameIndex

            if (!foundName) {
                //use UNKNOWN_GUID
                guidToNameIndex = guidToNameTableSize - 1;
            }

            RtlZeroMemory(tempBuffer, (UINT32)Context->LargestSVSpecificSectionSize);

            //
            // Read from raw dump.
            //
            status = WpDmppReadFromRawDump(
                         Context,
                         section->Offset,
                         (UINT32)section->Size,
                         tempBuffer
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to read SV section from raw dump", status);
                goto Exit;
            }

            //
            // Write BLOB header.
            // 
            blobSize = (UINT32)section->Size + RAW_DUMP_SECTION_HEADER_NAME_LENGTH;

            status = WriteBlobHeader(
                         Context->WindowsDumpHandle,
                         GUIDToName[guidToNameIndex].Guid,
                         blobSize,
                         &Context->WindowsDumpFileOffset
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to write blob header to DedicateDumpFile", status);
                goto Exit;
            }

            //
            // Write the name.
            // 
            status = WriteFileAtOffset(
                         Context->WindowsDumpHandle,
                         RAW_DUMP_SECTION_HEADER_NAME_LENGTH,
                         &Context->WindowsDumpFileOffset,
                         section->Name,
                         &bytesWritten
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to write SV name to DedicatedDumpFile", status);
                goto Exit;
            }

            if (bytesWritten != RAW_DUMP_SECTION_HEADER_NAME_LENGTH) {
                TraceInfo2("Partial write of section name", "Bytes Expected", RAW_DUMP_SECTION_HEADER_NAME_LENGTH,
                           "Actual Bytes", bytesWritten);
                status = STATUS_UNSUCCESSFUL;
                goto Exit;
            }

            //
            // Write the data.
            //
            status = WriteFileAtOffset(
                         Context->WindowsDumpHandle,
                         (UINT32)section->Size,
                         &Context->WindowsDumpFileOffset,
                         tempBuffer,
                         &bytesWritten
                         );
            if (FAILED(status)) {
                TraceNTSTATUS("Failed to write SV section to DedicatedDumpFile", status);
                goto Exit;
            }

            if (bytesWritten != (UINT32)section->Size) {
                status = STATUS_UNSUCCESSFUL;
                TraceInfo2("Partial write of section name", "Bytes Expected", (UINT32)section->Size,
                           "Actual Bytes", bytesWritten);
                goto Exit;
            }

            TraceInfo1("Wrote SV section to secondary data", "Section Index", sectionIndex);
        } // RAW_DUMP_SECTION_TYPE_SV_SPECIFIC
    }//for sectionindex

    //
    // Finally, write the nonOS DDR data.
    //
    status = WpDmppWriteNonOSDDR(Context);
    if (FAILED(status)) {
        TraceNTSTATUS("Failed to write non OS DDR sections", status);
    }

Exit:

    if (tempBuffer != nullptr) {
        HeapFree(GetProcessHeap(), NULL, tempBuffer);
        tempBuffer = nullptr;
    }

    return HRESULT_FROM_NT(status);
}



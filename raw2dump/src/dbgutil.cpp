/*++

Copyright (c) April 2013 Microsoft Corporation, All Rights Reserved

Module Name:
    dbgutil.cpp

Environment:
    User Mode

--*/
#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <ntdddisk.h>
#include <windows.h>
#include <winioctl.h>

#include <windef.h>
#include "dumputil.h"



VOID
DumpRawDumpHeader(
    _In_ PRAW_DUMP_HEADER            Header,
    _In_ PRAW_DUMP_SECTION_HEADER    SectionTable
)
{
    ULONG index;

    TraceInfo1("RAW_DUMP_HEADER", "Address", (PVOID)Header);
    TraceInfo1("DumpHeader", "Signature", (PSTR)Header->Signature);

    TraceInfo1("DumpHeader", "Version", Header->Version);
    TraceInfo1("DumpHeader", "Flags", Header->Flags);
    TraceInfo1("DumpHeader", "OsData", Header->OsData);
    TraceInfo1("DumpHeader", "CpuContext", Header->CpuContext);
    TraceInfo1("DumpHeader", "ResetTrigger", Header->ResetTrigger);
    TraceInfo1("DumpHeader", "DumpSize", Header->DumpSize);
    TraceInfo1("DumpHeader", "TotalDumpSizeRequire", Header->TotalDumpSizeRequired);
    TraceInfo1("DumpHeader", "SectionsCount", Header->SectionsCount);

    for (index = 0; index < Header->SectionsCount; index++) {
        TraceInfo2("Section", "Index", index, "Address", (PVOID)&SectionTable[index]);
        TraceInfo1("Section", "Flags", SectionTable[index].Flags);
        TraceInfo1("Section", "Version", SectionTable[index].Version);

        if (SectionTable[index].Type <  RAW_DUMP_SECTION_TYPE_Maximum) {
            TraceInfo2("Section", "Type", SectionTable[index].Type,
                       "Type Name", RawDumpSectionTypeStringTable[SectionTable[index].Type]);
        }

        TraceInfo1("Section", "Offset", SectionTable[index].Offset);
        TraceInfo1("Section", "Size", SectionTable[index].Size);

        switch (SectionTable[index].Type) {
        case RAW_DUMP_SECTION_TYPE_Reserved:
            TraceInfo("Type is reserved!!");
            break;

        case RAW_DUMP_SECTION_TYPE_DDR_RANGE:
            TraceInfo("Dumping DDR_INFORMATION");
            TraceInfo1("Section", "Base", SectionTable[index].u.DDRInformation.Base);
            break;

        case RAW_DUMP_SECTION_TYPE_CPU_CONTEXT:
            TraceInfo("Dumping CPU_CONTEXT_INFORMATION");
            break;

        case RAW_DUMP_SECTION_TYPE_SV_SPECIFIC:
            TraceInfo("Dumping SV_SPECIFIC_INFORMATION");
            DumpGUID((GUID*)SectionTable[index].u.SVSpecificInformation.SVSpecificData);
            break;

        default:
            TraceInfo1("Section", "UNKNOWN type", SectionTable[index].Type);
            break;
        }

        TraceInfo1("Section", "Name", (PSTR)SectionTable[index].Name);
    }
}


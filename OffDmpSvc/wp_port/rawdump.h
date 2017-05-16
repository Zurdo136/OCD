//
// Copyright (C) Microsoft. All rights reserved
//

//
// Based on http://windowsblue/docs/home/Windows%20Blue%20Feature%20Docs/Core%20(COR)/Kernel%20Platform%20Group%20(KPG)/Energy%20Efficiency/Offline%20Crash%20Dump%20Support/Windows%20Offline%20Crash%20Dump%20Specification_V3.0.docx
//


//
// Offline Crash Dump Configuration Table
//
// Defined in efiapi.h as EFI_OFFLINE_CRASHDUMP_TABLE_GUID
//{3804CF02-8538-11E2-8847-8DF16088709B}
//#define EFI_OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE_GUID \
//    {0x3804CF02, 0x8538, 0x11E2, 0x88, 0x47, 0x8D, 0xF1, 0x60, 0x88, 0x70, 0x9B}


//not defined
#define EFI_OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE_VERSION      0x2

//not defined
#define RESET_NORMAL                      0x0
#define RESET_ABNORMAL_REASON_UNKNOWN     0x1

//RAW_DUMP_SUPPORTED_VIA_DEDICATED_PARTITION defined as OFFLINE_DUMP_PARTITION in dumpctl.c
#define RAW_DUMP_NOT_SUPPORTED                          0x0
#define RAW_DUMP_SUPPORTED_VIA_DEDICATED_PARTITION      0x1
#define RAW_DUMP_SUPPORTED_VIA_DISK_MAP                 0x2

//defined as OFFLINE_CRASHDUMP_CONFIGURATION_TABLE in arc.h
typedef struct 
{
    UINT32      Version;
    UINT32      AbnormalResetOccurred;
    UINT32      OfflineMemoryDumpCapable;
} EFI_OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE, *PEFI_OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE;


//
// GUID for Offline Crash Dump variables
//

//defined as EFI_OFFLINE_CRASHDUMP_TABLE_GUID in efiapi.h.
//{77FA9ABD-0359-4d32-BD60-28F4E78F784B}
DEFINE_GUID(EFI_OFFLINE_CRASHDUMP_VENDOR_GUID,0x77FA9ABD, 
            0x0359, 0x4D32, 0xBD, 0x60, 0x28, 0xF4, 0xE7, 0x8F, 0x78, 0x4B);


//
// There are no defines in kernel side for these.
//
#define OFFLINE_MEMORY_DUMP_USE_CAPABILITY_VAR_NAME  L"OfflineMemoryDumpUseCapability"

#define OFFLINE_MEMORY_DUMP_OS_DATA_VAR_NAME L"OfflineMemoryDumpOsData"

#define OFFLINE_MEMORY_DUMP_AP_REG_ADDRESS_VAR_NAME L"APRegAddress"

//
// Defines for using dedicated partition as a dump location.
//

//{66C9B323-F7FC-48B6-BF96-6F32E335A428}
DEFINE_GUID(EFI_OFFLINE_CRASH_DUMP_DEDICATED_PARTITION_GUID,
            0x66C9B323, 0xF7FC, 0x48B6, 0xBF, 0x96, 0x6F, 0x32, 0xE3, 0x35, 0xA4, 0x28);

#define OFFLINE_MEMORY_DUMP_DEDICATED_PARTITION_NAME L"SVRawDump"

#define OFFLINE_MEMORY_DUMP_DEDICATED_PARTITION_ATTRIBUTES 0x0

//{6901D825-0E25-4D6C-8C11-E0AB2E98CAEF}
DEFINE_GUID(UNKNOWN_GUID,
            0x6901D825, 0x0E25, 0x4D6C, 0x8C, 0x11, 0xE0, 0xAB, 0x2E, 0x98, 0xCA, 0xEF);

//{D0A267A1-9CA5-471D-8E9C-79C986BE7777}
DEFINE_GUID(OCIMEM_GUID,
            0xD0A267A1, 0x9CA5, 0x471D, 0x8E, 0x9C, 0x79, 0xC9, 0x86, 0xBE, 0x77, 0x77);

//{100B990B-0F9B-40B3-82EF-06614F5305FE}
DEFINE_GUID(CODERAM_GUID,
            0x100B990B, 0x0F9B, 0x40B3, 0x82, 0xEF, 0x06, 0x61, 0x4F, 0x53, 0x05, 0xFE);

//{82233308-CE47-4D52-9211-F42E89618AF4}
DEFINE_GUID(DATARAM_GUID,
            0x82233308, 0xCE47, 0x4D52, 0x92, 0x11, 0xF4, 0x2E, 0x89, 0x61, 0x8A, 0xF4);

//{91A8C35C-A340-4F2E-B727-653947DB9C76}
DEFINE_GUID(MSGRAM_GUID,
            0x91A8C35C, 0xA340, 0x4F2E, 0xB7, 0x27, 0x65, 0x39, 0x47, 0xDB, 0x9C, 0x76);

//{877F61E0-A870-4635-9F41-330053202605}
DEFINE_GUID(LPM_GUID,
            0x877F61E0, 0xA870, 0x4635, 0x9F, 0x41, 0x33, 0x00, 0x53, 0x20, 0x26, 0x05);

//{10D25EDD-1558-4B88-AB5C-E81E7F47DAD9}
DEFINE_GUID(PMIC_PON_GUID,
            0x10D25EDD, 0x1558, 0x4B88, 0xAB, 0x5C, 0xE8, 0x1E, 0x7F, 0x47, 0xDA, 0xD9);

//{D0352E48-E359-459E-9BBF-2E16E628ACFB}
DEFINE_GUID(RST_STAT_GUID,
            0xD0352E48, 0xE359, 0x459E, 0x9B, 0xBF, 0x2E, 0x16, 0xE6, 0x28, 0xAC, 0xFB);

//{066A56C8-CE2A-4686-B610-5BFC22D0C7AB}
DEFINE_GUID(LOAD_CMM_GUID,
            0x066A56C8, 0xCE2A, 0x4686, 0xB6, 0x10, 0x5B, 0xFC, 0x22, 0xD0, 0xC7, 0xAB);

//{AB3A051F-EF0B-4A5F-A79A-80C243BA0848}
DEFINE_GUID(CPU_CONTEXT_GUID,
            0xAB3A051F, 0xEF0B, 0x4A5F, 0xA7, 0x9A, 0x80, 0xC2, 0x43, 0xBA, 0x08, 0x48);

//{3B9DEA8E-75BE-478F-9FB4-7D2C5F9DDE31}
DEFINE_GUID(NON_OS_DDR_GUID,
            0x3B9DEA8E, 0x75BE, 0x478F, 0x9F, 0xB4, 0x7D, 0x2C, 0x5F, 0x9D, 0xDE, 0x31);

//{11DF58C5-3FDC-4BB9-984D-D1D83DDF4D37}
DEFINE_GUID(MEMORY_MAP_GUID,
            0x11DF58C5, 0x3FDC, 0x4BB9, 0x98, 0x4D, 0xD1, 0xD8, 0x3D, 0xDF, 0x4D, 0x37);

//{0df632e9-5c48-43aa-b8bd-5ff61805025f}
DEFINE_GUID(RAW_DUMP_TABLE_GUID,
            0x0df632e9, 0x5c48, 0x43aa, 0xb8, 0xbd, 0x5f, 0xf6, 0x18, 0x05, 0x02, 0x5f);

//{62fb2678-933f-4177-8629-ff3f705502e3}
DEFINE_GUID(DDR_DATA_GUID,
            0x62fb2678, 0x933f, 0x4177, 0x86, 0x29, 0xff, 0x3f, 0x70, 0x55, 0x02, 0xe3);

//
// Defines for using disk map as a dump location.
//

#define OFFLINE_MEMORY_DUMP_LOCATION_DISK_VAR_NAME  L"OfflineMemoryDumpLocationDisk"

#define OFFLINE_MEMORY_DUMP_LOCATION_DISK_REGIONS_VAR_NAME  L"OfflineMemoryDumpLocationDiskRegions"

typedef struct 
{
    UINT64      BaseOffset;
    UINT64      Length;
} DISK_REGION, *PDISK_REGION;

#ifdef ANY_SIZE
#undef ANY_SIZE
#endif
#define ANY_SIZE 1

typedef struct
{
    UINT32      RegionCount;
    DISK_REGION Regions[ANY_SIZE];
} DISK_MAP, *PDISK_MAP;

//
// Define for SD-Filebased raw dump
//
#define RAW_DUMP_HEADER_FILE_NAME       L"rawdump.bin"
#define RAW_DUMP_START_FOLDER_NUMBER    1
#define RAW_DUMP_MAX_FOLDER_COUNT       100

//
// SBL creates the errfile.txt if the dump on the SD card
// is incomplete.
//

#define SBL_DUMP_ERROR_FILE L"errfile.txt"

//
// Cookie file that must be in root of SD card 
// in order for SBL to write dump to SD.
//
#define RAW_DUMP_COOKIE_FILE_NAME   L"rdcookie.txt"

//
//  Raw dump defintions
//

#define RAW_DUMP_HEADER_SIGNATURE_LENGTH                0x8

#define RAW_DUMP_HEADER_VERSION                         0x00001000

#define RAW_DUMP_HEADER_FLAGS_VALID                     0x1
#define RAW_DUMP_HEADER_FLAGS_INSUFFICIENT_STORAGE      0x2

#define RAW_DUMP_SECTION_TYPE_RESERVED                  0x0
#define RAW_DUMP_SECTION_TYPE_DDR_RANGE                 0x1
#define RAW_DUMP_SECTION_TYPE_CPU_CONTEXT               0x2
#define RAW_DUMP_SECTION_TYPE_SV_SPECIFIC               0x3

#define RAW_DUMP_SECTION_HEADER_VERSION                 0x00001000

#define RAW_DUMP_SECTION_HEADER_NAME_LENGTH 0x14

#include <pshpack1.h>
typedef struct 
{
    UINT64                 Base;
} DDR_INFORMATION, *PDDR_INFORMATION;
 
typedef struct
{
    UINT32                  Reserved;
} CPU_CONTEXT_INFORMATION, *PCPU_CONTEXT_INFORMATION;

#define RAW_DUMP_SV_SPECIFIC_DATA_LENGTH    0x10

typedef struct
{
    UCHAR   SVSpecificData[RAW_DUMP_SV_SPECIFIC_DATA_LENGTH];
} SV_SPECIFIC_INFORMATION, *PSV_SPECIFIC_INFORMATION;


typedef struct
{
    UINT32                          Flags;
    UINT32                          Version;
    UINT32                          Type;
    UINT64                          Offset;
    UINT64                          Size;

    union 
    {
        DDR_INFORMATION             DDRInformation;
        CPU_CONTEXT_INFORMATION     CPUContextInformation;
        SV_SPECIFIC_INFORMATION     SVSpecificInformation;
    } u;

    UCHAR                           Name[RAW_DUMP_SECTION_HEADER_NAME_LENGTH];
} RAW_DUMP_SECTION_HEADER, *PRAW_DUMP_SECTION_HEADER;

#define RAW_DUMP_HEADER_SIGNATURE                       (UINT64)(0x21706D445F776152)  // 8 Bytes - "Raw_Dmp!" - in hex, LittleEndian order

typedef struct
{
    UINT64                  Signature;  // Defined as 8 bytes: 0x52, 0x61, 0x77, 0x5F, 0x44, 0x6D, 0x70, 0x21 
    UINT32                  Version;
    UINT32                  Flags;
    UINT64                  OsData;
    UINT64                  CpuContext;
    UINT32                  ResetTrigger;
    UINT64                  DumpSize;
    UINT64                  TotalDumpSizeRequired;
    UINT32                  SectionsCount;
    RAW_DUMP_SECTION_HEADER SectionTable[ANY_SIZE];
} RAW_DUMP_HEADER, *PRAW_DUMP_HEADER;
#include <poppack.h>

//defined with the same name in blfirmw.h
#ifndef FW_TABLE_OFFLINE_CRASHDUMP
#define FW_TABLE_OFFLINE_CRASHDUMP 9
#endif

//
// Defines shared between wpcrdmp and wpdmp.
//
typedef enum {
    OfflineDumpVersionUndefined = 0x0, 
    OfflineDumpUEFI = 0xA,
    OfflineDumpSBL = 0xB
} OFFLINE_DUMP_VERSION;


//
// AP Reg defines.
//
#define AP_REG_STRUCTURE_MAGIC_VALUE    0x44434151
#define AP_REG_STRUCTURE_VERSION_2      0x2
#define AP_REG_STRUCTURE_VERSION_4      0x4
#define AP_REG_STRUCTURE_MAX_VERSION    AP_REG_STRUCTURE_VERSION_4
#define AP_REG_MAX_CPUS                 0x4

enum CPUMode
{
    usr_mode = 0x10,
    fiq_mode = 0x11,
    irq_mode = 0x12,
    svc_mode = 0x13,
    mon_mode = 0x16,
    abt_mode = 0x17,
    hyp_mode = 0x1a,
    und_mode = 0x1b,
    sys_mode = 0x1f,
};

//
// sc_status
//
typedef struct _CPU_STATUS
{
    UINT32 Status;
}CPU_STATUS, *PCPU_STATUS;

//
// sc_status Bits
// 
#define SC_STATUS_NS            0x00000001
#define SC_STATUS_WDT           0x00000002
#define SC_STATUS_SGI           0x00000004
#define SC_STATUS_WARM_BOOT     0x00000008
#define SC_STATUS_DBI_RSVD      0x00000010

//
// version 4 defines.
//
#define SC_STATUS_CPU_CONTEXT   0x00000020


//
// tzbsp_mon_cpu_ctx_t
//
typedef struct _SECURE_CPU_CONTEXT
{
    UINT32      Mon_Lr;
    UINT32      Mon_Spsr;
    UINT32      Usr_R0;
    UINT32      Usr_R1;
    UINT32      Usr_R2;
    UINT32      Usr_R3;
    UINT32      Usr_R4;
    UINT32      Usr_R5;
    UINT32      Usr_R6;
    UINT32      Usr_R7;
    UINT32      Usr_R8;
    UINT32      Usr_R9;
    UINT32      Usr_R10;
    UINT32      Usr_R11;
    UINT32      Usr_R12;
    UINT32      Usr_R13;
    UINT32      Usr_R14;
    UINT32      Irq_Spsr;
    UINT32      Irq_R13;
    UINT32      Irq_R14;
    UINT32      Svc_Spsr;
    UINT32      Svc_R13;
    UINT32      Svc_R14;
    UINT32      Abt_Spsr;
    UINT32      Abt_R13;
    UINT32      Abt_R14;
    UINT32      Und_Spsr;
    UINT32      Und_R13;
    UINT32      Und_R14;
    UINT32      Fiq_Spsr;
    UINT32      Fiq_R8;
    UINT32      Fiq_R9;
    UINT32      Fiq_R10;
    UINT32      Fiq_R11;
    UINT32      Fiq_R12;
    UINT32      Fiq_R13;
    UINT32      Fiq_R14;
}SECURE_CPU_CONTEXT, *PSECURE_CPU_CONTEXT;

//
// tzbsp_cpu_ctx_t
//
typedef struct _NON_SECURE_CPU_CONTEXT
{
    SECURE_CPU_CONTEXT  Saved_Ctx;
    UINT32              Mon_Sp;
    UINT32              Wdog_Pc;
}NON_SECURE_CPU_CONTEXT, *PNON_SECURE_CPU_CONTEXT;

//
// wdt_sts
//
typedef struct _WDOG_STATUS
{
    UINT32  Status;
} WDOG_STATUS, *PWDOG_STATUS;

typedef struct _AP_REG_B_FAMILY_HEADER
{
    UINT32                      Magic;
    UINT32                      Version;
    UINT32                      CPU_Count;
    //  Followed by array of CPU_STATUS
    //  Followed by array of NON_SECURE_CPU_CONTEXT
    //  Followed by one secure cpu context
    //  Followed by Array of WDOG_STATUS
}AP_REG_B_FAMILY_HEADER, *PAP_REG_B_FAMILY_HEADER;

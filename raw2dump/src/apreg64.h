//
// Contains the definition of APREG 64 bit
//
#pragma once
#include <stdlib.h>
#include <stdio.h>
#include "DumpUtil.h"

#define NUM_CPU_STATUS_ENTRIES 4
#define MAX_NUM_ENTRIES 0x110

//
// CPU status word 0 bits
//
#define CPU_STATUS_A53      0x01
#define CPU_STATUS_A57      0x02
#define CPU_STATUS_HYDRA    0x03
#define CPU_STATUS_MASK     0x03

//
// CPU status word 1 bits
//
#define NS_WDT_FROM_WARM_BOOT               0x08
#define NS_WDT_FROM_TZ                      0x04
#define NS_WDT_BITE_PROMTPED_DUMP           0x02
#define NS_WDT_BITE_PROMTPED_NS_EXECUTION   0x01

typedef enum
{
    MSM_DUMP_TYPE_DATA  = 0x00,
    MSM_DUMP_TYPE_TABLE = 0x01
} APREG_MSM_DUMP_TYPE;


//
// The LSB has no role in indicating what kind of data it is.
// Although the ID values are 0x00, 0x60 and so on, the enum below 
// represents the true data type. The LSB is processor number.
//
typedef enum
{
    MSM_DUMP_DATA_CPU_CTX	     = 0x0,
    MSM_DUMP_DATA_L1_INST_CACHE	 = 0x6,
    MSM_DUMP_DATA_L1_DATA_CACHE	 = 0x8,
    MSM_DUMP_DATA_ETM_REG	     = 0xA,
    MSM_DUMP_DATA_L2_CACHE	     = 0xC,
    MSM_DUMP_DATA_L3_CACHE	     = 0xD,
    MSM_DUMP_DATA_OCMEM	         = 0xE,
    MSM_DUMP_DATA_TMC_ETF	     = 0xF,
    MSM_DUMP_DATA_TMC_REG	     = 0x10,
    MSM_DUMP_DATA_MAX	         = MAX_NUM_ENTRIES
} MSM_DUMP_DATA_ID;


//
// A Dump table contains one or more Dump 
// Entries
//

typedef struct _AP_REG_MSM_DUMP_ENTRY
{
    //
    // This field specifies various ids  of table  or data entries. 
    // Enum MSM_DUMP_DATA_ids defines various data ids & 
    // enum MSM_DUMP_Table_ids are possible ids for table 
    //
    UINT32                  id;
    
    //
    // Reserved – Not used
    //
    UINT8                   Name[32];
    
    // 
    // This field specifies the type of entry:  
    // data Vs Table (enum MSM_DUMP_TYPE are possible values). 
    //    
    APREG_MSM_DUMP_TYPE     type;

    //
    //  The address to the next entry.
    //
    ULONGLONG               Address;
}AP_REG_MSM_DUMP_ENTRY, *PAP_REG_MSM_DUMP_ENTRY;


typedef struct _AP_REG_MSM_DUMP_TABLE
{
    UINT32                  Version;    // 0x01
    UINT32                  Num_Entries;
    AP_REG_MSM_DUMP_ENTRY   Entries[1];    
}AP_REG_MSM_DUMP_TABLE, *PAP_REG_MSM_DUMP_TABLE;


typedef struct _AP_REG_MSM_DUMP_DATA
{
    UINT32      version;        // 0x00000011
    UINT32      Magic;          // 0x42445953
    UINT8       name[32];
    ULONGLONG   address;
    ULONGLONG   len;
    ULONGLONG   reserved;
}AP_REG_MSM_DUMP_DATA, *PAP_AP_REG_MSM_DUMP_DATA;


/* External CPU Dump Structure - 64 bit EL */
typedef struct _SDI_CPU64_CTXT_REGS_TYPE
{
  UINT64 x0;
  UINT64 x1;
  UINT64 x2;
  UINT64 x3;
  UINT64 x4;
  UINT64 x5;
  UINT64 x6;
  UINT64 x7;
  UINT64 x8;
  UINT64 x9;
  UINT64 x10;
  UINT64 x11;
  UINT64 x12;
  UINT64 x13;
  UINT64 x14;
  UINT64 x15;
  UINT64 x16;
  UINT64 x17;
  UINT64 x18;
  UINT64 x19;
  UINT64 x20;
  UINT64 x21;
  UINT64 x22;
  UINT64 x23;
  UINT64 x24;
  UINT64 x25;
  UINT64 x26;
  UINT64 x27;
  UINT64 x28;
  UINT64 x29;
  UINT64 x30;
  UINT64 pc;
  UINT64 currentEL;
  UINT64 sp_el3;
  UINT64 elr_el3;
  UINT64 spsr_el3;
  UINT64 sp_el2;
  UINT64 elr_el2;
  UINT64 spsr_el2;
  UINT64 sp_el1;
  UINT64 elr_el1;
  UINT64 spsr_el1;
  UINT64 sp_el0;
  UINT64 __reserved1;
  UINT64 __reserved2;
  UINT64 __reserved3;
  UINT64 __reserved4;
} SDI_CPU64_CTXT_REGS_TYPE, *PSDI_CPU64_CTXT_REGS_TYPE;


typedef struct _SDI_CPU32_CTXT_REGS_TYPE
{
   UINT64 r0;
   UINT64 r1;
   UINT64 r2;
   UINT64 r3;
   UINT64 r4;
   UINT64 r5;
   UINT64 r6;
   UINT64 r7;
   UINT64 r8;
   UINT64 r9;
   UINT64 r10;
   UINT64 r11;
   UINT64 r12;
   UINT64 r13_usr;
   UINT64 r14_usr;
   UINT64 r13_hyp;
   UINT64 r14_irq;
   UINT64 r13_irq;
   UINT64 r14_svc;
   UINT64 r13_svc;
   UINT64 r14_abt;
   UINT64 r13_abt;
   UINT64 r14_und;
   UINT64 r13_und;
   UINT64 r8_fiq;
   UINT64 r9_fiq;
   UINT64 r10_fiq;
   UINT64 r11_fiq;
   UINT64 r12_fiq;
   UINT64 r13_fiq;
   UINT64 r14_fiq;
   UINT64 pc;
   UINT64 cpsr;
   UINT64 r13_mon;
   UINT64 r14_mon;
   UINT64 r14_hyp;
   UINT64 _reserved;
   UINT64 __reserved1;
   UINT64 __reserved2;
   UINT64 __reserved3;
   UINT64 __reserved4;
}SDI_CPU32_CTXT_REGS_TYPE, *PSDI_CPU32_CTXT_REGS_TYPE;

typedef union _SDI_CPU_CTXT_REGS_TYPE
{
    SDI_CPU32_CTXT_REGS_TYPE  Cpu32_ctxt;
    SDI_CPU64_CTXT_REGS_TYPE  Cpu64_ctxt;    
}SDI_CPU_CTXT_REGS_TYPE, *PSDI_CPU_CTXT_REGS_TYPE;

typedef struct _SDICPUCtxtType
{
    UINT32                     Status[NUM_CPU_STATUS_ENTRIES];
    SDI_CPU_CTXT_REGS_TYPE     Cpu_regs;
    SDI_CPU_CTXT_REGS_TYPE     _reserved3;
}SDICPUCtxtType, *PSDICPUCtxtType;



typedef struct _CPUContext64
{
    SDICPUCtxtType        cpuContext;
    struct _CPUContext64   *Flink;    
}CPUContext64, *PCPUContext64;

typedef struct _CPUContext64List
{
    PCPUContext64   listHead;
    PCPUContext64   listEnd;
    UINT32          numProcressors;
}CPUContext64List, *PCPUContext64List;

extern BOOL g_Shared_IMEM;

HRESULT
GetAPREG64AddrFromOCIMEMSection(
    _Inout_ PDMP_CONTEXT    Context
);


HRESULT
GetAPReg64(
    _Inout_ PDMP_CONTEXT    Context
);


HRESULT
UpdateContextWithAPReg64(
    _Inout_ PDMP_CONTEXT Context
);





/*++

THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Copyright (c) Microsoft Corporation. All rights reserved

Module Name:

WpCrDmpSentinel.h

Abstract:

This file contains WpCrDmp sentinel structure definitions.

Environment:

Any

--*/


//
// Information is laid out in contiguous physical memory in the following
// fashion. Both the preboot module and offline processing tool would have
// to validate a total of three signatures, from the head sentinel, the
// tail sentinel and the subsystems-data areas.
//
// Even thought the illustration indicates CrashDumpHeader, there might be
// other information stored between head and tail sentinels or between tail-
// sentinel and subsystems-data. The preboot module and offline processing
// tools must always use appropriate member to directly find the offset of
// the next signature in physical memory.
//
// The process:
// 1.  Scan raw memory capture for the head sentinel signature.
// 2.  Read the next 4 bytes to get the offset to the next signature. 
// 3.  Validate that the tail signature.
// 4.  Read the next 4 bytes to get the offset to the next signature. 
// 5.  Validate the subsystems data signature.
//
// Address of the next signature: (PUCHAR)(&(pSentinel->NextSigOffset) + pSentinel->NextSigOffset)
//
// .-----------------.
// |  Head sentinel  |
// |-----------------|
// |   Dump header   |
// |-----------------|
// |  Tail sentinel  |
// |-----------------|
// | Subsystems data |
// '-----------------'
//



#define RAWDUMP_LAYOUT_REVISION     (0x0001)



#include <pshpack4.h>
typedef struct {
	//
	// The fields Version and DumpHeaderSize are filled only in head sentinel.
	//
	// Head sentinel signature:
	//   hex: 5c 2f 5c 2f 50 43 6c 32 44 7c 5c 2f 7c 50 47 6f
	// ascii:  \  /  \  /  P  C  l  2  D  |  \  /  |  P  G  o --> \/\/PCl2D|\/|PGo
	//
	//
	// Tail sentinel signature:
	//   hex: 5c 2f 5c 2f 50 43 6c 32 44 7c 5c 2f 7c 50 65 4e
	// ascii:  \  /  \  /  P  C  l  2  D  |  \  /  |  P  e  N --> \/\/PCl2D|\/|PeN
	//
	UCHAR           Sig[16];
	ULONG           NextSigOffset;
	union {
		struct {
			ULONG   Revision;
			ULONG   DumpHeaderSize;
		};
		ULONG64     Reserved;
	};
} SENTINEL, *PSENTINEL;
#include <poppack.h>


// **** Head signature ****
//   hex: 5c 2f 5c 2f 50 43 6c 32 44 7c 5c 2f 7c 50 47 6f
// ascii:  \  /  \  /  P  C  l  2  D  |  \  /  |  P  G  o --> \/\/PCl2D|\/|PGo
//
#define HEAD_SENTINEL_SIGNATURE {0x5c, 0x2f, 0x5c, 0x2f, 0x50, 0x43, 0x6c, \
	0x32, 0x44, 0x7c, 0x5c, 0x2f, 0x7c, 0x50, 0x47, 0x6f}

//
// **** Tail signature ****
//   hex: 5c 2f 5c 2f 50 43 6c 32 44 7c 5c 2f 7c 50 65 4e
// ascii:  \  /  \  /  P  C  l  2  D  |  \  /  |  P  e  N --> \/\/PCl2D|\/|PeN
//
#define TAIL_SENTINEL_SIGNATURE {0x5c, 0x2f, 0x5c, 0x2f, 0x50, 0x43, 0x6c, \
	0x32, 0x44, 0x7c, 0x5c, 0x2f, 0x7c, 0x50, 0x65, 0x4e}

//
// **** Guard Value *****
//   hex: 5c 2f 5c 2f 50 38 43 6c 32 44 6c 5c 2f 6c 50 00
// ascii:  \  /  \  /  P  8  C  l  2  D  l  \  /  l  P    --> \/\/PC8l2Dl\/lP
//

#define GUARD_SENTINEL_VALUE {0x5c, 0x2f, 0x5c, 0x2f, 0x50, 0x38, 0x43, \
	0x6c, 0x32, 0x44, 0x6c, 0x5c, 0x2f, 0x6c, 0x50, 0x00}
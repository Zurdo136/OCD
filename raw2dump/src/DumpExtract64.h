/*++

Copyright (c) October 2014 Microsoft Corporation, All Rights Reserved

Module Name: DumpExtract64.h

Environment: User Mode

--*/

#pragma once


#include <windows.h>
#include "dumputil.h"

NTSTATUS
ExtractWindowsDumpFile64(PDMP_CONTEXT Context);


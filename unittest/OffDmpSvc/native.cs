//----------------------------------------------------------------------------------------------------------------------
// <copyright file="Native.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Defines the NativeMethods type.</summary>
//----------------------------------------------------------------------------------------------------------------------


namespace OffDmpSvc
{
    using System;
    using System.Runtime.InteropServices;

    static class NativeMethods
    {
    
//        private const string KERNELDLL = "kernel32.dll";     // Desktop
        private const string KERNELDLL = "kernelbase.dll";      // Phone

            [DllImport(KERNELDLL)]
            public static extern IntPtr LoadLibrary(string dllToLoad);

            [DllImport(KERNELDLL)]
            public static extern IntPtr GetProcAddress(IntPtr hModule, string procedureName);

            [DllImport(KERNELDLL)]
            public static extern bool FreeLibrary(IntPtr hModule);

    } // END:  class Native2Methods
} // END:  namespace OffDmpSvc

//----------------------------------------------------------------------------------------------------------------------
// <copyright file="OCDTest.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary>Defines the OCDTest type.</summary>
//----------------------------------------------------------------------------------------------------------------------
/*
    typedef struct
    {
        string      name;
        IntPtr      address;
    } TestFuncItem, *pTestFuncItem;
*/
    
namespace OffDmpSvc
{
    using System;
    using System.Diagnostics;
    using System.Collections;
    using System.IO;
    using System.Runtime.InteropServices;
    using WEX.Logging.Interop;
    using WEX.TestExecution;
    using WEX.TestExecution.Markup;


    [TestClass]
    public class OCDTest
    {
         #pragma warning disable 0649

        // This enum should have a value for each exported function to test
        private enum TEST_FUNC_ENUM {
            CHECK_AND_SUBMIT_OFFLINE_CRASH,         // "CheckAndSubmitOfflineCrash",
            IS_OFF_DUMP_READY,                      // "IsOffDumpReady",
            DETERMINE_OFFLINE_DUMP_VERSION,         // "DetermineOfflineDumpVersion",
            CHECK_OFFLINE_CRASH_DUMP_ENABLED,       // "CheckOfflineCrashdumpEnabled",
            IS_SBL_DUMP_EXPECTED,                   // "IsSBLDumpExpected"
            GET_OFFLINE_MEMORY_DUMP_USE_CAPBILITY,  // "GetOfflineMemoryDumpUseCapability"
//            FIND_DUMP_PARTITION,                    // "FindDumpPartition"
            LOCATE_AND_COPY_RAW_DUMP,               // LocateAndCopyRawDumpWrapper
            GET_DUMP_LOCATION,                      // GetDumpLocation
            GET_DUMP_FORMAT,                        // GetDumpFormat
            VERIFY_RAW_DUMP_HEADER,                 // VerifyRawDumpHeaderWrapper
            COLLATE_RAW_DUMPS,                      // CollateSDRawDumpsWrapper
            VERIFY_RAW_DUMP_SECTION_TABLE,          // VerifyRawDumpSectionTableWrapper
            BUILD_DDR_MEMORY_MAP,                   // BuildDDRMemoryMapWrapper
            GET_DUMP_INSTANCE,                      // GetDumpInstanceWrapper
            MAX_COUNT                               // MAX_TEST_FUNC_ENUM
        };

        // Use this enum to show dump location results
        private enum SBL_DUMP_LOCATION {
            INVALID = 0,
            EMMC    = 1,
            SD      = 2
        };

        // Use this enum to show dump file format results
        private enum SBL_DUMP_FORMAT {
            INVALID = 0,
            RAW     = 1,
            FILE    = 2
        };

		// Structure to capture the List of tests to run
        private struct TestFuncItem
        {
            public string   	name;
            public IntPtr   	address;
			public bool			passed;
        };
		
		// Instantiation of the tret array
        private TestFuncItem[] TestFunctionList = new TestFuncItem[(int)TEST_FUNC_ENUM.MAX_COUNT];
        
        [ClassInitialize]
        public static void TestClassSetup(TestContext testContext)
        {
        }
        
        [ClassCleanup]
        public static void TestClassCleanup()
        {
//            objectFactory.Dispose();
        }

        // Function to get the pointer to the function address in the test DLL
        private IntPtr GetFunctionPointer( int idx )
        {
            if( String.IsNullOrEmpty(TestFunctionList[idx].name ) )
            {
                Verify.Fail("Missing function name for function " + Convert.ToString(idx) );
            }
            else if( TestFunctionList[idx].address == IntPtr.Zero )
            {
                Verify.Fail("Invalid or Missing address for function " + TestFunctionList[idx].name + "()" );
            }

 
            IntPtr pAddressOfFunctionToCall = NativeMethods.GetProcAddress(OCDHandle, TestFunctionList[idx].name );

            if( pAddressOfFunctionToCall != TestFunctionList[idx].address )
                Verify.Fail("  FUnction address resolution failure!" );
            
            return pAddressOfFunctionToCall;
        }

        
        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Some basic init work...
		// reated this as a test pass so that the array is populated at test start time.
        public  void init()
        {
            Log.Comment("Initializing " + Convert.ToString( (int)TEST_FUNC_ENUM.MAX_COUNT ) + " functions for testing...");

            // This list should have an enum value and fucntion name for each function to test
            TestFunctionList[(int)TEST_FUNC_ENUM.CHECK_AND_SUBMIT_OFFLINE_CRASH].name           = "CheckAndSubmitOfflineCrashWrapper";          // "CheckAndSubmitOfflineCrash";
            TestFunctionList[(int)TEST_FUNC_ENUM.IS_OFF_DUMP_READY].name                        = "IsOffDumpReadyWrapper";                      // "IsOffDumpReady";
            TestFunctionList[(int)TEST_FUNC_ENUM.DETERMINE_OFFLINE_DUMP_VERSION].name           = "DetermineOfflineDumpVersionWrapper";         // "DetermineOfflineDumpVersion";
            TestFunctionList[(int)TEST_FUNC_ENUM.CHECK_OFFLINE_CRASH_DUMP_ENABLED].name         = "CheckOfflineCrashdumpEnabledWrapper";        // "CheckOfflineCrashdumpEnabled";
            TestFunctionList[(int)TEST_FUNC_ENUM.IS_SBL_DUMP_EXPECTED].name                     = "IsSBLDumpExpectedWrapper";                   // "IsSBLDumpExpected";
            TestFunctionList[(int)TEST_FUNC_ENUM.GET_OFFLINE_MEMORY_DUMP_USE_CAPBILITY].name    = "GetOfflineMemoryDumpUseCapabilityWrapper";   // "GetOfflineMemoryDumpUseCapability";
            TestFunctionList[(int)TEST_FUNC_ENUM.LOCATE_AND_COPY_RAW_DUMP].name                 = "LocateAndCopyRawDumpWrapper";                // "LocateAndCopyRawDumpWrapper"
            TestFunctionList[(int)TEST_FUNC_ENUM.GET_DUMP_LOCATION].name                        = "GetDumpLocation";                            // GetDumpLocation
            TestFunctionList[(int)TEST_FUNC_ENUM.GET_DUMP_FORMAT].name                          = "GetDumpFormat";                              // GetDumpFormat
            TestFunctionList[(int)TEST_FUNC_ENUM.VERIFY_RAW_DUMP_HEADER].name                   = "VerifyRawDumpHeaderWrapper";                 // VerifyRawDumpHeaderWrapper
            TestFunctionList[(int)TEST_FUNC_ENUM.COLLATE_RAW_DUMPS].name                        = "CollateSDRawDumpsWrapper";                   // CollateSDRawDumpsWrapper
            TestFunctionList[(int)TEST_FUNC_ENUM.VERIFY_RAW_DUMP_SECTION_TABLE].name            = "VerifyRawDumpSectionTableWrapper";           // VerifyRawDumpSectionTableWrapper
            TestFunctionList[(int)TEST_FUNC_ENUM.BUILD_DDR_MEMORY_MAP].name                     = "BuildDDRMemoryMapWrapper";                   // BuildDDRMemoryMapWrapper
            TestFunctionList[(int)TEST_FUNC_ENUM.GET_DUMP_INSTANCE].name                        = "GetDumpInstanceWrapper";                     // GetDumpInstanceWrapper
        }
        

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test if the DLL can be loaded
        public void TestODCLoad()
        {
            Log.Comment("Testing if "+OCD_DLL_Path+" can be loaded:");
            OCDHandle = NativeMethods.LoadLibrary(OCD_DLL_Path);

            Verify.IsNotNull(OCDHandle, "Successfully Loaded DLL" );

        }

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test to determine if the exported functions are present in the DLL
        public void TestODCFunctionExports()
        {
            Log.Comment("Checking for presence of known functions in "+OCD_DLL_Path+":");
         
            int missedTest = 0;
            int idxFunc = 0;
            
            // Loop through the array of exported functions we want to test.
            for( ; idxFunc < TestFunctionList.Length; idxFunc++ )
            {
                // Skip any blank array items (functions)
                if( String.IsNullOrEmpty(TestFunctionList[idxFunc].name) )
                {
                    missedTest++;
                    continue;
                }
                
               string outString = "Test for presence of function " + TestFunctionList[idxFunc].name + "():";
               Log.Comment(outString);
 
               TestFunctionList[idxFunc].address = NativeMethods.GetProcAddress(OCDHandle, TestFunctionList[idxFunc].name );
               if( TestFunctionList[idxFunc].address != IntPtr.Zero )
                    Log.Comment("    Function " + TestFunctionList[idxFunc].name + "() found at address: " + Convert.ToString(TestFunctionList[idxFunc].address) );
            }

            if( missedTest > 0 )
            {
                string outString = "WARNING: " + missedTest + " function presence ";
                if( missedTest == 1 )
                    outString += " test was skipped because it is";
                else
                    outString += " tests were skipped because they are";
                outString += " missing from the function list.\r\n";
                outString += "   Althoug this is not a test failure, check that all functions were included in the function test list.";
                Log.Comment(outString);
            }
            else
            {
               Log.Comment("Processed " + Convert.ToString(idxFunc) + " functions with no errors.");
            }

        }

        // Private function to check the erros from the wrapper functions and fail appropriately
        private void dumpWrapperError(int val, string fName)
        {
            if( val <= 0 )
            {
                switch( val)
                {
                    case 0:
                        break;
                    case -1:
                        Verify.Fail("Function " + fName + "() failed");
                        break;
                    case -2:
                        Log.Comment("An invalid parameter was use to test " + fName + "()");
                        break;
                    default:
                        Verify.Fail("An unknown error was returned from " + fName + "() wrapper, reurned value: " + Convert.ToString(val));
                        break;
                }                
            }
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int DetermineOfflineDumpVersion(int x);

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_DetermineOfflineDumpVersion()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.DETERMINE_OFFLINE_DUMP_VERSION );
            
            DetermineOfflineDumpVersion  OfflineDumpVersion = (DetermineOfflineDumpVersion)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(DetermineOfflineDumpVersion) );
            // Use param 1 to get Version
            int retVal = OfflineDumpVersion(1);
            dumpWrapperError(retVal, "OfflineDumpVersion");
            if( (retVal<1) && (retVal>2) )
                Verify.Fail( "Version is invalid: " + Convert.ToString(retVal) + " should be in range [1:2]");
            Log.Comment("                   Version: " + Convert.ToString(retVal) );

            // Use param 2 to get AbnormalResetOccured
            retVal = OfflineDumpVersion(2);
            dumpWrapperError(retVal, "OfflineDumpVersion");
            if( (retVal<0) && (retVal>1) )
                Verify.Fail( "AbnormalResetOccured is invalid: " + Convert.ToString(retVal) + " should be in range [0:1]");
            Log.Comment("      AbnormalResetOccured: " + Convert.ToString(retVal) );
            AbnormalResetOccurred = (uint)retVal;
            
            // Use param 3 to get OfflineMemoryDumpCapable
            retVal = OfflineDumpVersion(3);
            dumpWrapperError(retVal, "OfflineDumpVersion");
            Log.Comment("      OfflineMemoryDumpCapable: " + Convert.ToString(retVal) );
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int DetermineIsOffDumpReady(int t);

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_IsOffDumpReady()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.IS_OFF_DUMP_READY );

            DetermineIsOffDumpReady  IsOffDumpReady = (DetermineIsOffDumpReady)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(DetermineIsOffDumpReady) );
            // get value of IsDumpEnabled and verify result
            int retVal = IsOffDumpReady(1);
            dumpWrapperError(retVal, "IsOffDumpReady");
            if( (retVal<0) && (retVal>1) )
                Verify.Fail( "IsDumpEnabled is invalid: " + Convert.ToString(retVal) + " should be in range [0:1]");
            Log.Comment("    IsDumpEnabled: " + Convert.ToString(retVal) );

            // get value of IsDumpExpected and verify result
            retVal = IsOffDumpReady(2);
            dumpWrapperError(retVal, "IsOffDumpReady");
            if( (retVal<0) && (retVal>1) )
                Verify.Fail( "IsDumpExpected is invalid: " + Convert.ToString(retVal) + " should be in range [0:1]");
            Log.Comment("   IsDumpExpected: " + Convert.ToString(retVal) );
        }



        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int GetOfflineMemoryDumpUseCapability();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_GetOfflineMemoryDumpUseCapability()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.GET_OFFLINE_MEMORY_DUMP_USE_CAPBILITY );

            GetOfflineMemoryDumpUseCapability  GetOffMemDmpUseCap = (GetOfflineMemoryDumpUseCapability)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(GetOfflineMemoryDumpUseCapability) );
            int retVal = GetOffMemDmpUseCap();
            dumpWrapperError(retVal, "GetOffMemDmpUseCap");
            Log.Comment("      GetOffMemDmpUseCap() : " + Convert.ToString( retVal ) );
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int DetermineIsSBLDumpExpected(int i);

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_IsSBLDumpExpected()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.IS_SBL_DUMP_EXPECTED );

            DetermineIsSBLDumpExpected  isSBLDumpExpected = (DetermineIsSBLDumpExpected)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(DetermineIsSBLDumpExpected) );

            // Test for invalid input to wrapper
            int retVal = isSBLDumpExpected(1);
            dumpWrapperError(retVal, "isSBLDumpExpected");
            Log.Comment( "                    bGood:  " + Convert.ToString(retVal) );   //isSBLDumpExpected(1)) );

            retVal = isSBLDumpExpected(2);
            dumpWrapperError(retVal, "isSBLDumpExpected");
            Log.Comment( "   IsAbnormalResetNonZero:  " + Convert.ToString(retVal) );   //isSBLDumpExpected(2)) );

            retVal = isSBLDumpExpected(3);
            dumpWrapperError(retVal, "isSBLDumpExpected");
            Log.Comment( "   IsUseCapabilityPresent:  " + Convert.ToString(retVal) );   //isSBLDumpExpected(3)) );

            retVal = isSBLDumpExpected(4);
            dumpWrapperError(retVal, "isSBLDumpExpected");
            Log.Comment( "        IsUseCapabilitySet: " + Convert.ToString(retVal) );   //isSBLDumpExpected(4)) );

        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int FindDumpPartition();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int DumpData();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_FindDumpPartition()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.LOCATE_AND_COPY_RAW_DUMP );
            FindDumpPartition  FindDmpPart = (FindDumpPartition)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(FindDumpPartition) );
            int retVal = FindDmpPart();
            dumpWrapperError(retVal, "LocateAndCopyRawDump");

            if(retVal>0)
            {
                switch(retVal)
                {
                    case 1:
                        Verify.Fail("   ERROR: function found invalid hDisk");
                        break;
                    case 2:
                        Verify.Fail( "   ERROR: function found invalid diskOffset");
                        break;
                    case 3:
                        Verify.Fail( "   ERROR: function found invalid RawDumpPartitionLength");
                        break;
                    default:
                        Verify.Fail( "   ERROR: unknown return value from wrapper");
                        break;
                }
            }
            else
            {
                pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.GET_DUMP_LOCATION );
                DumpData  dmpData = (DumpData)Marshal.GetDelegateForFunctionPointer(
                                                                        pAddressOfFunctionToCall,
                                                                        typeof(DumpData) );
                switch( dmpData() )
                {
                    case (int)SBL_DUMP_LOCATION.EMMC:
                        Log.Comment("Dump file location: Phone" );
                        break;
                    case (int)SBL_DUMP_LOCATION.SD:
                        Log.Comment("Dump file location: SD Card");
                        break;
                    case (int)SBL_DUMP_LOCATION.INVALID:
                        Verify.Fail("Dump file location: INVALID");
                        break;
                    default:
                        Verify.Fail("Dump file location: Unknown");
                        break;
                }

                pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.GET_DUMP_FORMAT );
                dmpData = (DumpData)Marshal.GetDelegateForFunctionPointer(
                                                                        pAddressOfFunctionToCall,
                                                                        typeof(DumpData) );
                switch( (int)dmpData() )
                {
                    case (int)SBL_DUMP_FORMAT.FILE:
                        Log.Comment("  Dump file format: File" );
                        break;
                    case (int)SBL_DUMP_FORMAT.RAW:
                        Log.Comment("  Dump file format: RAW");
                        break;
                    case (int)SBL_DUMP_FORMAT.INVALID:
                        Verify.Fail("  Dump file format: INVALID");
                        break;
                    default:
                        Verify.Fail("  Dump file format: Unknown");
                        break;
                }
            }
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int VerifyRawDumpHeader();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_VerifyRawDumpHeader()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.VERIFY_RAW_DUMP_HEADER );
            VerifyRawDumpHeader  testFunc = (VerifyRawDumpHeader)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(VerifyRawDumpHeader) );
            int retVal = testFunc();
            dumpWrapperError(retVal, "VerifyRawDumpHeader");

            switch(retVal)
            {
                case 1:
                    Log.Comment("   A valid RAW Dump header was found");
                    break;
                case 2:
                    Verify.Fail("   ERROR: Invalid RAW Dump header");
                    break;
                default:
                    Verify.Fail("   ERROR: unknown return value from wrapper: " + Convert.ToString(retVal) );
                    break;
            }
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int CollateSDRawDumps();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_CollateSDRawDumps()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.COLLATE_RAW_DUMPS );
            CollateSDRawDumps  testFunc = (CollateSDRawDumps)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(CollateSDRawDumps) );
            int retVal = testFunc();
            dumpWrapperError(retVal, "CollateSDRawDumps");

            switch( retVal )
            {
                case 0:
                    Log.Comment("   Sucessfully Collated SV Raw Dumps from SD card");
                    break;
                case 1:
                    Log.Comment("   No Need to Collate SV Raw Dumps from SD card");
                    break;
                default:
                    Verify.Fail("   ERROR: Collated SV Raw Dumps failed with status: " + Convert.ToString(retVal) );
                    break;
            }
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int VerifyRawDumpSectionTable();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_VerifyRawDumpSectionTable()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.VERIFY_RAW_DUMP_SECTION_TABLE );
            VerifyRawDumpSectionTable  testFunc = (VerifyRawDumpSectionTable)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(VerifyRawDumpSectionTable) );
            int retVal = testFunc();
            dumpWrapperError(retVal, "VerifyRawDumpSectionTable");

            switch(retVal)
            {
                case 1:
                    Log.Comment("   A valid RAW Dump Section Table was found");
                    break;
                case 2:
                    Verify.Fail("   ERROR: Invalid RAW Dump Section Table");
                    break;
                default:
                    Verify.Fail("   ERROR: unknown return value from wrapper: " + Convert.ToString(retVal) );
                    break;
            }
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int BuildDDRMemoryMap();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_BuildDDRMemoryMap()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.BUILD_DDR_MEMORY_MAP );
            BuildDDRMemoryMap  testFunc = (BuildDDRMemoryMap)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(BuildDDRMemoryMap) );
            int retVal = testFunc();
            dumpWrapperError(retVal, "BuildDDRMemoryMap");

            switch(retVal)
            {
                case 0:
                    Log.Comment("   A valid DDR Memory Map was built");
                    break;
                default:
                    Verify.Fail("   ERROR: unknown return value from wrapper: " + Convert.ToString(retVal) );
                    break;
            }
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int GetDumpInstance();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_GetDumpInstance()
        {
            IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.GET_DUMP_INSTANCE );
            GetDumpInstance  testFunc = (GetDumpInstance)Marshal.GetDelegateForFunctionPointer(
                                                                                        pAddressOfFunctionToCall,
                                                                                        typeof(GetDumpInstance) );
            int retVal = testFunc();
            dumpWrapperError(retVal, "GetDumpInstance");

            switch(retVal)
            {
                case 0:
                    Log.Comment("   A valid Dump Instance was found ");
                    break;
                default:
                    Verify.Fail("   ERROR: unknown return value from wrapper: " + Convert.ToString(retVal) );
                    break;
            }
        }


        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        private delegate int RunOfflineCrash();

        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCFunction_FullOfflineCrashCollection()
        {
            if( AbnormalResetOccurred == 1 )
            {
                IntPtr pAddressOfFunctionToCall = GetFunctionPointer( (int)TEST_FUNC_ENUM.CHECK_AND_SUBMIT_OFFLINE_CRASH );

                RunOfflineCrash  RunOfflineCrashDump = (RunOfflineCrash)Marshal.GetDelegateForFunctionPointer(
                                                                                            pAddressOfFunctionToCall,
                                                                                            typeof(RunOfflineCrash) );

                int retVal = RunOfflineCrashDump();
                dumpWrapperError(retVal, "GetDumpInstance");
            }
            else
                Log.Comment("   Device did not generate a crash dump, AbnormalResetOccurred=0 ");
        }


        [TestMethod]
        [Owner("Jose Pagan/JoPagan")]
        [Priority(0)]
        // Test the exported function is OK
        public void TestODCUnLoad()
        {
            Log.Comment("Testing if "+OCD_DLL_Path+" can be un-Loaded:");
            Verify.IsTrue(NativeMethods.FreeLibrary(OCDHandle), "Successfully Un-Loaded DLL" );
        }
            

        private string  OCD_DLL_Path = "OffDmpSvc.dll";
        private IntPtr  OCDHandle = IntPtr.Zero;
        private uint    AbnormalResetOccurred = 0;
        
        //defined as OFFLINE_CRASHDUMP_CONFIGURATION_TABLE
         struct  OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE
        {
            public uint      Version;
            public uint      AbnormalResetOccurred;
            public uint      OfflineMemoryDumpCapable;
        }; // OFFLINE_CRASH_DUMP_CONFIGURATION_TABLE; //, *pOFFLINE_CRASH_DUMP_CONFIGURATION_TABLE;

    } // END:  public class OCDTest

} // END:  namespace OffDmpSvc


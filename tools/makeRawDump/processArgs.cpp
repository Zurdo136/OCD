/*++

Copyright (C) Microsoft. All rights reserved.

Module Name:
    processArgs.cpp

Environment:
    User Mode

Author:
    José Pagán (jopagan)
--*/
#pragma once

#include <string>
#include <stdio.h>
#include <stdlib.h>

#include "processArgs.h"
#include "processDDR.h"

#define HEX_PREFIX                      "0x"
#define TOKEN_DELIMITER                 ":"
#define TOKEN_BASE_DECIMAL              10
#define TOKEN_BASE_HEX                  16

// Strings used in various comparisions
#define DDR_PROXIMITY_STRING_ADJACENT   "ADJACENT"
#define DDR_PROXIMITY_STRING_SCATTER    "SCATTER"

#define DDR_ORDER_STRING_ASLISTED       "ASLISTED"
#define DDR_ORDER_STRING_ASCENDING      "ASCENDING"
#define DDR_ORDER_STRING_DESCENDING     "DESCENDING"
#define DDR_ORDER_STRING_RANDOM         "RANDOM"

// These belong only to the DDR argument
#define DDR_SECTION_ID      0
#define DDR_BASE            1
#define DDR_SIZE            2

extern std::string  outFileName;

// Table of valid command line arguments with function pointers to the token processing
//  The fields are:
//     Arg String    NoArgs     Token Parsing Function
PARAM_ROW   parameterList[] = {
    {  "FileSize",      1,      &ProcessFileSize },         // Output file size to create, should be more than computed size
    {  "FileName",      1,      &ProcessFileName },         // Set the output file (name)
    {  "Partition",     0,      &ProcessPart },             // Flag to make partition the target for output
    {  "Part",          0,      &ProcessPart },             // Flag to make partition the target for output (short form) 
    {  "DDR",           3,      &ProcessDDRParameters },    // Define a single DDR section
    {  "DDRSize",       1,      &ProcessDDRSize},           // Change the default DDR section size, for sections not defined by /DDR
    {  "DDRCount",      1,      &ProcessDDRCount },         // Number of otal DDR sections, should be more than /DDRCount
    {  "DDROrder",      1,      &ProcessDDROrder },         // How to order the DDR sections, default is ASCENDING
    {  "DDROrd",        1,      &ProcessDDROrder },         // How to order the DDR sections (short form), default is ASCENDING
    {  "DDRProximity",  1,      &ProcessDDRProximity },     // How close are DDR sections, default is ADJACENT
    {  "DDRProx",       1,      &ProcessDDRProximity },     // How close are DDR sections (short form), default is ADJACENT
    {  "NumCores",      1,      &ProcessNumCores },         // Number of cores to use for CPU section
    {  "NoPayload",     0,      &ProcessNoPayload },        // Sets the no-payload flag, write only header and sections data to file/partition
    {  "NoApReg",       0,      &ProcessNoAPReg },          // Flag to exclude the CPU section, default is to create one of size = 1
    {  "NoSvData",      0,      nullptr },                  // Flag to exclude all SV sections
    {  "NoTzData",      0,      nullptr }                   // Flag to exclude the TZ section from the SV sections
};


/****************************************************************************************************
** HRESULT ProcessCommandArguments(
**              _In_ int argc,
**              _In_ char** argv,
**              _Inout_ PDUMP_CONFIG config,
**              _Out_ std::string *failString)
**
** Description:
**  Process the command line arguments (argv) and compares them to the table of valid arguments
**  (parameterList). Arguments not on this table will fail the program.  Arguments on this table
**  can be place holders (have no processing function) or valid arguemnts.  Place holders will
**  trigger a message and be ignored. Valid arguments may have modifiers or they can be flags,
**  with no modifiers.
**
**  Flag arguments are used to set a feature or invalidate some part of the dump. They cannot have
**  any modifiers.  Their format is /<argument> where the argument name is can insensitive.
**
**  Arguments with modifiers are in the format /<argument>:<mod1>:<mod2>:<mod3>:...  where the
**  number of modifiers must be present, evven if a default value exists.  The procesing function
**  should parse the modifiers and extract the appropriate values.
**
** Arguments :
**  argc - number of arguments to parse
**  argv - array of arguments to parse (array of strings)
**  config - configuration table, apply arguemnts to this table
**  failString - class to return, populated with argument erorrs found.
**
** Return :
**  S_OK
**  E_OUTOFMEMRY
**
*****************************************************************************************************/
HRESULT ProcessCommandArguments(_In_ int argc, _In_ char** argv, _Inout_ PDUMP_CONFIG config, _Out_ std::string *failString)
{
    UNREFERENCED_PARAMETER(config);
    UNREFERENCED_PARAMETER(failString);

    HRESULT hr = S_OK;

    // Set the DDR defaults: 
    config->DDR_SectionCount    = DEFAULT_DDR_SECTION_COUNT;    // Number of sections, can be set by /DDRCount
    config->DDR_Proximity       = DDR_PROXIMITY_UNSET;             // Unset proximity of DDR sections, can be set by /DDRProximity
    config->DDR_Order           = DDR_ORDER_UNSET;              // Unset order of DDDR sections, can be set by /DDROrder
    config->DDR_Size.QuadPart   = DEFAULT_DDR_SECTION_LENGTH;   // Default DDR section size, can be set by /DDRSize

    // set the ApReg defaults: 
    config->excludeApReg        = FALSE;

    // set the SV section defaults: 
    config->excludeTZ           = FALSE;

    // Other defaults that could be configured via s switch
    config->writePayload        = TRUE;
    config->CoreCount           = INVALID_UINT32;

    if (argc > 1)
    {
        for (UINT32 i = 1; SUCCEEDED(hr) && (i < (UINT32)argc); i++)
        {
            UINT32 foundArgument = INVALID_UINT32;
            //PCHAR  tmpArg = (PCHAR)malloc(strlen(argv[i]));

            if ( ('/' == argv[i][0]) || ('-' == argv[i][0]) )
            {
                PCHAR tmpArg = (PCHAR)malloc(strlen(argv[i]));
                if (nullptr == tmpArg)
                {
                    hr = E_OUTOFMEMORY;
                }
                else
                {
                    strcpy(tmpArg, &argv[i][1]);

                    if (INVALID_UINT32 == (foundArgument = FindArgumentInParameterArray(tmpArg)))
                    { // Fail if the argument is not in the table
                        failString->append("   unknown switch ==> [");
                        failString->append(&argv[i][1]);
                        failString->append("]\r\n");
                        hr = E_FAIL;
                    }
                    else if (nullptr == parameterList[foundArgument].proccessArgsFunc)
                    { // Not yet implemented - there is no function pointer to process this argument
                        failString->append("   unsupported switch ==> [");
                        failString->append(parameterList[foundArgument].paramStr);
                        failString->append("] not implemented, will be ignored\r\n");
                    }
                    else if ( (0 == parameterList[foundArgument].numArgs) &&
                              (CHAR_OUT_OF_STRING(TOKEN_DELIMITER, 0) == tmpArg[strlen(parameterList[foundArgument].paramStr)])
                            )
                    { // No expected arguemnt modifiers and argument seems to have some
                        failString->append("   invalid switch ==> [");
                        failString->append(parameterList[foundArgument].paramStr);
                        failString->append("] takes no modifiers\r\n");
                        hr = E_FAIL;
                    }
                    else if ((nullptr != parameterList[foundArgument].proccessArgsFunc) &&
                             FAILED(hr = parameterList[foundArgument].proccessArgsFunc(config, foundArgument, tmpArg))
                             )
                    { // Failed to process the tokens - append to the error string
                        failString->append("   invalid switch modifier ==> [");
                        failString->append(&argv[i][1]);
                        failString->append("]\r\n");
                    }

                    free(tmpArg);
                }

            }
            else
            { // One of the arguments did not begin with a slash (/) or dash (-)
                failString->append(" * invalid switch ==> [");
                failString->append(argv[i]);
                failString->append("] must begin with / or -\r\n");
                hr = E_FAIL;
            }

        }

    }

    if (SUCCEEDED(hr))
    { // Use default settings if these were not specified on the command line
        if (DDR_PROXIMITY_UNSET == config->DDR_Proximity)
        {
            config->DDR_Proximity = DDR_PROXIMITY_ADJACENT;
        }

        if (DDR_ORDER_UNSET == config->DDR_Order)
        {
            config->DDR_Order = DDR_ORDER_ASCENDING;
        }

        if (INVALID_UINT32 == config->CoreCount)
        {
            config->CoreCount = DEFAULT_CORE_COUNT;
        }
    }

    return hr;
}


/****************************************************************************************************
** UINT32 FindArgumentInParameterArray(
**              _In_ PCHAR arg)
**
** Description:
**  Searches the argument array (parameterList) for the arguemnt (arg).
**
** Arguments :
**
** Return :
**  i - index of the arguemnt, in the array (parameerList)
**  (UINT32)(-1) - arg not found
**
*****************************************************************************************************/
UINT32 FindArgumentInParameterArray(_In_ PCHAR arg)
{
    UINT32 ret = INVALID_UINT32;
    UINT32 argCount = ArrayCount(parameterList);

    for (UINT32 i = 0; i < argCount; i++)
    {
        if (0 == _strnicmp(arg, parameterList[i].paramStr, strlen(parameterList[i].paramStr)))
        { // We are looking for arguments that match the table, no validation is done here.

            // For a vaild match we should have one of:
            //    a. the arg string size should match the array string - flag argument
            //    b. the argument argument string should end in with a TOKEN_DELIMITER
            if ( (strlen(arg) == strlen(parameterList[i].paramStr)) ||
                 (CHAR_OUT_OF_STRING(TOKEN_DELIMITER, 0) == CHAR_OUT_OF_STRING(arg, strlen(parameterList[i].paramStr)))
               )
            {
                ret = i;
                break;
            }

        }

    }

    return ret;
}


/****************************************************************************************************
**  HRESULT     ResizeSectionsList(
**                    _In_out_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList,
**                    _In_ UINT32 newSize)
** Description:
**  Wrapper to resize a component vector.  Each vector is essentially an array of pointers to
**  RAW_DUMP_SECTION_HEADER data types.  The vector size is determined by a default size when no
**  other information is provided.  Vectors are resized by /DDRCount or /DDR entries which define
**  specific DDR segments.
**
** Arguments:
**  sectionList - vector to resize
**  newSize - new size, has no affect if size is smaller than the current size
**
** Return:
**  S_OK
**  E_OUTOFMEMRY - when resize fails
**
*****************************************************************************************************/
HRESULT ResizeSectionsList(_Inout_ std::vector<PRAW_DUMP_SECTION_HEADER> *sectionList, _In_ UINT32 newSize)
{
    HRESULT ret = S_OK;

    if (newSize > sectionList->size())
    { // Adjust the size of the DDR section table, in necessary
        try
        {
            sectionList->resize(newSize);
        }
        catch (...)
        {
            ret = E_OUTOFMEMORY;
        }

    }

    return ret;
}

/*****************************************************************************************************
******************************************************************************************************
**  Funcitions below are to proces the tokens for each parameter
******************************************************************************************************
******************************************************************************************************/


/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_OUTOFMEMRY - when resize fails
**
*****************************************************************************************************/
HRESULT ProcessDDRParameters(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    HRESULT                     ret = S_OK;
    UINT32                      argCount = parameterList[dRow].numArgs;
    LONG                        sectionID = INVALID_UINT32;
    ULARGE_INTEGER              sectionBaseAddress = { 0 };
    ULARGE_INTEGER              sectionSize = { 0 };
    PCHAR                       paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    {
        ret = E_POINTER;
    }
    else if ((nullptr == (paramToken = strtok(argList, TOKEN_DELIMITER))) ||
             (0 != _strnicmp(paramToken, parameterList[dRow].paramStr, strlen(parameterList[dRow].paramStr)))
            )
    { // parameter is not /DDR or there are no tokens
        ret = E_INVALIDARG;
    }
    else
    { // Processing the arguments we expeect and ignore any that follow.
      // The tokens in "/DDR" command are colon separated values, decimal or hex supported
        for (UINT32 i = 0; SUCCEEDED(ret) && (i < argCount); i++)
        { 
            if (nullptr == (paramToken = strtok(NULL, TOKEN_DELIMITER)))
            { // If processing failed or null token
                ret = E_INVALIDARG;
            }
            else
            { // ensure conversion will be either HEX or decimal
                UINT tokenBase = (0 == _strnicmp(paramToken, HEX_PREFIX, strlen(HEX_PREFIX))) ? TOKEN_BASE_HEX : TOKEN_BASE_DECIMAL;

                switch (i)
                {
                    case DDR_SECTION_ID: // (0) - ID of DDR section being described
                        sectionID = atoi(paramToken);
                        break;

                    case DDR_BASE:       // (1) - Base address of section
                        sectionBaseAddress.QuadPart = _strtoui64(paramToken, NULL, tokenBase);
                        break;

                    case DDR_SIZE:       // (2) - Size of the section
                        sectionSize.QuadPart = _strtoui64(paramToken, NULL, tokenBase);
                        break;

                    default:            // Failure case - if argument count is incorrect in the table
                        ret = E_FAIL;
                        break;
                }

            }

        }

    }

    if (SUCCEEDED(ret))
    { // DDR tokens extracted, now the section to the vector list
        if (sectionID >= (cfg->sectionDDR).size())
        { // Update the vector list if section is beyond the end of the list
            cfg->DDR_SectionCount = sectionID + 1;
            ret = ResizeSectionsList(&(cfg->sectionDDR), cfg->DDR_SectionCount);
        }

        if (SUCCEEDED(ret))
        {
            cfg->sectionDDR[sectionID] = new RAW_DUMP_SECTION_HEADER;
            if (nullptr == cfg->sectionDDR[sectionID])
            {
                ret = E_OUTOFMEMORY;
            }
            else
            {
                ZeroMemory(cfg->sectionDDR[sectionID], sizeof RAW_DUMP_SECTION_HEADER);

                if (SUCCEEDED(MakeDDRNameString((PCHAR)(cfg->sectionDDR[sectionID]->Name), sizeof(cfg->sectionDDR[sectionID]->Name), sectionID)))
                {
                    cfg->sectionDDR[sectionID]->Flags = RAW_DUMP_HEADER_FLAGS_VALID;
                    cfg->sectionDDR[sectionID]->Version = RAW_DUMP_SECTION_HEADER_VERSION;
                    cfg->sectionDDR[sectionID]->Type = RAW_DUMP_SECTION_TYPE_DDR_RANGE;
                    cfg->sectionDDR[sectionID]->Size = sectionSize.QuadPart;
                    cfg->sectionDDR[sectionID]->u.DDRInformation.Base = sectionBaseAddress.QuadPart;
                    cfg->sectionDDR[sectionID]->Offset = INVALID_ULARGE_INTEGER;
                }
            }
        }
    }

    return ret;
}


/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessDDRSize(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{

    HRESULT     ret = S_OK;
    PCHAR       paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if (DEFAULT_DDR_SECTIONS_SIZE != cfg->DDR_Size.QuadPart)
    { // string already set 
        ret = E_FAIL;
    }
    else if ((nullptr == (paramToken = strtok(argList, TOKEN_DELIMITER))) ||
        (0 != _strnicmp(paramToken, parameterList[dRow].paramStr, strlen(parameterList[dRow].paramStr)))
             )
    { // parameter is not /DDRSize or there are no tokens
        ret = E_INVALIDARG;
    }
    else if (nullptr == (paramToken = strtok(NULL, TOKEN_DELIMITER)))
    { // If processing failed or null token
        ret = E_INVALIDARG;
    }
    else
    { // ensure conversion will be either HEX or decimal
        UINT tokenBase = (0 == _strnicmp(paramToken, HEX_PREFIX, strlen(HEX_PREFIX))) ? TOKEN_BASE_HEX : TOKEN_BASE_DECIMAL;

        cfg->DDR_Size.QuadPart = _strtoui64(paramToken, NULL, tokenBase);
    }

    return ret;
}


/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_OUTOFMEMRY - when resize fails
**
*****************************************************************************************************/
HRESULT ProcessDDRCount(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    HRESULT         ret = S_OK;
    PCHAR           paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if ((nullptr == (paramToken = strtok(argList, TOKEN_DELIMITER))) ||
             (0 != _strnicmp(paramToken, parameterList[dRow].paramStr, strlen(parameterList[dRow].paramStr))) ||
             (nullptr == (paramToken = strtok(NULL, TOKEN_DELIMITER)))
            )
    { // parameter is not what was expeted or there are no tokens
        ret = E_INVALIDARG;
    }
    else
    { // Look at the parameter tokens...
        UINT    tokenBase = (0 == _strnicmp(paramToken, HEX_PREFIX, strlen(HEX_PREFIX))) ? TOKEN_BASE_HEX : TOKEN_BASE_DECIMAL;
        ULONG   newDdrCount = strtoul(paramToken, NULL, tokenBase);

        if (newDdrCount > MAX_UINT32)
        { // Value is too large to handle
            ret = E_FAIL;
        }
        else if (newDdrCount > MAX_DDR_SECTIONS)
        {
            ret = E_INVALIDARG;
        }
        else
        {
            if (newDdrCount > cfg->DDR_SectionCount)
            {
                cfg->DDR_SectionCount = newDdrCount;
                ret = ResizeSectionsList(&(cfg->sectionDDR), newDdrCount);
            }

        }

    }

    return ret;
}

/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessFileSize(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    HRESULT         ret = S_OK;
    PCHAR           paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if ((nullptr == (paramToken = strtok(argList, TOKEN_DELIMITER))) ||
             (0 != _strnicmp(paramToken, parameterList[dRow].paramStr, strlen(parameterList[dRow].paramStr))) ||
             (nullptr == (paramToken = strtok(NULL, TOKEN_DELIMITER)))
            )
    { // parameter is not what was expeted or there are no tokens
        ret = E_INVALIDARG;
    }
    else
    { // Look at the parameter tokens...
        UINT    tokenBase = (0 == _strnicmp(paramToken, HEX_PREFIX, strlen(HEX_PREFIX))) ? TOKEN_BASE_HEX : TOKEN_BASE_DECIMAL;

        // cfg->RequestedRawDumpFileSize.QuadPart = strtoull(paramToken, NULL, tokenBase);
        cfg->requestedRawDumpFileSize.QuadPart = _strtoui64(paramToken, NULL, tokenBase);
    }

    return ret;
}


/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessFileName(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    UNREFERENCED_PARAMETER(dRow);
    HRESULT         ret = S_OK;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else
    {
        std::string arg(argList);
        size_t argPos = arg.find_first_of(":");

        if (std::string::npos == argPos)
        { // parameter is not what was expeted or there are no tokens
            ret = E_INVALIDARG;
        }
        else if ((TRUE == cfg->outputToPartition) || (0 != outFileName.compare(DEFAULT_DUMP_FILE_NAME)))
        { // string already set
            ret = E_FAIL;
        }
        else
        { // Capture the parameter token
            outFileName = arg.substr(argPos + 1).c_str();
            cfg->outputToPartition = FALSE;
        }

    }

    return ret;
}

/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessPart(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    UNREFERENCED_PARAMETER(dRow);

    HRESULT         ret = S_OK;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if (0 != outFileName.compare(DEFAULT_DUMP_FILE_NAME))
    { // string already set 
        ret = E_FAIL;
    }
    else
    { // No parameter here, find the first SVRawDump Partition on all mounted drives
        cfg->outputToPartition = TRUE;
    }

    return ret;
}


/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessDDRProximity(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    UNREFERENCED_PARAMETER(dRow);

    HRESULT         ret = S_OK;
    PCHAR           paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if (DDR_PROXIMITY_UNSET != cfg->DDR_Proximity)
    { // string already set 
        ret = E_FAIL;
    }
    else if ((nullptr == (paramToken = strtok(argList, TOKEN_DELIMITER))) ||
             (0 != _strnicmp(paramToken, parameterList[dRow].paramStr, strlen(parameterList[dRow].paramStr))) ||
             (nullptr == (paramToken = strtok(NULL, TOKEN_DELIMITER)))
            )
    { // parameter is not what was expeted or there are no tokens
        ret = E_INVALIDARG;
    }
    else if (0 == _strnicmp(paramToken, DDR_PROXIMITY_STRING_ADJACENT, strlen(DDR_PROXIMITY_STRING_ADJACENT)))
    { // Look at the parameters token.  The left side of the string should match, extra chars ignored. 
        cfg->DDR_Proximity = DDR_PROXIMITY_ADJACENT;
    }
    else if (0 == _strnicmp(paramToken, DDR_PROXIMITY_STRING_SCATTER, strlen(DDR_PROXIMITY_STRING_SCATTER)))
    { // Look at the parameters token.  The left side of the string should match, extra chars ignored. 
        cfg->DDR_Proximity = DDR_PROXIMITY_SCATTER;
    }
    else
    { // fail as argument erro if none of the above strings match
        ret = E_INVALIDARG;
    }

    return ret;
}

/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessDDROrder(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    UNREFERENCED_PARAMETER(dRow);

    HRESULT         ret = S_OK;
    PCHAR           paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if (DDR_ORDER_UNSET != cfg->DDR_Proximity)
    { // string already set 
        ret = E_FAIL;
    }
    else if ((nullptr == (paramToken = strtok(argList, TOKEN_DELIMITER))) ||
        (0 != _strnicmp(paramToken, parameterList[dRow].paramStr, strlen(parameterList[dRow].paramStr))) ||
             (nullptr == (paramToken = strtok(NULL, TOKEN_DELIMITER)))
             )
    { // parameter is not what was expeted or there are no tokens
        ret = E_INVALIDARG;
    }
    else if (0 == _strnicmp(paramToken, DDR_ORDER_STRING_ASLISTED, strlen(DDR_ORDER_STRING_ASLISTED)))
    { // Look at the parameter tokens...
        cfg->DDR_Order = DDR_ORDER_ASLISTED;
    }
    else if (0 == _strnicmp(paramToken, DDR_ORDER_STRING_ASCENDING, strlen(DDR_ORDER_STRING_ASCENDING)))
    { // Look at the parameter tokens...
        cfg->DDR_Order = DDR_ORDER_ASCENDING;
    }
    else if (0 == _strnicmp(paramToken, DDR_ORDER_STRING_DESCENDING, strlen(DDR_ORDER_STRING_DESCENDING)))
    { // Look at the parameter tokens...
        cfg->DDR_Order = DDR_ORDER_DESCENDING;
    }
    else if (0 == _strnicmp(paramToken, DDR_ORDER_STRING_RANDOM, strlen(DDR_ORDER_STRING_RANDOM)))
    { // Look at the parameter tokens...
        cfg->DDR_Order = DDR_ORDER_RANDOM;
    }
    else
    { // fail as argument erro if none of the above strings match
        ret = E_INVALIDARG;
    }

    return ret;
}

/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessNumCores(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    UNREFERENCED_PARAMETER(dRow);

    HRESULT         ret = S_OK;
    PCHAR           paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if (INVALID_UINT32 != cfg->CoreCount)
    { // Already set
        ret = E_FAIL;
    }
    else if ( (nullptr != strtok(argList, TOKEN_DELIMITER)) &&
              (nullptr == (paramToken = strtok(NULL, TOKEN_DELIMITER)))
            )
    { // fail - switch should have a modifier
        ret = E_INVALIDARG;
    }
    else
    {
        UINT tokenBase = (0 == _strnicmp(paramToken, HEX_PREFIX, strlen(HEX_PREFIX))) ? TOKEN_BASE_HEX : TOKEN_BASE_DECIMAL;

        cfg->CoreCount = strtoul(paramToken, NULL, tokenBase);
    }

    return ret;
}


/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessNoPayload(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    UNREFERENCED_PARAMETER(dRow);

    HRESULT         ret = S_OK;
    PCHAR           paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if (FALSE == cfg->writePayload)
    { // Already set
        ret = E_FAIL;
    }
    else if ( (nullptr == strtok(argList, TOKEN_DELIMITER)) ||
              (nullptr != (paramToken = strtok(NULL, TOKEN_DELIMITER)))
            )
    { // fail - switch should not have a modifier
        ret = E_INVALIDARG;
    }
    else
    {
        cfg->writePayload = FALSE;
    }

    return ret;
}


/****************************************************************************************************
** Description:
**
** Arguments :
**
** Return :
**  S_OK
**  E_POINTER - invalid argument pointers passed
**
*****************************************************************************************************/
HRESULT ProcessNoAPReg(_Inout_ PDUMP_CONFIG cfg, _In_ UINT32 dRow, _In_ PCHAR argList)
{
    UNREFERENCED_PARAMETER(dRow);

    HRESULT         ret = S_OK;
    PCHAR           paramToken = nullptr;

    if ((nullptr == cfg) || (nullptr == argList))
    { // fail if pointers are null
        ret = E_POINTER;
    }
    else if (TRUE == cfg->excludeApReg)
    { // Already set
        ret = E_FAIL;
    }
    else if ((nullptr == strtok(argList, TOKEN_DELIMITER)) ||
        (nullptr != (paramToken = strtok(NULL, TOKEN_DELIMITER)))
             )
    { // fail - switch should not have a modifier
        ret = E_INVALIDARG;
    }
    else
    {
        cfg->excludeApReg = TRUE;
    }

    return ret;
}


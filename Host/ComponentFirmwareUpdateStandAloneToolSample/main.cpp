/*++
    MIT License
    
    Copyright (C) Microsoft Corporation. All rights reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE


Module Name:

    Main.cpp

Abstract:
    
    This application implements the Component Firmware Update (CFU) 
    protocol for updating a device's firmware image.

Environment:

    User mode.

--*/

#include <stdio.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "tchar.h"
#include <clocale>
#include <time.h>
#include "stdlib.h"
#include "atlbase.h"
#include "Winver.h"
#include <windows.h>
#include <winternl.h>
#include <combaseapi.h>

#include "HidCommands.h"
#include "FwUpdate.h"

using namespace std;

void Usage();

_Check_return_
HRESULT FwUpdateMain(
    __in const int argc,                // Number of strings in array argv
    __in TCHAR* argv[]                  // Array of command-line argument strings
    );   

_Check_return_
HRESULT FwUpdateVersionRequest(
    __in const int argc,                // Number of strings in array argv
    __in TCHAR* argv[]                  // Array of command-line argument strings
);

_Check_return_
bool ReadProtocolSettingsFile(
    _In_ const wstring& settingsPath,
    _Out_ FwUpdateCfu::CfuHidDeviceConfiguration& protocolSettings
);

_Check_return_
const wchar_t* 
DeviceSelect(
    vector<FwUpdateCfu::PathAndVersion>& vectorInterfaces); //Collection of available matching HID devices on the system.

LONG _cdecl _tmain(
    int argc,
    TCHAR* argv[])
/*++

Routine Description:

    Creates an instance of the CFU object if it does not exits.

Arguments:
    
    argc -- Number of arguments
    argv -- Array of input parameters.

Return Value:

  Main return code, or NULL on failure.

--*/
{
    printf("Argc = %d\n", argc);
    LONG ret = 0;
    for (int i = 1; i < argc; i++)
    {
        wprintf(L"Argv #%d is: %ws\n", i, argv[i]);
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Error in CoInitializeEx 0x%x", hr);
        ret = false;
        goto Exit;
    }

    if (argc == 1)  //No arguments
    {
        Usage();
        ret = false;
        goto Exit;
    }

    if (_wcsicmp(argv[1], L"update") == 0)
    {
        ret = FwUpdateMain(argc, argv);
    }
    else if (_wcsicmp(argv[1], L"version") == 0)
    {
        ret = FwUpdateVersionRequest(argc, argv);
    }
    else
    {
        printf("Failed to parse input tokens. ");
    }

Exit:
    
    if (SUCCEEDED(hr))
    {
        CoUninitialize();
    }

    return ret;
}//_tmain()

void Usage()
/*++

Routine Description:

    Displays usage for this tool.

Arguments:
    

Return Value:

--*/
{
    printf("\n");
    printf("Usage:\n");
    printf(">.exe update <protocolSettingsPath> <path to offer file> <path to srec.bin file> <forceIgnoreVersion>(optional) <forceReset>(optional)\n");
    printf(">.exe version <protocolSettingsPath> (to retrieve version of device)\n");
    printf("\t<VID> / <PID> = 0x045e or 045e\n");
    printf("\n");
}//Usage()

_Check_return_
HRESULT 
FwUpdateVersionRequest(
    __in const int argc,
    __in TCHAR* argv[]
)
/*++

Routine Description:

    Get the version of the firmware.

Arguments:
    
    argc -- Number of command line arguments.
    argv -- Command line arguments.

Return Value:

    S_OK on success or underlying failure code.
--*/
{
    HRESULT hr = S_OK;
    FwUpdateCfu::VersionFormat version = { 0 };
    vector<FwUpdateCfu::PathAndVersion> deviceInterfaces;
    FwUpdateCfu::CfuHidDeviceConfiguration protocolSettings = { 0 };

    if (argc < 3)
    {
        printf("Error, too few parameters.\n");
        Usage();
        hr = E_INVALIDARG;
        goto Exit;
    }
    else if (argc > 3)
    {
        printf("Error, too many parameters.\n");
        Usage();
        hr = E_INVALIDARG;
        goto Exit;
    }


    if (!ReadProtocolSettingsFile(argv[2], protocolSettings))
    {
        hr = E_FAIL;
        goto Exit;
    }
    
    FwUpdateCfu* cfu = FwUpdateCfu::GetInstance();
    if (cfu)
    {
        hr = cfu->RetrieveDevicesWithVersions(deviceInterfaces, protocolSettings);
        if (FAILED(hr))
        {
            printf("Error Device not found or not working\n");
            goto Exit;
        }
    }

Exit: 

    return hr;
}// FwUpdateVersionRequest()

_Check_return_
HRESULT 
FwUpdateMain(
    __in const int argc,
    __in TCHAR* argv[]) 
/*++

Routine Description:

    Called from main.

Arguments:
    
    argc -- Number of command line arguments.
    argv -- Command line arguments.

Return Value:

    S_OK on success or underlying failure code.
--*/
{
    HRESULT hr = S_OK;
    FwUpdateCfu::CfuHidDeviceConfiguration protocolSettings = { 0 };
    FwUpdateCfu::VersionFormat version = { 0 };
    const wchar_t* pInterface = nullptr;
    vector<FwUpdateCfu::PathAndVersion> deviceInterfaces;
    FwUpdateCfu* cfu = nullptr;
    wstring offerPath;
    wstring srecBinPath;
    uint8_t forceIgnoreVersion = FALSE;
    uint8_t forceReset = FALSE;

    if (argc < 5)
    {
        printf("Error, too few parameters.\n");
        Usage();
        hr = E_INVALIDARG;
        goto Exit;
    }

    //update <protocolSettingsPath> <path to offer file> <path to srec.bin file> <forceIgnoreVersion> <forceReset>\n");


    offerPath = argv[3];
    srecBinPath = argv[4];


    //Walk through list of arguments past the mandatory ones and see if they match our options
    int optionalArgsToParse = argc - 5;
    for(int i = 0; i < optionalArgsToParse; i++)
    {
        if (_wcsicmp(argv[i+5], L"forceIgnoreVersion") == 0)
        {
            forceIgnoreVersion = TRUE;
        }
        else if (_wcsicmp(argv[i+5], L"forceReset") == 0)
        {
            forceReset = TRUE;
        }
    }

    if (!ReadProtocolSettingsFile(argv[2], protocolSettings))
    {
        hr = E_FAIL;
        goto Exit;
    }

    cfu = FwUpdateCfu::GetInstance();
    if (cfu)
    {
        hr = cfu->RetrieveDevicesWithVersions(deviceInterfaces, protocolSettings);
    }
    else
    {
        hr = E_OUTOFMEMORY;
        goto Exit;
    }

    pInterface = DeviceSelect(deviceInterfaces);
    version = deviceInterfaces[0].version.version;
    wprintf(L"Processing offer against %s\n", pInterface);

    bool returnVal = false;

    //Prevent Windows from sleeping in the middle of the update
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);

    LARGE_INTEGER freq;
    LARGE_INTEGER startFWTime;
    LARGE_INTEGER stopFWTime;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&startFWTime);

    returnVal = cfu->FwUpdateOfferSrec(protocolSettings, 
                                       offerPath.c_str(), 
                                       srecBinPath.c_str(), 
                                       pInterface, 
                                       forceIgnoreVersion, 
                                       forceReset);
    if (returnVal)
    {
        QueryPerformanceCounter(&stopFWTime);
        wprintf(L"FW Update Completed Successfully in %f seconds!\n", (stopFWTime.QuadPart - startFWTime.QuadPart) / 1.0 / freq.QuadPart);
        hr = S_OK;
    }
    else
    {
        wprintf(L"FW Update not performed on offer %s\n", offerPath.c_str());
        hr = E_FAIL;
    }

    //allows Windows to do sleep/hibernate again
    SetThreadExecutionState(ES_CONTINUOUS);

Exit:

    return hr;
}//FwUpdateMain() 

_Check_return_
const wchar_t* 
DeviceSelect(vector<FwUpdateCfu::PathAndVersion>& vectorInterfaces)
/*++

Routine Description:

    Selects a device from a list of interfaces.

Arguments:
    
    vectorInterfaces -- List 0f device interfaces found.

Return Value:

    Pointer to the device path or nullptr on failure.
--*/
{
    if (vectorInterfaces.size() == 0)
    {
        printf("No devices found to select from.\n");
        return nullptr;
    }
    else if (vectorInterfaces.size() == 1)
    {
        printf("Only one device found, auto-selecting.\n");
        return vectorInterfaces[0].devicePath.c_str();
    }
    else
    {
        int selection = -1;
        while (selection == -1)
        {
            printf("Enter device selection: ");
            cin >> selection;   //Using cin as workaround for TShell issues with getchar()

            if (selection > vectorInterfaces.size() - 1 || selection < 0)
            {
                cout << "\nSelected device doesn't exist (must be between 0 and " << vectorInterfaces.size() - 1 << ")!!!\n";
                selection = -1;
            }
        }

        return vectorInterfaces[selection].devicePath.c_str();
    }
}//DeviceSelect()

vector<string> 
Split(_In_ const string& InputString, _In_ char Delimiter)
/*++

Routine Description:

    Splits a string based on the specified delimiter.

Arguments:
    
    InputString -- Input string
    Delimiter   -- Delimiter used to split the string.

Return Value:

    List of strings.

--*/
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(InputString);
    while (getline(tokenStream, token, Delimiter))
    {
        tokens.push_back(token);
    }

    return tokens;
}//Split()

_Check_return_
bool 
ReadProtocolSettingsFile(
    _In_ const wstring& SettingsPath,
    _Out_ FwUpdateCfu::CfuHidDeviceConfiguration& ProtocolSettings)
/*++

Routine Description:

    Reads the settings file.

Arguments:
    
    SettingsPath     -- Path to the input settings file.
    ProtocolSettings -- Settings to use for this device.

Return Value:

    List of strings.

--*/
{
    memset(&ProtocolSettings, 0, sizeof(ProtocolSettings));

    ifstream configStream(SettingsPath);
    if (!configStream)
    {
        wprintf(L"Failed to open settings file \"%s\"\n", SettingsPath.c_str());
        return false;
    }

    char line[512];
    while (configStream.getline(line, 512))
    {
        string lines = line;
        vector<string> tokens = Split(lines, ',');

        if (tokens.size() < 2)
        {
            continue;
        }

        const char* tag = tokens[0].c_str();
        const char* value = tokens[1].c_str();
        if (_stricmp(tag, "VID") == 0)
        {
            ProtocolSettings.Vid = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "PID") == 0)
        {
            ProtocolSettings.Pid = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "USAGEPAGE") == 0)
        {
            ProtocolSettings.UsagePage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "USAGECOLLECTION") == 0)
        {
            ProtocolSettings.UsageTlc = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "VERSION_FEATURE_USAGE") == 0)
        {
            ProtocolSettings.Reports[FwUpdateCfu::FwUpdateVersion].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            ProtocolSettings.Reports[FwUpdateCfu::FwUpdateVersion].inOutFeature = HidP_Feature;
            ProtocolSettings.Reports[FwUpdateCfu::FwUpdateVersion].size = 60; //bytes
        }
        else if (_stricmp(tag, "CONTENT_OUTPUT_USAGE") == 0)
        {
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateContent].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateContent].inOutFeature = HidP_Output;
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateContent].size = 60; //bytes
        }
        else if (_stricmp(tag, "CONTENT_RESPONSE_INPUT_USAGE") == 0)
        {
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateContentResponse].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateContentResponse].inOutFeature = HidP_Input;
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateContentResponse].size = 16; //bytes
        }
        else if (_stricmp(tag, "OFFER_OUTPUT_USAGE") == 0)
        {
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateOffer].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateOffer].inOutFeature = HidP_Output;
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateOffer].size = 16; //bytes
        }
        else if (_stricmp(tag, "OFFER_RESPONSE_INPUT_USAGE") == 0)
        {
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateOfferResponse].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateOfferResponse].inOutFeature = HidP_Input;
            ProtocolSettings.Reports[FwUpdateCfu::FWUpdateOfferResponse].size = 16; //bytes
        }
    }
    
    return true;
}//ReadProtocolSettingsFile()

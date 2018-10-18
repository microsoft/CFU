//Copyright(c) Microsoft Corporation.All rights reserved.
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE

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

HRESULT FwUpdateMain(
    __in const int argc,                // Number of strings in array argv
    __in TCHAR* argv[]                  // Array of command-line argument strings
    );   

HRESULT FwUpdateVersionRequest(
    __in const int argc,                // Number of strings in array argv
    __in TCHAR* argv[]                  // Array of command-line argument strings
);

bool ReadProtocolSettingsFile(
    _In_ const wstring& settingsPath,
    _Out_ FwUpdateCfu::CfuHidDeviceConfiguration& protocolSettings
);

const wchar_t* DeviceSelect(
    vector<FwUpdateCfu::PathAndVersion>& vectorInterfaces); //Collection of available HID devices on the system.

LONG _cdecl _tmain(
    int argc,                           // Number of strings in array argv
    TCHAR* argv[])                      // Array of command-line argument strings

{
    printf("Argc = %d\n", argc);
    for (int i = 1; i < argc; i++)
    {
        wprintf(L"Argv #%d is: %ws\n", i, argv[i]);
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        wprintf(L"Error in CoInitializeEx 0x%x", hr);
    }

    if (argc == 1)  //No args
    {
        Usage();
        return 0;
    }

    if (_wcsicmp(argv[1], L"update") == 0)
    {
        return FwUpdateMain(argc, argv);
    }
    else if (_wcsicmp(argv[1], L"version") == 0)
    {
        return FwUpdateVersionRequest(argc, argv);
    }
    else
    {
        printf("Failed to parse input tokens. ");
    }

    return 0;
}

void Usage()
{
    printf("\n");
    printf("Usage:\n");
    printf(">.exe update <protocolSettingsPath> <path to offer file> <path to srec.bin file> <forceIgnoreVersion>(optional) <forceReset>(optional)\n");
    printf(">.exe version <protocolSettingsPath> (to retrieve version of device)\n");
    printf("\t<VID> / <PID> = 0x045e or 045e\n");
    printf("\n");
}

HRESULT FwUpdateVersionRequest(
    __in const int argc,                // Number of strings in array argv
    __in TCHAR* argv[]                  // Array of command-line argument strings
)
{
    HRESULT hr = S_OK;

    if (argc < 3)
    {
        printf("Error, too few parameters.\n");
        Usage();
        return E_FAIL;
    }
    else if (argc > 3)
    {
        printf("Error, too many parameters.\n");
        Usage();
        return E_FAIL;
    }

    FwUpdateCfu::VersionFormat version = { 0 };
    vector<FwUpdateCfu::PathAndVersion> deviceInterfaces;

    FwUpdateCfu::CfuHidDeviceConfiguration protocolSettings = { 0 };

    if (!ReadProtocolSettingsFile(argv[2], protocolSettings))
    {
        return E_FAIL;
    }

    if (!FwUpdateCfu::GetInstance()->RetrieveDevicesWithVersions(deviceInterfaces, protocolSettings))
    {
        printf("Error Device not found or not working\n");
        return E_FAIL;
    }

    return hr;
}

HRESULT FwUpdateMain(
    __in const int argc,       // Number of strings in array argv  
    __in TCHAR* argv[])        // Array of command-line argument strings
{
    HRESULT hr = S_OK;

    if (argc < 5)
    {
        printf("Error, too few parameters.\n");
        Usage();
        return -1;
    }

    //update <protocolSettingsPath> <path to offer file> <path to srec.bin file> <forceIgnoreVersion> <forceReset>\n");

    FwUpdateCfu::VersionFormat version = { 0 };
    const wchar_t* pInterface = nullptr;
    vector<FwUpdateCfu::PathAndVersion> deviceInterfaces;

    wstring offerPath = argv[3];
    wstring srecBinPath = argv[4];

    uint8_t forceIgnoreVersion = FALSE;
    uint8_t forceReset = FALSE;

    //Walk through list of args past the mandatory ones and see if they match our options
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

    FwUpdateCfu::CfuHidDeviceConfiguration protocolSettings = { 0 };
    if (!ReadProtocolSettingsFile(argv[2], protocolSettings))
    {
        return E_FAIL;
    }

    if (!FwUpdateCfu::GetInstance()->RetrieveDevicesWithVersions(deviceInterfaces, protocolSettings))
    {
        printf("Error Device not found or not working\n");
        return E_FAIL;
    }
    pInterface = DeviceSelect(deviceInterfaces);
    version = deviceInterfaces[0].version.version;
    wprintf(L"Processing offer against %s\n", pInterface);

    bool returnVal = false;

    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);

    LARGE_INTEGER freq;
    LARGE_INTEGER startFWTime;
    LARGE_INTEGER stopFWTime;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&startFWTime);

    returnVal = FwUpdateCfu::GetInstance()->FwUpdateOfferSrec(protocolSettings, offerPath.c_str(), srecBinPath.c_str(), pInterface, forceIgnoreVersion, forceReset);
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
    SetThreadExecutionState(ES_CONTINUOUS);
  

    return hr;
}

const wchar_t* DeviceSelect(vector<FwUpdateCfu::PathAndVersion>& vectorInterfaces)
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
}

vector<string> split(const string& s, char delimiter)
{
    vector<string> tokens;
    string token;
    istringstream tokenStream(s);
    while (getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

bool ReadProtocolSettingsFile(
    _In_ const wstring& settingsPath,
    _Out_ FwUpdateCfu::CfuHidDeviceConfiguration& protocolSettings)
{
    memset(&protocolSettings, 0, sizeof(protocolSettings));

    ifstream configStream(settingsPath);
    if (!configStream)
    {
        wprintf(L"Failed to open settings file \"%s\"\n", settingsPath.c_str());
        return false;
    }

    char line[512];
    while (configStream.getline(line, 512))
    {
        string lines = line;
        vector<string> tokens = split(lines, ',');

        if (tokens.size() < 2)
        {
            continue;
        }

        const char* tag = tokens[0].c_str();
        const char* value = tokens[1].c_str();
        if (_stricmp(tag, "VID") == 0)
        {
            protocolSettings.Vid = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "PID") == 0)
        {
            protocolSettings.Pid = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "USAGEPAGE") == 0)
        {
            protocolSettings.UsagePage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "USAGECOLLECTION") == 0)
        {
            protocolSettings.UsageTlc = static_cast<uint16_t>(strtoul(value, nullptr, 16));
        }
        else if (_stricmp(tag, "VERSION_FEATURE_USAGE") == 0)
        {
            protocolSettings.Reports[FwUpdateCfu::FwUpdateVersion].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            protocolSettings.Reports[FwUpdateCfu::FwUpdateVersion].inOutFeature = HidP_Feature;
            protocolSettings.Reports[FwUpdateCfu::FwUpdateVersion].size = 60; //bytes
        }
        else if (_stricmp(tag, "CONTENT_OUTPUT_USAGE") == 0)
        {
            protocolSettings.Reports[FwUpdateCfu::FWUpdateContent].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            protocolSettings.Reports[FwUpdateCfu::FWUpdateContent].inOutFeature = HidP_Output;
            protocolSettings.Reports[FwUpdateCfu::FWUpdateContent].size = 60; //bytes
        }
        else if (_stricmp(tag, "CONTENT_RESPONSE_INPUT_USAGE") == 0)
        {
            protocolSettings.Reports[FwUpdateCfu::FWUpdateContentResponse].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            protocolSettings.Reports[FwUpdateCfu::FWUpdateContentResponse].inOutFeature = HidP_Input;
            protocolSettings.Reports[FwUpdateCfu::FWUpdateContentResponse].size = 16; //bytes
        }
        else if (_stricmp(tag, "OFFER_OUTPUT_USAGE") == 0)
        {
            protocolSettings.Reports[FwUpdateCfu::FWUpdateOffer].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            protocolSettings.Reports[FwUpdateCfu::FWUpdateOffer].inOutFeature = HidP_Output;
            protocolSettings.Reports[FwUpdateCfu::FWUpdateOffer].size = 16; //bytes
        }
        else if (_stricmp(tag, "OFFER_RESPONSE_INPUT_USAGE") == 0)
        {
            protocolSettings.Reports[FwUpdateCfu::FWUpdateOfferResponse].Usage = static_cast<uint16_t>(strtoul(value, nullptr, 16));
            protocolSettings.Reports[FwUpdateCfu::FWUpdateOfferResponse].inOutFeature = HidP_Input;
            protocolSettings.Reports[FwUpdateCfu::FWUpdateOfferResponse].size = 16; //bytes
        }
    }
    
    return true;
}

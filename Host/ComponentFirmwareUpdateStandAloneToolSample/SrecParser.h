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

#pragma once

using namespace std;

static bool ProcessSrecBin(ifstream& srecBinStream, FwUpdateCfu::ContentData& contentData)
{
    contentData.length = 0;

    char pBuff[64] = { 0 };

    if (srecBinStream.eof())
    {
        wprintf(L"Stream reached the end of file\n");
        return false;
    }

    //read the address offset from the binary file
    srecBinStream.read(pBuff, sizeof(contentData.address));
    contentData.address = *reinterpret_cast<uint32_t*>(pBuff);

    //read the byte length from the binary file
    srecBinStream.read(pBuff, sizeof(contentData.length));
    contentData.length = *reinterpret_cast<uint8_t*>(pBuff);

    if (contentData.length == 0)
    {
        //printf("length 0\n");
        return false;
    }

    //read the content block using the length we just collected
    srecBinStream.read(pBuff, contentData.length);

    //copy the content data into out var
    memcpy_s(contentData.data, sizeof(contentData.data), pBuff, contentData.length);

    return true;
}

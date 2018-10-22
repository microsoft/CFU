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

    FwUpdateCfu.h

Abstract:

    String utilities and safe delete utilities

Environment:

    User mode.

--*/
#pragma once

#include <string>
#include <algorithm>
#include <tchar.h>

// macros for deleting buffers, etc..
#ifndef SAFE_DELETE
#define SAFE_DELETE(x) do { if ((x) != NULL) { delete (x); (x) = NULL; }} while(0);
#endif

#ifndef SAFE_DELETEA
#define SAFE_DELETEA(x) do { if ((x) != NULL) { delete [] (x); (x) = NULL; }} while(0);
#endif

typedef struct 
{
     static BOOL comparewsi(std::wstring& a, std::wstring& b) {
         std::transform(a.begin(), a.end(), a.begin(), toupper);
         std::transform(b.begin(), b.end(), b.begin(), toupper);
         return (a == b);
     }

     static BOOL comparewsi(TCHAR* pa, std::wstring& b) {
         std::wstring a(pa);
         std::transform(a.begin(), a.end(), a.begin(), toupper);
         std::transform(b.begin(), b.end(), b.begin(), toupper);
         return (a == b);
     }

     static BOOL comparewsi(TCHAR* pa, const wchar_t* pb) {
         std::wstring a(pa);
         std::wstring b(pb);
         std::transform(a.begin(), a.end(), a.begin(), toupper);
         std::transform(b.begin(), b.end(), b.begin(), toupper);
         return (a == b);
     }

     static BOOL comparecsi(const char* pa, const char* pb) {
         std::string a(pa);
         std::string b(pb);
         std::transform(a.begin(), a.end(), a.begin(), toupper);
         std::transform(b.begin(), b.end(), b.begin(), toupper);
         return (a == b);
     }
} StringUtil;

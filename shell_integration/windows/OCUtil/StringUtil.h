/**
* Copyright (c) 2014 ownCloud GmbH. All rights reserved.
*
* This library is free software; you can redistribute it and/or modify it under
* the terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; version 2.1 of the License
*
* This library is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
* details.
*/

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

#pragma once

#include <string>
#include <cassert>

class __declspec(dllexport) StringUtil {
public:
    static std::string  toUtf8(const wchar_t* utf16, int len = -1);
    static std::wstring toUtf16(const char* utf8, int len = -1);

    template<class T>
    static bool begins_with(const T& input, const T& match)
    {
        return input.size() >= match.size()
            && std::equal(match.begin(), match.end(), input.begin());
    }

    static bool isDescendantOf(const std::wstring& child, const std::wstring& parent) {
        return isDescendantOf(child.c_str(), child.size(), parent.c_str(), parent.size());
    }

    static bool isDescendantOf(PCWSTR child, size_t childLength, const std::wstring& parent) {
        return isDescendantOf(child, childLength, parent.c_str(), parent.size());
    }

    static bool isDescendantOf(PCWSTR child, size_t childLength, PCWSTR parent, size_t parentLength) {
        if (!parentLength)
            return false;
        return (childLength == parentLength || childLength > parentLength && (child[parentLength] == L'\\' || child[parentLength - 1] == L'\\'))
            && wcsncmp(child, parent, parentLength) == 0;
    }

    static bool extractChunks(const std::wstring &source, std::wstring &secondChunk, std::wstring &thirdChunk) {
        auto statusBegin = source.find(L':', 0);
        assert(statusBegin != std::wstring::npos);

        auto statusEnd = source.find(L':', statusBegin + 1);
        if (statusEnd == std::wstring::npos) {
            // the command do not contains two colon?
            return false;
        }

        // Assume the caller extracted the chunk before the first colon.
        secondChunk = source.substr(statusBegin + 1, statusEnd - statusBegin - 1);
        thirdChunk = source.substr(statusEnd + 1);
        return true;
    }

    static bool extractChunks(const std::wstring &source, std::wstring &secondChunk, std::wstring &thirdChunk, std::wstring &forthChunk)
    {
        auto statusBegin = source.find(L':', 0);
        assert(statusBegin != std::wstring::npos);

        auto statusEnd = source.find(L':', statusBegin + 1);
        if (statusEnd == std::wstring::npos) {
            // the command do not contains two colon?
            return false;
        }

        auto thirdColon = source.find(L':', statusEnd + 1);
        if (statusEnd == std::wstring::npos) {
            // the command do not contains three colon?
            return false;
        }

        // Assume the caller extracted the chunk before the first colon.
        secondChunk = source.substr(statusBegin + 1, statusEnd - statusBegin - 1);
        thirdChunk = source.substr(statusEnd + 1, thirdColon - statusEnd - 1);
        forthChunk = source.substr(thirdColon + 1);
        return true;
    }
};

#endif // STRINGUTIL_H

#include "StringUtility.h"
#include <Windows.h>

namespace StringUtility
{
    // string → wstring
    std::wstring ConvertString(const std::string& str)
    {
        // 文字列長が 0 の場合は空文字を返す
        if (str.empty()) { return std::wstring(); }

        // 必要なバッファサイズを取得
        int sizeNeeded = MultiByteToWideChar(
            CP_UTF8, 0,
            str.c_str(), -1,
            nullptr, 0);

        std::wstring result(sizeNeeded, L'\0');

        MultiByteToWideChar(
            CP_UTF8, 0,
            str.c_str(), -1,
            &result[0], sizeNeeded);

        // 終端の '\0' を削除
        if (!result.empty() && result.back() == L'\0')
        {
            result.pop_back();
        }

        return result;
    }

    // wstring → string
    std::string ConvertString(const std::wstring& str)
    {
        // 文字列長が 0 の場合は空文字を返す
        if (str.empty()) { return std::string(); }

        // 必要なバッファサイズを取得
        int sizeNeeded = WideCharToMultiByte(
            CP_UTF8, 0,
            str.c_str(), -1,
            nullptr, 0,
            nullptr, nullptr);

        std::string result(sizeNeeded, '\0');

        WideCharToMultiByte(
            CP_UTF8, 0,
            str.c_str(), -1,
            &result[0], sizeNeeded,
            nullptr, nullptr);

        // 終端の '\0' を削除
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back();
        }

        return result;
    }
}

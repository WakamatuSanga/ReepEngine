#include <string>

namespace StringUtility
{
    // string を wstring に変換する
    std::wstring ConvertString(const std::string& str);

    // wstring を string に変換する
    std::string ConvertString(const std::wstring& str);
}

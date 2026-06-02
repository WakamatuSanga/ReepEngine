#include "Logger.h"
#include <Windows.h>
#include <iostream>

namespace Logger
{
    void Log(const std::string& message)
    {
        // デバッグ出力
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");

        // コンソールにも出力（必要に応じて）
        std::cout << message << std::endl;
    }
}

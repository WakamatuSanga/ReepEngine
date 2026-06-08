#include "BlenderLauncher.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {
    std::wstring ToWideString(const std::string& text) {
        if (text.empty()) {
            return {};
        }

        const int size = MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0);
        if (size <= 0) {
            return {};
        }

        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            size);
        return result;
    }

    std::string ToUtf8String(const std::wstring& text) {
        if (text.empty()) {
            return {};
        }

        const int size = WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0,
            nullptr,
            nullptr);
        if (size <= 0) {
            return {};
        }

        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            size,
            nullptr,
            nullptr);
        return result;
    }

    std::filesystem::path MakePathFromUtf8(const std::string& path) {
        return std::filesystem::path(ToWideString(path));
    }

    bool PathExists(const std::filesystem::path& path) {
        std::error_code errorCode;
        return std::filesystem::exists(path, errorCode);
    }

    bool IsDirectory(const std::filesystem::path& path) {
        std::error_code errorCode;
        return std::filesystem::is_directory(path, errorCode);
    }

    std::string ToAbsoluteGenericString(const std::filesystem::path& path) {
        return ToUtf8String(std::filesystem::absolute(path).lexically_normal().wstring());
    }

    std::string Quote(const std::string& path) {
        return "\"" + path + "\"";
    }

    bool HasPathSeparator(const std::string& path) {
        return path.find('/') != std::string::npos || path.find('\\') != std::string::npos;
    }

    std::string TrimMessage(std::string message) {
        while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
            message.pop_back();
        }
        return message;
    }

    std::string FormatWin32ErrorMessage(uint32_t errorCode) {
        if (errorCode == 0) {
            return {};
        }

        wchar_t* buffer = nullptr;
        const DWORD length = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(errorCode),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPWSTR>(&buffer),
            0,
            nullptr);

        if (length == 0 || buffer == nullptr) {
            return "FormatMessageW failed.";
        }

        const std::string message = TrimMessage(ToUtf8String(std::wstring(buffer, length)));
        LocalFree(buffer);
        return message;
    }

    std::string ResolveProjectFilePath(const std::string& path) {
        if (path.empty()) {
            return {};
        }

        const std::filesystem::path requestedPath = MakePathFromUtf8(path);
        if (PathExists(requestedPath)) {
            return ToAbsoluteGenericString(requestedPath);
        }

        if (requestedPath.is_absolute()) {
            return {};
        }

        const std::array<std::filesystem::path, 5> basePaths = {
            std::filesystem::path{ "project" },
            std::filesystem::path{ ".." } / "project",
            std::filesystem::path{ ".." } / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / ".." / "project",
        };

        for (const std::filesystem::path& basePath : basePaths) {
            const std::filesystem::path candidate = basePath / requestedPath;
            if (PathExists(candidate)) {
                return ToAbsoluteGenericString(candidate);
            }
        }

        return {};
    }

    std::string ResolveExecutablePath(const std::string& path) {
        if (path.empty()) {
            return {};
        }

        const std::filesystem::path requestedPath = MakePathFromUtf8(path);
        if (requestedPath.is_absolute() || HasPathSeparator(path)) {
            if (PathExists(requestedPath)) {
                return ToAbsoluteGenericString(requestedPath);
            }

            if (!requestedPath.is_absolute()) {
                return ResolveProjectFilePath(path);
            }
            return {};
        }

        const std::wstring widePath = ToWideString(path);
        std::vector<wchar_t> buffer(32768);
        const DWORD length = SearchPathW(
            nullptr,
            widePath.c_str(),
            nullptr,
            static_cast<DWORD>(buffer.size()),
            buffer.data(),
            nullptr);
        if (length > 0 && length < buffer.size()) {
            return ToUtf8String(std::wstring(buffer.data(), length));
        }

        return {};
    }

    void AppendBlenderCandidate(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& path) {
        if (PathExists(path)) {
            candidates.push_back(path);
        }
    }

    std::string FindBlenderExecutableCandidate() {
        std::vector<std::filesystem::path> candidates;
        const std::array<std::filesystem::path, 2> roots = {
            std::filesystem::path{ L"C:\\Program Files\\Blender Foundation" },
            std::filesystem::path{ L"C:\\Program Files (x86)\\Blender Foundation" },
        };

        for (const std::filesystem::path& root : roots) {
            if (!IsDirectory(root)) {
                continue;
            }

            AppendBlenderCandidate(candidates, root / L"blender.exe");

            std::error_code errorCode;
            for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root, errorCode)) {
                if (errorCode) {
                    break;
                }
                if (entry.is_directory(errorCode)) {
                    AppendBlenderCandidate(candidates, entry.path() / L"blender.exe");
                }
            }
        }

        std::sort(candidates.begin(), candidates.end(), [](const std::filesystem::path& lhs, const std::filesystem::path& rhs) {
            return lhs.wstring() < rhs.wstring();
        });
        if (!candidates.empty()) {
            return ToAbsoluteGenericString(candidates.back());
        }

        return ResolveExecutablePath("blender.exe");
    }

    bool IsProjectDirectory(const std::filesystem::path& path) {
        return PathExists(path / L"DirectXGame.vcxproj") && IsDirectory(path / L"resources");
    }

    std::string ResolveProjectDirectory(const std::string& resolvedBlendPath) {
        const std::array<std::filesystem::path, 6> candidates = {
            std::filesystem::path{ "." },
            std::filesystem::path{ "project" },
            std::filesystem::path{ ".." } / "project",
            std::filesystem::path{ ".." } / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / "project",
            std::filesystem::path{ ".." } / ".." / ".." / ".." / "project",
        };

        for (const std::filesystem::path& candidate : candidates) {
            if (IsProjectDirectory(candidate)) {
                return ToAbsoluteGenericString(candidate);
            }
        }

        if (!resolvedBlendPath.empty()) {
            std::filesystem::path parentPath = MakePathFromUtf8(resolvedBlendPath).parent_path();
            while (!parentPath.empty()) {
                if (IsProjectDirectory(parentPath)) {
                    return ToAbsoluteGenericString(parentPath);
                }
                const std::filesystem::path nextParent = parentPath.parent_path();
                if (nextParent == parentPath) {
                    break;
                }
                parentPath = nextParent;
            }
        }

        return {};
    }

    bool RequiresBlendFile(BlenderLaunchMode mode) {
        return mode == BlenderLaunchMode::BlendOnly || mode == BlenderLaunchMode::BlendWithStartupScript;
    }

    bool RequiresStartupScript(BlenderLaunchMode mode) {
        return mode == BlenderLaunchMode::BlendWithStartupScript;
    }

    std::string BuildCommandLine(const BlenderLaunchDiagnostics& diagnostics, BlenderLaunchMode mode) {
        const std::string exePath = diagnostics.resolvedBlenderExePath.empty()
            ? diagnostics.requestedBlenderExePath
            : diagnostics.resolvedBlenderExePath;
        if (exePath.empty()) {
            return {};
        }

        std::string commandLine = Quote(exePath);
        if (RequiresBlendFile(mode)) {
            const std::string blendPath = diagnostics.resolvedBlendFilePath.empty()
                ? diagnostics.requestedBlendFilePath
                : diagnostics.resolvedBlendFilePath;
            if (!blendPath.empty()) {
                commandLine += " " + Quote(blendPath);
            }
        }
        if (RequiresStartupScript(mode)) {
            const std::string scriptPath = diagnostics.resolvedStartupScriptPath.empty()
                ? diagnostics.requestedStartupScriptPath
                : diagnostics.resolvedStartupScriptPath;
            if (!scriptPath.empty()) {
                commandLine += " --python " + Quote(scriptPath);
            }
        }

        return commandLine;
    }

    void* ToHandle(uintptr_t handle) {
        return reinterpret_cast<void*>(handle);
    }
}

BlenderLauncher::BlenderLauncher() {
    const std::string candidatePath = FindBlenderExecutableCandidate();
    if (!candidatePath.empty()) {
        blenderExePath_ = candidatePath;
    } else {
        lastError_ =
            "blender.exe was not found under C:/Program Files/Blender Foundation or PATH. Set Blender Exe Path manually.";
    }

    RefreshDiagnostics();
}

BlenderLauncher::~BlenderLauncher() {
    CloseProcessHandle();
}

bool BlenderLauncher::Launch(BlenderLaunchMode mode) {
    lastError_.clear();
    RefreshDiagnostics(mode);
    lastCommandLine_ = diagnostics_.finalCommandLine;

    if (IsRunning()) {
        return true;
    }

    if (!diagnostics_.blenderExeExists) {
        lastError_ = "Blender exe not found: " + blenderExePath_;
        return false;
    }
    if (RequiresBlendFile(mode) && !diagnostics_.blendFileExists) {
        lastError_ = "Blend file not found: " + blendFilePath_;
        return false;
    }
    if (RequiresStartupScript(mode) && !diagnostics_.startupScriptExists) {
        lastError_ = "Startup script not found: " + startupScriptPath_;
        return false;
    }
    if (diagnostics_.workingDirectory.empty()) {
        lastError_ = "Working directory could not be resolved.";
        return false;
    }

    std::wstring wideExePath = ToWideString(diagnostics_.resolvedBlenderExePath);
    std::wstring wideCommandLine = ToWideString(diagnostics_.finalCommandLine);
    std::wstring wideWorkingDirectory = ToWideString(diagnostics_.workingDirectory);
    std::vector<wchar_t> commandLineBuffer(wideCommandLine.begin(), wideCommandLine.end());
    commandLineBuffer.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInformation{};
    const BOOL result = CreateProcessW(
        wideExePath.c_str(),
        commandLineBuffer.data(),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        wideWorkingDirectory.empty() ? nullptr : wideWorkingDirectory.c_str(),
        &startupInfo,
        &processInformation);

    diagnostics_.createProcessSucceeded = result != FALSE;
    diagnostics_.win32ErrorCode = result ? 0u : static_cast<uint32_t>(::GetLastError());
    diagnostics_.win32ErrorMessage = FormatWin32ErrorMessage(diagnostics_.win32ErrorCode);

    if (!result) {
        lastError_ =
            "CreateProcessW failed. code=" + std::to_string(diagnostics_.win32ErrorCode) +
            " message=" + diagnostics_.win32ErrorMessage;
        return false;
    }

    CloseHandle(processInformation.hThread);
    processHandle_ = reinterpret_cast<uintptr_t>(processInformation.hProcess);
    return true;
}

void BlenderLauncher::RefreshDiagnostics(BlenderLaunchMode mode) {
    const uint32_t previousErrorCode = diagnostics_.win32ErrorCode;
    const std::string previousErrorMessage = diagnostics_.win32ErrorMessage;
    const bool previousCreateProcessSucceeded = diagnostics_.createProcessSucceeded;

    diagnostics_ = BlenderLaunchDiagnostics{};
    diagnostics_.launchMode = mode;
    diagnostics_.requestedBlenderExePath = blenderExePath_;
    diagnostics_.requestedBlendFilePath = blendFilePath_;
    diagnostics_.requestedStartupScriptPath = startupScriptPath_;
    diagnostics_.resolvedBlenderExePath = ResolveExecutablePath(blenderExePath_);
    diagnostics_.resolvedBlendFilePath = ResolveProjectFilePath(blendFilePath_);
    diagnostics_.resolvedStartupScriptPath = ResolveProjectFilePath(startupScriptPath_);
    diagnostics_.blenderExeExists = !diagnostics_.resolvedBlenderExePath.empty();
    diagnostics_.blendFileExists = !diagnostics_.resolvedBlendFilePath.empty();
    diagnostics_.startupScriptExists = !diagnostics_.resolvedStartupScriptPath.empty();

    diagnostics_.workingDirectory = ResolveProjectDirectory(diagnostics_.resolvedBlendFilePath);
    if (diagnostics_.workingDirectory.empty() && diagnostics_.blenderExeExists) {
        diagnostics_.workingDirectory =
            ToAbsoluteGenericString(MakePathFromUtf8(diagnostics_.resolvedBlenderExePath).parent_path());
    }

    diagnostics_.finalCommandLine = BuildCommandLine(diagnostics_, mode);
    diagnostics_.createProcessSucceeded = previousCreateProcessSucceeded;
    diagnostics_.win32ErrorCode = previousErrorCode;
    diagnostics_.win32ErrorMessage = previousErrorMessage;
}

bool BlenderLauncher::IsRunning() {
    if (processHandle_ == kInvalidProcessHandle) {
        return false;
    }

    DWORD exitCode = 0;
    if (!GetExitCodeProcess(ToHandle(processHandle_), &exitCode)) {
        CloseProcessHandle();
        return false;
    }
    if (exitCode == STILL_ACTIVE) {
        return true;
    }

    CloseProcessHandle();
    return false;
}

void BlenderLauncher::CloseProcessHandle() {
    if (processHandle_ != kInvalidProcessHandle) {
        CloseHandle(ToHandle(processHandle_));
        processHandle_ = kInvalidProcessHandle;
    }
}

#pragma once
#include <cstdint>
#include <string>

enum class BlenderLaunchMode {
    BlenderOnly,
    BlendOnly,
    BlendWithStartupScript,
};

struct BlenderLaunchDiagnostics {
    std::string requestedBlenderExePath;
    std::string requestedBlendFilePath;
    std::string requestedStartupScriptPath;
    std::string resolvedBlenderExePath;
    std::string resolvedBlendFilePath;
    std::string resolvedStartupScriptPath;
    std::string workingDirectory;
    std::string finalCommandLine;
    std::string win32ErrorMessage;
    bool blenderExeExists = false;
    bool blendFileExists = false;
    bool startupScriptExists = false;
    bool createProcessSucceeded = false;
    uint32_t win32ErrorCode = 0;
    BlenderLaunchMode launchMode = BlenderLaunchMode::BlendWithStartupScript;
};

class BlenderLauncher {
public:
    BlenderLauncher();
    ~BlenderLauncher();

    bool Launch(BlenderLaunchMode mode = BlenderLaunchMode::BlendWithStartupScript);
    void RefreshDiagnostics(BlenderLaunchMode mode = BlenderLaunchMode::BlendWithStartupScript);
    bool IsRunning();
    void CloseProcessHandle();

    void SetBlenderExePath(const std::string& path) { blenderExePath_ = path; }
    void SetBlendFilePath(const std::string& path) { blendFilePath_ = path; }
    void SetStartupScriptPath(const std::string& path) { startupScriptPath_ = path; }

    const std::string& GetBlenderExePath() const { return blenderExePath_; }
    const std::string& GetBlendFilePath() const { return blendFilePath_; }
    const std::string& GetStartupScriptPath() const { return startupScriptPath_; }
    const std::string& GetLastError() const { return lastError_; }
    const std::string& GetLastCommandLine() const { return lastCommandLine_; }
    const BlenderLaunchDiagnostics& GetDiagnostics() const { return diagnostics_; }

private:
    static constexpr uintptr_t kInvalidProcessHandle = 0;

    uintptr_t processHandle_ = kInvalidProcessHandle;
    std::string blenderExePath_ = "blender.exe";
    std::string blendFilePath_ = "resources/level_editor/Stage_editor.blend";
    std::string startupScriptPath_ = "resources/level_editor/level_editor/live_sync_startup.py";
    std::string lastError_;
    std::string lastCommandLine_;
    BlenderLaunchDiagnostics diagnostics_;
};

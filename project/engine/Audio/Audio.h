#pragma once

#include <xaudio2.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl.h>
#include <string>
#include <map>
#include <memory>
#include <unordered_set>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

class Audio {
    friend struct std::default_delete<Audio>;
public:
    struct SoundData {
        WAVEFORMATEX wfex;
        std::unique_ptr<BYTE[]> pBuffer; // unique_ptrで配列を管理
        unsigned int bufferSize;
    };

public:
    static Audio* GetInstance();

    void Initialize();
    void Finalize();

    void LoadAudio(const std::string& filename);
    void PlayAudio(const std::string& filename);

private:
    Audio() = default;
    ~Audio() = default;
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    void UnloadAudio(SoundData* soundData);

private:
    static std::unique_ptr<Audio> instance_;

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;

    std::map<std::string, SoundData> soundDatas_;

    std::unordered_set<IXAudio2SourceVoice*> sourceVoices_;
};

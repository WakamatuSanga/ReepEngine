#include "Audio.h"
#include "Engine/Utility/StringUtility.h"
#include <cassert>
#include <vector>

std::unique_ptr<Audio> Audio::instance_ = nullptr;

Audio* Audio::GetInstance() {
    if (!instance_) {
        instance_.reset(new Audio());
    }
    return instance_.get();
}

void Audio::Initialize() {
    HRESULT result;
    result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(result));

    result = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(result));

    result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    assert(SUCCEEDED(result));
}

void Audio::Finalize() {
    for (auto voice : sourceVoices_) {
        voice->Stop();
        voice->DestroyVoice();
    }
    sourceVoices_.clear();

    for (auto& pair : soundDatas_) {
        UnloadAudio(&pair.second);
    }
    soundDatas_.clear();

    MFShutdown();
    if (xAudio2_) { xAudio2_.Reset(); }
}

void Audio::LoadAudio(const std::string& filename) {
    if (soundDatas_.contains(filename)) return;

    std::wstring wFilePath = StringUtility::ConvertString(filename);
    Microsoft::WRL::ComPtr<IMFSourceReader> pReader;
    HRESULT hr = MFCreateSourceReaderFromURL(wFilePath.c_str(), nullptr, &pReader);
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IMFMediaType> pAudioType;
    hr = MFCreateMediaType(&pAudioType);
    assert(SUCCEEDED(hr));
    hr = pAudioType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    assert(SUCCEEDED(hr));
    hr = pAudioType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    assert(SUCCEEDED(hr));

    hr = pReader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr, pAudioType.Get());
    assert(SUCCEEDED(hr));

    Microsoft::WRL::ComPtr<IMFMediaType> pUncompressedAudioType;
    hr = pReader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &pUncompressedAudioType);
    assert(SUCCEEDED(hr));

    WAVEFORMATEX* pWfex = nullptr;
    UINT32 wfexSize = 0;
    hr = MFCreateWaveFormatExFromMFMediaType(pUncompressedAudioType.Get(), &pWfex, &wfexSize);
    assert(SUCCEEDED(hr));

    std::vector<BYTE> audioData;
    while (true) {
        Microsoft::WRL::ComPtr<IMFSample> pSample;
        DWORD dwFlags = 0;
        hr = pReader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &dwFlags, nullptr, &pSample);
        assert(SUCCEEDED(hr));

        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) break;

        if (pSample) {
            Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
            hr = pSample->ConvertToContiguousBuffer(&pBuffer);
            assert(SUCCEEDED(hr));

            BYTE* pData = nullptr;
            DWORD cbData = 0;
            hr = pBuffer->Lock(&pData, nullptr, &cbData);
            assert(SUCCEEDED(hr));

            size_t currentSize = audioData.size();
            audioData.resize(currentSize + cbData);
            memcpy(audioData.data() + currentSize, pData, cbData);

            pBuffer->Unlock();
        }
    }

    SoundData soundData{};
    soundData.wfex = *pWfex;
    CoTaskMemFree(pWfex);

    soundData.bufferSize = static_cast<unsigned int>(audioData.size());
    soundData.pBuffer = std::make_unique<BYTE[]>(soundData.bufferSize);
    memcpy(soundData.pBuffer.get(), audioData.data(), soundData.bufferSize);

    soundDatas_.insert(std::make_pair(filename, std::move(soundData)));
}

void Audio::UnloadAudio(SoundData* soundData) {
    soundData->pBuffer.reset();
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void Audio::PlayAudio(const std::string& filename) {
    assert(soundDatas_.contains(filename));

    for (auto it = sourceVoices_.begin(); it != sourceVoices_.end(); ) {
        XAUDIO2_VOICE_STATE state;
        (*it)->GetState(&state);
        if (state.BuffersQueued == 0) {
            (*it)->DestroyVoice();
            it = sourceVoices_.erase(it);
        } else {
            ++it;
        }
    }

    const SoundData& soundData = soundDatas_.at(filename);
    HRESULT result;
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(result));

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer.get();
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    result = pSourceVoice->SubmitSourceBuffer(&buf);
    assert(SUCCEEDED(result));
    result = pSourceVoice->Start();
    assert(SUCCEEDED(result));

    sourceVoices_.insert(pSourceVoice);
}

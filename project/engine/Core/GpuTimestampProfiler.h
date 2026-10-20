#pragma once

#include <array>
#include <cstdint>
#include <vector>
#include <d3d12.h>
#include <wrl.h>

// One DIRECT queue; no waits or signals are issued by this class.
// Each submitted slot stays owned until its associated fence has completed.
class GpuTimestampProfiler {
public:
    enum class Role { Frame, Game, TitleBackground, TitleForeground };
    enum class Stage { Frame, Raymarch, Composite, ParticleUpdate, MainScene, AfterCloud, Post };
    enum class Path { Frame, Direct, LowResolution, Culled, Disabled };
    enum class State { Pending, Measured, Culled, Disabled, Reused, NotApplicable, CapacityExceeded, Invalid };
    struct Context {
        Role role = Role::Frame;
        Path path = Path::Frame;
        uint64_t invocation = 0;
        uint64_t constantsHash = 0, compositeConstantsHash = 0;
        uint32_t outputWidth = 0, outputHeight = 0, cloudWidth = 0, cloudHeight = 0;
        uint32_t viewSteps = 0, lightSteps = 0;
        float resolution = 0, cloudTime = 0;
        bool distanceLod = true, reallocated = false;
    };
    struct Sample {
        Context context;
        Stage stage = Stage::Frame;
        State state = State::Pending;
        double milliseconds = 0; // Valid only for Measured, including actual zero.
        uint32_t beginQuery = 0, endQuery = 0;
        bool ended = false;
    };
    struct FrameResult {
        uint64_t frame = 0, fence = 0, frequency = 0;
        Role scene = Role::Frame;
        std::vector<Sample> samples;
    };
    static constexpr uint32_t kInvalidToken = UINT32_MAX;
    static constexpr uint32_t kRingSize = 4;
    static constexpr uint32_t kQueriesPerFrame = 256;

    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Fence* fence);
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }
    bool IsAvailable() const { return frequency_ != 0; }
    void BeginFrame(ID3D12GraphicsCommandList* commands, uint32_t width, uint32_t height);
    bool HasBegunFrame() const { return frameBegun_; }
    void SetFrameOutputSize(uint32_t width, uint32_t height);
    void SetScene(Role role);
    uint32_t Begin(ID3D12GraphicsCommandList* commands, Stage stage, const Context& context);
    void End(ID3D12GraphicsCommandList* commands, uint32_t token);
    void Record(Stage stage, const Context& context, State state);
    void Resolve(ID3D12GraphicsCommandList* commands);
    void OnSubmitted(uint64_t fenceValue);
    void Collect();
    const FrameResult& GetLatest() const { return latest_; }
    uint32_t GetPendingCount() const;
    uint64_t GetDroppedFrames() const { return droppedFrames_; }
    static const char* StateName(State state);
    static const char* RoleName(Role role);
    static const char* PathName(Path path);

private:
    struct Slot {
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> queries;
        Microsoft::WRL::ComPtr<ID3D12Resource> readback;
        FrameResult result;
        uint32_t usedQueries = 0;
        bool pending = false, recording = false, resolved = false;
    };
    bool EnsureResources(Slot& slot);
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    std::array<Slot, kRingSize> slots_;
    Slot* active_ = nullptr;
    FrameResult latest_;
    uint64_t frequency_ = 0, nextFrame_ = 0, droppedFrames_ = 0;
    uint32_t frameToken_ = kInvalidToken;
    bool enabled_ = false, frameBegun_ = false;
};

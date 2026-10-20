#include "GpuTimestampProfiler.h"

bool GpuTimestampProfiler::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue, ID3D12Fence* fence) {
    if (!device || !queue || !fence || queue->GetDesc().Type != D3D12_COMMAND_LIST_TYPE_DIRECT) return false;
    // A profiler belongs to one device/queue/fence lifetime. Rebinding requires
    // the renderer to finish the old queue and destroy the old profiler first.
    if (device_) return device_.Get() == device && queue_.Get() == queue && fence_.Get() == fence;
    UINT64 frequency = 0;
    if (FAILED(queue->GetTimestampFrequency(&frequency)) || frequency == 0) return false;
    device_ = device; queue_ = queue; fence_ = fence; frequency_ = frequency;
    return true;
}

bool GpuTimestampProfiler::EnsureResources(Slot& slot) {
    if (slot.queries && slot.readback) return true;
    D3D12_QUERY_HEAP_DESC query{};
    query.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP; query.Count = kQueriesPerFrame;
    if (FAILED(device_->CreateQueryHeap(&query, IID_PPV_ARGS(&slot.queries)))) return false;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = kQueriesPerFrame * sizeof(uint64_t); buffer.Height = 1;
    buffer.DepthOrArraySize = 1; buffer.MipLevels = 1; buffer.SampleDesc.Count = 1;
    buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return SUCCEEDED(device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&slot.readback)));
}

void GpuTimestampProfiler::BeginFrame(ID3D12GraphicsCommandList* commands, uint32_t width, uint32_t height) {
    Collect();
    frameBegun_ = true;
    active_ = nullptr; frameToken_ = kInvalidToken;
    const uint64_t frame = ++nextFrame_;
    if (!enabled_ || !frequency_ || !commands) return;
    for (auto& slot : slots_) {
        if (slot.pending || slot.recording) continue;
        if (!EnsureResources(slot)) { ++droppedFrames_; return; }
        slot.result = {}; slot.result.frame = frame; slot.result.frequency = frequency_;
        slot.usedQueries = 0; slot.recording = true; slot.resolved = false; active_ = &slot;
        Context context; context.outputWidth = width; context.outputHeight = height;
        frameToken_ = Begin(commands, Stage::Frame, context);
        return;
    }
    ++droppedFrames_; // Ring full: skip diagnostics, never block rendering.
}

void GpuTimestampProfiler::SetFrameOutputSize(uint32_t width, uint32_t height) {
    if (!active_ || frameToken_ >= active_->result.samples.size()) return;
    auto& context = active_->result.samples[frameToken_].context;
    context.outputWidth = width; context.outputHeight = height;
}

void GpuTimestampProfiler::SetScene(Role role) { if (active_) active_->result.scene = role; }

uint32_t GpuTimestampProfiler::Begin(ID3D12GraphicsCommandList* commands, Stage stage, const Context& context) {
    if (!active_ || active_->resolved) return kInvalidToken;
    if (active_->result.samples.size() >= kQueriesPerFrame) return kInvalidToken;
    const uint32_t token = static_cast<uint32_t>(active_->result.samples.size());
    Sample sample; sample.context = context; sample.stage = stage;
    if (active_->usedQueries + 2 > kQueriesPerFrame) {
        sample.state = State::CapacityExceeded; active_->result.samples.push_back(sample); return kInvalidToken;
    }
    sample.beginQuery = active_->usedQueries++; sample.endQuery = active_->usedQueries++;
    commands->EndQuery(active_->queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, sample.beginQuery);
    active_->result.samples.push_back(sample);
    return token;
}

void GpuTimestampProfiler::End(ID3D12GraphicsCommandList* commands, uint32_t token) {
    if (!active_ || active_->resolved || token >= active_->result.samples.size()) return;
    auto& sample = active_->result.samples[token];
    if (sample.state != State::Pending || sample.ended) return;
    commands->EndQuery(active_->queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, sample.endQuery);
    sample.ended = true;
}

void GpuTimestampProfiler::Record(Stage stage, const Context& context, State state) {
    if (!active_ || active_->resolved || active_->result.samples.size() >= kQueriesPerFrame) return;
    Sample sample; sample.context = context; sample.stage = stage; sample.state = state;
    active_->result.samples.push_back(sample);
}

void GpuTimestampProfiler::Resolve(ID3D12GraphicsCommandList* commands) {
    if (!active_ || active_->resolved) return;
    End(commands, frameToken_);
    // Resolve only initialized queries, even if a caller forgot an End.
    for (uint32_t i = 0; i < active_->result.samples.size(); ++i) {
        auto& sample = active_->result.samples[i];
        if (sample.state == State::Pending && !sample.ended) {
            End(commands, i); sample.state = State::Invalid;
        }
    }
    commands->ResolveQueryData(active_->queries.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0,
        active_->usedQueries, active_->readback.Get(), 0);
    active_->resolved = true;
}

void GpuTimestampProfiler::OnSubmitted(uint64_t fenceValue) {
    frameBegun_ = false;
    if (!active_) return;
    active_->result.fence = fenceValue; active_->pending = true; active_->recording = false;
    active_ = nullptr;
}

void GpuTimestampProfiler::Collect() {
    if (!fence_) return;
    const uint64_t completed = fence_->GetCompletedValue();
    if (completed == UINT64_MAX) return; // Device removed: no valid readback.
    for (auto& slot : slots_) {
        if (!slot.pending || slot.result.fence > completed) continue;
        void* data = nullptr;
        const D3D12_RANGE range{ 0, slot.usedQueries * sizeof(uint64_t) };
        if (FAILED(slot.readback->Map(0, &range, &data))) continue;
        const auto* timestamps = static_cast<const uint64_t*>(data);
        for (auto& sample : slot.result.samples) {
            if (sample.state != State::Pending) continue;
            const auto begin = timestamps[sample.beginQuery], end = timestamps[sample.endQuery];
            if (!sample.ended || end < begin) { sample.state = State::Invalid; continue; }
            sample.milliseconds = static_cast<double>(end - begin) * 1000.0 / static_cast<double>(slot.result.frequency);
            sample.state = State::Measured;
        }
        const D3D12_RANGE written{ 0, 0 }; slot.readback->Unmap(0, &written);
        if (slot.result.frame > latest_.frame) latest_ = slot.result;
        slot.pending = false;
    }
}

uint32_t GpuTimestampProfiler::GetPendingCount() const {
    uint32_t count = 0; for (const auto& slot : slots_) if (slot.pending) ++count;
    return count;
}

const char* GpuTimestampProfiler::StateName(State state) {
    switch (state) {
    case State::Pending: return "Pending"; case State::Measured: return "Measured";
    case State::Culled: return "Culled"; case State::Disabled: return "Disabled";
    case State::Reused: return "Interval reuse"; case State::NotApplicable: return "N/A";
    case State::CapacityExceeded: return "Query capacity"; default: return "Invalid";
    }
}
const char* GpuTimestampProfiler::RoleName(Role role) {
    switch (role) { case Role::Game: return "Game"; case Role::TitleBackground: return "Title background";
    case Role::TitleForeground: return "Title foreground"; default: return "Frame"; }
}
const char* GpuTimestampProfiler::PathName(Path path) {
    switch (path) { case Path::Direct: return "Direct"; case Path::LowResolution: return "Low resolution";
    case Path::Culled: return "Culled"; case Path::Disabled: return "Disabled"; default: return "Frame"; }
}

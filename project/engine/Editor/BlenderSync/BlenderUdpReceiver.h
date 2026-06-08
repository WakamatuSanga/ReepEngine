#pragma once
#include "Engine/Editor/BlenderSync/BlenderSyncTypes.h"
#include <array>
#include <cstdint>
#include <string>

class BlenderUdpReceiver {
public:
    BlenderUdpReceiver();
    ~BlenderUdpReceiver();

    bool Start(uint16_t port = 50000);
    void Stop();
    void Update();

    bool IsRunning() const { return status_.isRunning; }
    bool HasUnreadPacket() const { return hasUnreadPacket_; }
    void MarkPacketRead() { hasUnreadPacket_ = false; }

    const std::string& GetLastPacket() const { return lastPacket_; }
    const BlenderUdpReceiverStatus& GetStatus() const { return status_; }

private:
    void SetLastError(const std::string& message);

    static constexpr uintptr_t kInvalidSocketHandle = static_cast<uintptr_t>(-1);
    uintptr_t socketHandle_ = kInvalidSocketHandle;
    bool winsockStarted_ = false;
    bool hasUnreadPacket_ = false;
    std::array<char, 65536> receiveBuffer_{};
    std::string lastPacket_;
    BlenderUdpReceiverStatus status_{};
};

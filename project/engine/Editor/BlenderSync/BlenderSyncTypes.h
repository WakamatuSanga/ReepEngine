#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

struct BlenderUdpReceiverStatus {
    bool isRunning = false;
    std::string address = "127.0.0.1";
    uint16_t port = 50000;
    uint64_t packetCount = 0;
    size_t lastPacketSize = 0;
    std::string lastReceiveTime;
    std::string lastError;
};

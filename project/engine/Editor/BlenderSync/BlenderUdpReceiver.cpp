#include "BlenderUdpReceiver.h"
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctime>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

namespace {
    SOCKET ToSocket(uintptr_t handle) {
        return static_cast<SOCKET>(handle);
    }

    std::string MakeSocketErrorMessage(const std::string& action, int errorCode) {
        return action + " failed. WSAGetLastError=" + std::to_string(errorCode);
    }

    std::string MakeCurrentTimeText() {
        std::time_t now = std::time(nullptr);
        std::tm localTime{};
        localtime_s(&localTime, &now);

        char buffer[32]{};
        std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
        return buffer;
    }
}

BlenderUdpReceiver::BlenderUdpReceiver() = default;

BlenderUdpReceiver::~BlenderUdpReceiver() {
    Stop();
}

bool BlenderUdpReceiver::Start(uint16_t port) {
    if (status_.isRunning) {
        return true;
    }

    status_.port = port;
    status_.lastError.clear();

    WSADATA wsaData{};
    const int startupResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (startupResult != 0) {
        SetLastError(MakeSocketErrorMessage("WSAStartup", startupResult));
        return false;
    }
    winsockStarted_ = true;

    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udpSocket == INVALID_SOCKET) {
        SetLastError(MakeSocketErrorMessage("socket", WSAGetLastError()));
        Stop();
        return false;
    }
    socketHandle_ = static_cast<uintptr_t>(udpSocket);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(udpSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
        SetLastError(MakeSocketErrorMessage("bind", WSAGetLastError()));
        Stop();
        return false;
    }

    u_long nonBlocking = 1;
    if (ioctlsocket(udpSocket, FIONBIO, &nonBlocking) == SOCKET_ERROR) {
        SetLastError(MakeSocketErrorMessage("ioctlsocket", WSAGetLastError()));
        Stop();
        return false;
    }

    status_.isRunning = true;
    status_.address = "127.0.0.1";
    status_.lastError.clear();
    return true;
}

void BlenderUdpReceiver::Stop() {
    if (socketHandle_ != kInvalidSocketHandle) {
        closesocket(ToSocket(socketHandle_));
        socketHandle_ = kInvalidSocketHandle;
    }

    if (winsockStarted_) {
        WSACleanup();
        winsockStarted_ = false;
    }

    status_.isRunning = false;
    hasUnreadPacket_ = false;
}

void BlenderUdpReceiver::Update() {
    if (!status_.isRunning || socketHandle_ == kInvalidSocketHandle) {
        return;
    }

    SOCKET udpSocket = ToSocket(socketHandle_);
    for (int receiveCount = 0; receiveCount < 32; ++receiveCount) {
        sockaddr_in fromAddress{};
        int fromSize = sizeof(fromAddress);
        const int receivedBytes = recvfrom(
            udpSocket,
            receiveBuffer_.data(),
            static_cast<int>(receiveBuffer_.size()),
            0,
            reinterpret_cast<sockaddr*>(&fromAddress),
            &fromSize);

        if (receivedBytes > 0) {
            lastPacket_.assign(receiveBuffer_.data(), static_cast<size_t>(receivedBytes));
            status_.lastPacketSize = static_cast<size_t>(receivedBytes);
            status_.lastReceiveTime = MakeCurrentTimeText();
            status_.lastError.clear();
            ++status_.packetCount;
            hasUnreadPacket_ = true;
            continue;
        }

        if (receivedBytes == SOCKET_ERROR) {
            const int errorCode = WSAGetLastError();
            if (errorCode != WSAEWOULDBLOCK) {
                SetLastError(MakeSocketErrorMessage("recvfrom", errorCode));
            }
            break;
        }

        break;
    }
}

void BlenderUdpReceiver::SetLastError(const std::string& message) {
    status_.lastError = message;
}

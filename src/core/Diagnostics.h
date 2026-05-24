#pragma once

#include <QString>

#include <cstdint>

namespace bgtc {

struct ClientDiagnostics {
    QString sdkVersion;
    QString gatewayHost;
    std::uint16_t gatewayPort = 0;
    std::uint64_t pushesReceived = 0;
    std::uint64_t snapshotsReceived = 0;
    std::uint64_t inputsSent = 0;
    std::uint64_t reconnectAttempts = 0;
    int latestFrame = 0;
    QString lastError;
    QString lastEvent;
};

}  // namespace bgtc

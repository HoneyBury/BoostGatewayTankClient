#pragma once

#include <QString>
#include <QDateTime>

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
    std::uint64_t errorsThisSession = 0;
    int latestFrame = 0;
    double averagePushRate = 0.0;
    double averageLatencyMs = 0.0;
    QString lastError;
    QString lastEvent;
    QString currentRoomId;
    QString currentBattleId;
    qint64 uptimeMs = 0;
    QDateTime connectedSince;
};

}  // namespace bgtc

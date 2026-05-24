#pragma once

#include <QString>

namespace bgtc {

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Registering,
    LoggedIn,
    InLobby,
    InRoom,
    InBattle,
    Reconnecting,
};

struct ClientSession {
    QString userId;
    QString displayName;
    QString roomId;
    QString battleId;
    QString lastWinnerUserId;
    QString lastFinishReason;
    int lastBattleFrames = 0;
    int lastBattleScore = 0;
    bool ready = false;
    ConnectionState state = ConnectionState::Disconnected;
};

}  // namespace bgtc

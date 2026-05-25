#pragma once

#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QTextEdit;
class QTimer;

namespace bgtc {

class LobbyWidget final : public QWidget {
    Q_OBJECT

public:
    LobbyWidget(AppConfig config, ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);
    void refreshRoomList();

signals:
    void roomEntered();
    void leaderboardRequested();
    void themeChanged(QString themeName);
    void matchRoomReceived(QString roomId);
    void matchmakingRequested();

private:
    void createRoom();
    void joinRoom(const QString& roomId);
    void joinMatchmaking();
    void leaveMatchmaking();
    void refreshMatchStatus();
    void renderRoomList(const QString& body);
    void refreshLobbyStateCard(int queueSize = -1);
    void appendLog(const QString& text);
    void setRoomCell(int row, int column, const QString& text);

    AppConfig config_;
    ClientSession& session_;
    GatewayClient& gateway_;
    QLineEdit* roomEdit_ = nullptr;
    QLabel* lobbyState_ = nullptr;
    QComboBox* themeBox_ = nullptr;
    QComboBox* modeBox_ = nullptr;
    QSpinBox* mmrBox_ = nullptr;
    QTableWidget* roomTable_ = nullptr;
    QTextEdit* log_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
};

}  // namespace bgtc

#pragma once

#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QTextEdit;
class QListWidget;
class QListWidgetItem;

namespace bgtc {

class LobbyWidget final : public QWidget {
    Q_OBJECT

public:
    LobbyWidget(AppConfig config, ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

signals:
    void battleStarted(QString battleId);
    void leaderboardRequested();

private:
    void createRoom();
    void joinRoom();
    void leaveRoom();
    void setReady();
    void unsetReady();
    void startBattle();
    void refreshLeaderboard();
    void refreshRoomList();
    void refreshRoomDetail();
    void showUnsupportedRoomAdmin();
    void appendLog(const QString& text);
    void refreshRoomSummary();
    void selectRoomFromList(QListWidgetItem* item);
    void renderRoomList(const QString& body);
    [[nodiscard]] bool requireRoom(const QString& action);

    AppConfig config_;
    ClientSession& session_;
    GatewayClient& gateway_;
    QLineEdit* roomEdit_ = nullptr;
    QLabel* roomState_ = nullptr;
    QLabel* capabilityState_ = nullptr;
    QListWidget* roomList_ = nullptr;
    QTextEdit* log_ = nullptr;
};

}  // namespace bgtc

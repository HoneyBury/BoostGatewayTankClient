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
class QPushButton;
class QStackedWidget;

namespace bgtc {

class LobbyWidget final : public QWidget {
    Q_OBJECT

public:
    LobbyWidget(AppConfig config, ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

signals:
    void battleStarted(QString battleId);
    void leaderboardRequested();
    void returnToBattleRequested();

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
    void kickRoomMember();
    void transferRoomOwner();
    void appendLog(const QString& text);
    void refreshRoomSummary();
    void showLobbyPage();
    void showRoomPage();
    void selectRoomFromList(QListWidgetItem* item);
    void renderRoomList(const QString& body);
    void renderRoomDetail(const QString& body);
    [[nodiscard]] bool requireRoom(const QString& action);

    AppConfig config_;
    ClientSession& session_;
    GatewayClient& gateway_;
    QLineEdit* roomEdit_ = nullptr;
    QLineEdit* adminUserEdit_ = nullptr;
    QLabel* roomState_ = nullptr;
    QLabel* capabilityState_ = nullptr;
    QStackedWidget* pageStack_ = nullptr;
    QListWidget* roomList_ = nullptr;
    QListWidget* memberList_ = nullptr;
    QPushButton* returnBattleButton_ = nullptr;
    QTextEdit* log_ = nullptr;
};

}  // namespace bgtc

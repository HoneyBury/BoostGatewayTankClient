#pragma once

#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QTextEdit;

namespace bgtc {

class LobbyWidget final : public QWidget {
    Q_OBJECT

public:
    LobbyWidget(AppConfig config, ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

signals:
    void battleStarted(QString battleId);

private:
    void createRoom();
    void joinRoom();
    void leaveRoom();
    void setReady();
    void startBattle();
    void refreshLeaderboard();
    void appendLog(const QString& text);

    AppConfig config_;
    ClientSession& session_;
    GatewayClient& gateway_;
    QLineEdit* roomEdit_ = nullptr;
    QTextEdit* log_ = nullptr;
};

}  // namespace bgtc

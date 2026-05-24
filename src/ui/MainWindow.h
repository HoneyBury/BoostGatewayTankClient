#pragma once

#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankWorldModel.h"

#include <QMainWindow>

class QStackedWidget;
class QLabel;
class QPushButton;

namespace bgtc {

class LobbyWidget;
class BattleWidget;
class DiagnosticsWidget;
class LeaderboardWidget;
class ReplayWidget;
class SettingsWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(AppConfig config, QString userId, QString token, QWidget* parent = nullptr);

private:
    void connectAndLogin(const QString& token);
    void reconnect();
    void setStatus(const QString& text);
    void showPage(int index);
    void showLobby();
    void showBattle();
    void showLoadingThenBattle(const QString& battleId);
    void handleTankSnapshot(const bgtc::TankSnapshot& snapshot);
    bool restoreBattleSnapshot();
    QString activeBattleIdFromRoomDetail(const QString& roomId);

    AppConfig config_;
    QString token_;
    ClientSession session_;
    GatewayClient gateway_;
    QPushButton* backButton_ = nullptr;
    QPushButton* lobbyButton_ = nullptr;
    QStackedWidget* stack_ = nullptr;
    LobbyWidget* lobby_ = nullptr;
    BattleWidget* battle_ = nullptr;
    LeaderboardWidget* leaderboard_ = nullptr;
    ReplayWidget* replay_ = nullptr;
    DiagnosticsWidget* diagnostics_ = nullptr;
    SettingsWidget* settings_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace bgtc

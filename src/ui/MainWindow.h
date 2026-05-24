#pragma once

#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QMainWindow>

class QStackedWidget;
class QLabel;

namespace bgtc {

class LobbyWidget;
class BattleWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(AppConfig config, QString userId, QString token, QWidget* parent = nullptr);

private:
    void connectAndLogin(const QString& token);
    void setStatus(const QString& text);

    AppConfig config_;
    ClientSession session_;
    GatewayClient gateway_;
    QStackedWidget* stack_ = nullptr;
    LobbyWidget* lobby_ = nullptr;
    BattleWidget* battle_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace bgtc

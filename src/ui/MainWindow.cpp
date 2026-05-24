#include "ui/MainWindow.h"

#include "ui/BattleWidget.h"
#include "ui/LobbyWidget.h"

#include <QLabel>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>

namespace bgtc {

MainWindow::MainWindow(AppConfig config, QString userId, QString token, QWidget* parent)
    : QMainWindow(parent), config_(std::move(config)) {
    session_.userId = std::move(userId);
    session_.displayName = session_.userId;

    setWindowTitle("BoostGateway Tank Client");
    stack_ = new QStackedWidget(this);
    lobby_ = new LobbyWidget(config_, session_, gateway_, this);
    battle_ = new BattleWidget(session_, gateway_, this);

    stack_->addWidget(lobby_);
    stack_->addWidget(battle_);
    setCentralWidget(stack_);

    statusLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statusLabel_);

    connect(lobby_, &LobbyWidget::battleStarted, this, [this](const QString& battleId) {
        session_.battleId = battleId;
        session_.state = ConnectionState::InBattle;
        stack_->setCurrentWidget(battle_);
        setStatus("Battle started: " + battleId);
    });
    connect(&gateway_, &GatewayClient::tankSnapshotReceived, battle_, &BattleWidget::applySnapshot);
    connect(&gateway_, &GatewayClient::disconnected, this, [this]() {
        session_.state = ConnectionState::Disconnected;
        setStatus("Disconnected from gateway.");
    });

    connectAndLogin(token);
}

void MainWindow::connectAndLogin(const QString& token) {
    QString error;
    session_.state = ConnectionState::Connecting;
    setStatus("Connecting...");
    if (!gateway_.connectToGateway(config_, &error)) {
        QMessageBox::critical(this, "Connect Failed", error);
        setStatus("Connect failed.");
        return;
    }

    session_.state = ConnectionState::Connected;
    setStatus("Logging in...");
    if (!gateway_.login(session_.userId, token, &error)) {
        QMessageBox::critical(this, "Login Failed", error);
        setStatus("Login failed.");
        return;
    }

    session_.state = ConnectionState::InLobby;
    setStatus("Logged in as " + session_.userId);
}

void MainWindow::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

}  // namespace bgtc

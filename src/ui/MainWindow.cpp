#include "ui/MainWindow.h"

#include "ui/BattleWidget.h"
#include "ui/DiagnosticsWidget.h"
#include "ui/LeaderboardWidget.h"
#include "ui/LobbyWidget.h"
#include "ui/ReplayWidget.h"
#include "ui/SettingsWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QWidget>

namespace bgtc {

MainWindow::MainWindow(AppConfig config, QString userId, QString token, QWidget* parent)
    : QMainWindow(parent), config_(std::move(config)), token_(std::move(token)) {
    session_.userId = std::move(userId);
    session_.displayName = session_.userId;

    setWindowTitle("BoostGateway Tank Client");
    auto* shell = new QWidget(this);
    auto* shellLayout = new QHBoxLayout(shell);
    navigation_ = new QListWidget(shell);
    navigation_->addItems({"房间大厅", "战斗", "排行榜", "回放", "诊断", "设置"});
    navigation_->setFixedWidth(150);

    stack_ = new QStackedWidget(shell);
    lobby_ = new LobbyWidget(config_, session_, gateway_, this);
    battle_ = new BattleWidget(session_, gateway_, this);
    leaderboard_ = new LeaderboardWidget(session_, gateway_, this);
    replay_ = new ReplayWidget(session_, gateway_, this);
    diagnostics_ = new DiagnosticsWidget(gateway_, this);
    settings_ = new SettingsWidget(config_, session_, gateway_, this);

    stack_->addWidget(lobby_);
    stack_->addWidget(battle_);
    stack_->addWidget(leaderboard_);
    stack_->addWidget(replay_);
    stack_->addWidget(diagnostics_);
    stack_->addWidget(settings_);

    shellLayout->addWidget(navigation_);
    shellLayout->addWidget(stack_, 1);
    setCentralWidget(shell);
    navigation_->setCurrentRow(0);

    statusLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(statusLabel_);
    auto* reconnectAction = menuBar()->addAction("重连");
    connect(reconnectAction, &QAction::triggered, this, &MainWindow::reconnect);

    connect(navigation_, &QListWidget::currentRowChanged, this, &MainWindow::showPage);
    connect(lobby_, &LobbyWidget::battleStarted, this, [this](const QString& battleId) {
        session_.battleId = battleId;
        session_.state = ConnectionState::InBattle;
        navigation_->setCurrentRow(1);
        setStatus("Battle started: " + battleId);
    });
    connect(&gateway_, &GatewayClient::tankSnapshotReceived, battle_, &BattleWidget::applySnapshot);
    connect(&gateway_, &GatewayClient::tankSnapshotReceived, this, &MainWindow::handleTankSnapshot);
    connect(&gateway_, &GatewayClient::disconnected, this, [this]() {
        session_.state = ConnectionState::Disconnected;
        setStatus("Disconnected from gateway.");
    });

    connectAndLogin(token_);
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

void MainWindow::reconnect() {
    QString error;
    session_.state = ConnectionState::Reconnecting;
    setStatus("Reconnecting...");
    if (!gateway_.reconnectToGateway(config_, session_.userId, token_, &error)) {
        QMessageBox::warning(this, "重连失败", error);
        setStatus("Reconnect failed.");
        return;
    }
    session_.state = session_.battleId.isEmpty() ? ConnectionState::InLobby : ConnectionState::InBattle;
    setStatus("Reconnected.");
}

void MainWindow::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

void MainWindow::showPage(int index) {
    if (index >= 0 && index < stack_->count()) {
        stack_->setCurrentIndex(index);
    }
}

void MainWindow::handleTankSnapshot(const TankSnapshot& snapshot) {
    if (!snapshot.finished) {
        return;
    }

    session_.state = ConnectionState::InLobby;
    session_.lastWinnerUserId = QString::fromStdString(snapshot.winnerUserId);
    session_.lastFinishReason = QString::fromStdString(snapshot.finishReason);
    session_.lastBattleFrames = snapshot.totalFrames > 0 ? snapshot.totalFrames : snapshot.frame;
    session_.lastBattleScore = 0;
    const auto userId = session_.userId.toStdString();
    for (const auto& score : snapshot.scores) {
        if (score.userId == userId) {
            session_.lastBattleScore = score.score;
            break;
        }
    }

    const auto winner = session_.lastWinnerUserId.isEmpty() ? "未知" : session_.lastWinnerUserId;
    setStatus(QString("Battle finished. winner=%1 frames=%2")
                  .arg(winner)
                  .arg(session_.lastBattleFrames));
    if (leaderboard_ != nullptr) {
        leaderboard_->refreshAfterBattle();
    }
    if (navigation_ != nullptr) {
        navigation_->setCurrentRow(2);
    }
}

}  // namespace bgtc

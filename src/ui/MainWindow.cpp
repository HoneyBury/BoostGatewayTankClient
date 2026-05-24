#include "ui/MainWindow.h"

#include "ui/BattleWidget.h"
#include "ui/DiagnosticsWidget.h"
#include "ui/LeaderboardWidget.h"
#include "ui/LobbyWidget.h"
#include "ui/ReplayWidget.h"
#include "ui/SettingsWidget.h"

#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
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
    resize(1180, 760);
    setStyleSheet(R"(
        QMainWindow {
            background: #0f1720;
        }
        QWidget {
            color: #dfe7ef;
            font-size: 14px;
        }
        QListWidget#MainNavigation {
            background: #111c28;
            border: 1px solid #243447;
            border-radius: 16px;
            padding: 10px;
            outline: 0;
        }
        QListWidget#MainNavigation::item {
            border-radius: 10px;
            margin: 3px 0;
            padding: 12px 14px;
        }
        QListWidget#MainNavigation::item:selected {
            background: #1f8a70;
            color: #ffffff;
        }
        QListWidget#MainNavigation::item:hover:!selected {
            background: #1a2a3a;
        }
        QLabel#PageTitle {
            color: #f7fbff;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#PageHint {
            color: #9fb0c2;
            line-height: 1.4;
        }
        QLabel#StatePill {
            background: #132436;
            border: 1px solid #2c4359;
            border-radius: 12px;
            color: #cfe8ff;
            padding: 10px 12px;
        }
        QLineEdit, QTextEdit, QListWidget {
            background: #101a25;
            border: 1px solid #26384a;
            border-radius: 12px;
            color: #e8f1f8;
            selection-background-color: #1f8a70;
        }
        QLineEdit {
            padding: 10px 12px;
        }
        QTextEdit {
            padding: 10px;
        }
        QPushButton {
            background: #1c2f42;
            border: 1px solid #34506a;
            border-radius: 11px;
            color: #edf6ff;
            padding: 9px 14px;
        }
        QPushButton:hover {
            background: #25415a;
            border-color: #4d718f;
        }
        QPushButton:pressed {
            background: #183047;
        }
        QStatusBar {
            background: #0b1118;
            color: #aebdca;
        }
        QMenuBar {
            background: #0f1720;
            color: #dfe7ef;
        }
    )");

    auto* shell = new QWidget(this);
    auto* shellLayout = new QHBoxLayout(shell);
    shellLayout->setContentsMargins(18, 18, 18, 14);
    shellLayout->setSpacing(18);
    navigation_ = new QListWidget(shell);
    navigation_->setObjectName("MainNavigation");
    navigation_->addItems({"大厅 Lobby", "战斗 Battle", "排行榜", "回放", "诊断", "设置"});
    navigation_->setFixedWidth(172);
    navigation_->setSpacing(2);

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
    statusLabel_->setObjectName("PageHint");
    statusBar()->addPermanentWidget(statusLabel_);
    auto* reconnectAction = menuBar()->addAction("重连");
    connect(reconnectAction, &QAction::triggered, this, &MainWindow::reconnect);

    connect(navigation_, &QListWidget::currentRowChanged, this, &MainWindow::showPage);
    connect(lobby_, &LobbyWidget::battleStarted, this, [this](const QString& battleId) {
        session_.battleId = battleId;
        session_.state = ConnectionState::InBattle;
        navigation_->setCurrentRow(1);
        setStatus("战斗已开始：" + battleId);
    });
    connect(&gateway_, &GatewayClient::tankSnapshotReceived, battle_, &BattleWidget::applySnapshot);
    connect(&gateway_, &GatewayClient::tankSnapshotReceived, this, &MainWindow::handleTankSnapshot);
    connect(&gateway_, &GatewayClient::sessionResumed, this, [this](const QString& roomId, bool inBattle) {
        if (!roomId.isEmpty()) {
            session_.roomId = roomId;
        }
        session_.state = inBattle ? ConnectionState::InBattle : ConnectionState::InRoom;
        if (inBattle) {
            navigation_->setCurrentRow(1);
        } else {
            navigation_->setCurrentRow(0);
        }
        setStatus(QString("服务端恢复会话：room=%1，battle=%2")
                      .arg(session_.roomId.isEmpty() ? "未知" : session_.roomId)
                      .arg(inBattle ? "是" : "否"));
    });
    connect(&gateway_, &GatewayClient::sessionKicked, this, [this](const QString& reason) {
        session_.state = ConnectionState::Disconnected;
        QMessageBox::warning(this, "会话被替换", "当前账号在其他连接登录或会话被服务端替换：\n" + reason);
        setStatus("会话被替换，请重新登录或重连。");
    });
    connect(&gateway_, &GatewayClient::disconnected, this, [this]() {
        session_.state = ConnectionState::Disconnected;
        setStatus("已断开与网关的连接。");
    });

    connectAndLogin(token_);
}

void MainWindow::connectAndLogin(const QString& token) {
    QString error;
    session_.state = ConnectionState::Connecting;
    setStatus("正在连接网关...");
    if (!gateway_.connectToGateway(config_, &error)) {
        QMessageBox::critical(this, "Connect Failed", error);
        setStatus("连接失败，请检查服务端地址或网络。");
        return;
    }

    session_.state = ConnectionState::Connected;
    setStatus("连接成功，正在登录...");
    if (!gateway_.login(session_.userId, token, &error)) {
        QMessageBox::critical(this, "Login Failed", error);
        setStatus("登录失败，请检查账号或 token。");
        return;
    }

    session_.state = ConnectionState::InLobby;
    setStatus("已登录：" + session_.userId + "，可以创建或加入房间。");
}

void MainWindow::reconnect() {
    QString error;
    session_.state = ConnectionState::Reconnecting;
    setStatus("正在重连...");
    if (!gateway_.reconnectToGateway(config_, session_.userId, token_, &error)) {
        QMessageBox::warning(this, "重连失败", error);
        setStatus("重连失败，请稍后再试。");
        return;
    }
    if (restoreBattleSnapshot()) {
        session_.state = ConnectionState::InBattle;
        navigation_->setCurrentRow(1);
        setStatus("重连成功，已从服务端恢复最新战斗 snapshot。");
        return;
    }
    session_.state = session_.battleId.isEmpty() ? ConnectionState::InLobby : ConnectionState::InBattle;
    setStatus("重连成功，已恢复本地上下文；当前没有可查询的 battle snapshot。");
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
    if (snapshot.battleState.has_value() && !snapshot.battleState->battleId.empty()) {
        session_.battleId = QString::fromStdString(snapshot.battleState->battleId);
        if (!snapshot.finished) {
            session_.state = ConnectionState::InBattle;
        }
    }

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
    setStatus(QString("战斗结束：胜者 %1，总帧数 %2")
                  .arg(winner)
                  .arg(session_.lastBattleFrames));
    if (leaderboard_ != nullptr) {
        leaderboard_->refreshAfterBattle();
    }
    if (navigation_ != nullptr) {
        navigation_->setCurrentRow(2);
    }
}

QString MainWindow::activeBattleIdFromRoomDetail(const QString& roomId) {
    if (roomId.isEmpty()) {
        return {};
    }
    QString error;
    const auto body = gateway_.queryRoomDetail(roomId, &error);
    if (!error.isEmpty() || body.isEmpty()) {
        return {};
    }
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    const auto room = doc.object().value("room").toObject();
    return room.value("active_battle_id").toString();
}

bool MainWindow::restoreBattleSnapshot() {
    auto battleId = session_.battleId;
    if (battleId.isEmpty()) {
        battleId = activeBattleIdFromRoomDetail(session_.roomId);
        if (!battleId.isEmpty()) {
            session_.battleId = battleId;
        }
    }
    if (battleId.isEmpty()) {
        return false;
    }

    QString error;
    const auto body = gateway_.queryBattleState(battleId, &error);
    if (!error.isEmpty() || body.isEmpty()) {
        return false;
    }
    auto snapshot = decodeTankSnapshot(body.toStdString());
    if (!snapshot.has_value()) {
        return false;
    }
    battle_->applySnapshot(*snapshot);
    handleTankSnapshot(*snapshot);
    return true;
}

}  // namespace bgtc

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
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QVBoxLayout>
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
        QWidget#TopBar {
            background: #101a25;
            border: 1px solid #26384a;
            border-radius: 18px;
        }
    )");

    auto* shell = new QWidget(this);
    auto* shellLayout = new QVBoxLayout(shell);
    shellLayout->setContentsMargins(18, 18, 18, 14);
    shellLayout->setSpacing(14);

    auto* topBar = new QWidget(shell);
    topBar->setObjectName("TopBar");
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(14, 10, 14, 10);
    auto* title = new QLabel("BoostGateway Tank Arena", topBar);
    title->setObjectName("PageTitle");
    backButton_ = new QPushButton("返回大厅", topBar);
    lobbyButton_ = new QPushButton("大厅/房间", topBar);
    auto* leaderboardButton = new QPushButton("排行榜", topBar);
    auto* replayButton = new QPushButton("回放", topBar);
    auto* diagnosticsButton = new QPushButton("诊断", topBar);
    auto* settingsButton = new QPushButton("设置", topBar);
    topLayout->addWidget(title, 1);
    topLayout->addWidget(backButton_);
    topLayout->addWidget(lobbyButton_);
    topLayout->addWidget(leaderboardButton);
    topLayout->addWidget(replayButton);
    topLayout->addWidget(diagnosticsButton);
    topLayout->addWidget(settingsButton);

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

    shellLayout->addWidget(topBar);
    shellLayout->addWidget(stack_, 1);
    setCentralWidget(shell);
    showLobby();

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("PageHint");
    statusBar()->addPermanentWidget(statusLabel_);
    auto* reconnectAction = menuBar()->addAction("重连");
    connect(reconnectAction, &QAction::triggered, this, &MainWindow::reconnect);

    connect(backButton_, &QPushButton::clicked, this, &MainWindow::showLobby);
    connect(lobbyButton_, &QPushButton::clicked, this, &MainWindow::showLobby);
    connect(leaderboardButton, &QPushButton::clicked, this, [this]() { showPage(2); });
    connect(replayButton, &QPushButton::clicked, this, [this]() { showPage(3); });
    connect(diagnosticsButton, &QPushButton::clicked, this, [this]() { showPage(4); });
    connect(settingsButton, &QPushButton::clicked, this, [this]() { showPage(5); });
    connect(lobby_, &LobbyWidget::battleStarted, this, [this](const QString& battleId) {
        enterBattle(battleId);
    });
    connect(lobby_, &LobbyWidget::returnToBattleRequested, this, [this]() {
        if (!session_.battleId.isEmpty()) {
            showBattle();
        }
    });
    connect(&gateway_, &GatewayClient::battleStartedPush, this, [this](const QString& roomId, const QString& battleId) {
        if (!roomId.isEmpty()) {
            session_.roomId = roomId;
        }
        enterBattle(battleId);
    });
    connect(&gateway_, &GatewayClient::tankSnapshotReceived, battle_, &BattleWidget::applySnapshot);
    connect(&gateway_, &GatewayClient::tankSnapshotReceived, this, &MainWindow::handleTankSnapshot);
    connect(&gateway_, &GatewayClient::sessionResumed, this, [this](const QString& roomId, bool inBattle) {
        if (!roomId.isEmpty()) {
            session_.roomId = roomId;
        }
        session_.state = inBattle ? ConnectionState::InBattle : ConnectionState::InRoom;
        if (inBattle) {
            showBattle();
        } else {
            showLobby();
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
        showBattle();
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
        if (backButton_ != nullptr) {
            backButton_->setVisible(index != 0);
        }
    }
}

void MainWindow::showLobby() {
    showPage(0);
}

void MainWindow::showBattle() {
    showPage(1);
    battle_->setFocus(Qt::OtherFocusReason);
    setStatus("已进入战斗：" + (session_.battleId.isEmpty() ? "等待 battle id" : session_.battleId));
}

void MainWindow::enterBattle(const QString& battleId) {
    if (battleId.isEmpty()) {
        return;
    }
    if (session_.battleId == battleId && session_.state == ConnectionState::InBattle &&
        stack_->currentIndex() == 1) {
        return;
    }
    session_.battleId = battleId;
    session_.state = ConnectionState::InBattle;
    showLoadingThenBattle(battleId);
}

void MainWindow::showLoadingThenBattle(const QString& battleId) {
    auto* loading = new QWidget(this);
    auto* layout = new QVBoxLayout(loading);
    layout->setContentsMargins(56, 56, 56, 56);
    layout->setSpacing(18);
    auto* title = new QLabel("正在载入战斗", loading);
    title->setObjectName("PageTitle");
    auto* hint = new QLabel(QString("Battle: %1\nRoom: %2\n玩家: %3\n正在等待服务端分配出生点、生成道具与同步初始状态...")
                                .arg(battleId, session_.roomId, session_.userId),
                            loading);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    auto* progress = new QProgressBar(loading);
    progress->setRange(0, 100);
    progress->setValue(15);
    layout->addStretch(1);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addWidget(progress);
    layout->addStretch(2);

    const int loadingIndex = stack_->addWidget(loading);
    showPage(loadingIndex);
    setStatus("战斗加载中：" + battleId);

    auto* timer = new QTimer(loading);
    timer->setInterval(90);
    connect(timer, &QTimer::timeout, this, [this, progress, loading, loadingIndex, timer]() {
        progress->setValue(std::min(100, progress->value() + 8));
        if (progress->value() >= 100) {
            timer->stop();
            showBattle();
            stack_->removeWidget(loading);
            loading->deleteLater();
        }
    });
    timer->start();
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
    showPage(2);
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

#include "ui/MainWindow.h"

#include "ui/BattleWidget.h"
#include "ui/DiagnosticsWidget.h"
#include "ui/LeaderboardWidget.h"
#include "ui/LobbyWidget.h"
#include "ui/MatchmakingWidget.h"
#include "ui/ReplayWidget.h"
#include "ui/RoomWidget.h"
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
#include <algorithm>

namespace bgtc {

MainWindow::MainWindow(AppConfig config, QString userId, QString token, QWidget* parent)
    : QMainWindow(parent), config_(std::move(config)), token_(std::move(token)) {
    session_.userId = std::move(userId);
    session_.displayName = session_.userId;

    setWindowTitle("BoostGateway Tank Client");
    resize(1180, 760);
    applyTheme("霓虹蓝");

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
    lobbyButton_ = new QPushButton("大厅", topBar);
    auto* roomButton = new QPushButton("房间", topBar);
    auto* leaderboardButton = new QPushButton("排行榜", topBar);
    auto* replayButton = new QPushButton("回放", topBar);
    auto* diagnosticsButton = new QPushButton("诊断", topBar);
    auto* settingsButton = new QPushButton("设置", topBar);
    auto* matchmakingButton = new QPushButton("匹配", topBar);
    topLayout->addWidget(title, 1);
    topLayout->addWidget(backButton_);
    topLayout->addWidget(lobbyButton_);
    topLayout->addWidget(roomButton);
    topLayout->addWidget(leaderboardButton);
    topLayout->addWidget(replayButton);
    topLayout->addWidget(diagnosticsButton);
    topLayout->addWidget(settingsButton);
    topLayout->addWidget(matchmakingButton);

    stack_ = new QStackedWidget(shell);
    lobby_ = new LobbyWidget(config_, session_, gateway_, this);
    room_ = new RoomWidget(session_, gateway_, this);
    battle_ = new BattleWidget(session_, gateway_, this);
    leaderboard_ = new LeaderboardWidget(session_, gateway_, this);
    replay_ = new ReplayWidget(session_, gateway_, this);
    diagnostics_ = new DiagnosticsWidget(gateway_, this);
    settings_ = new SettingsWidget(config_, session_, gateway_, this);
    matchmaking_ = new MatchmakingWidget(session_, gateway_, this);

    stack_->addWidget(lobby_);
    stack_->addWidget(room_);
    stack_->addWidget(battle_);
    stack_->addWidget(leaderboard_);
    stack_->addWidget(replay_);
    stack_->addWidget(diagnostics_);
    stack_->addWidget(settings_);
    matchmakingIndex_ = stack_->addWidget(matchmaking_);

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
    connect(roomButton, &QPushButton::clicked, this, &MainWindow::showRoom);
    connect(leaderboardButton, &QPushButton::clicked, this, [this]() {
        leaderboard_->refreshTop();
        showPage(3);
    });
    connect(replayButton, &QPushButton::clicked, this, [this]() { showPage(4); });
    connect(diagnosticsButton, &QPushButton::clicked, this, [this]() { showPage(5); });
    connect(settingsButton, &QPushButton::clicked, this, [this]() { showPage(6); });
    connect(matchmakingButton, &QPushButton::clicked, this, [this]() { showPage(matchmakingIndex_); });
    connect(lobby_, &LobbyWidget::roomEntered, this, [this]() {
        room_->refreshRoomDetail();
        showRoom();
    });
    connect(lobby_, &LobbyWidget::matchRoomReceived, this, [this](const QString& roomId) {
        session_.roomId = roomId;
        room_->refreshRoomDetail();
        showRoom();
    });
    connect(lobby_, &LobbyWidget::leaderboardRequested, this, [this]() {
        leaderboard_->refreshTop();
        showPage(3);
    });
    connect(lobby_, &LobbyWidget::matchmakingRequested, this, [this]() { showPage(matchmakingIndex_); });
    connect(lobby_, &LobbyWidget::themeChanged, this, &MainWindow::applyTheme);
    connect(room_, &RoomWidget::battleStarted, this, [this](const QString& battleId) {
        enterBattle(battleId);
    });
    connect(room_, &RoomWidget::loadingRequested, this, &MainWindow::enterBattleLoading);
    connect(room_, &RoomWidget::leftRoom, this, &MainWindow::showLobby);
    connect(room_, &RoomWidget::returnToBattleRequested, this, [this]() {
        if (!session_.battleId.isEmpty()) {
            showBattle();
        }
    });
    connect(room_, &RoomWidget::themeChanged, this, &MainWindow::applyTheme);
    connect(&gateway_, &GatewayClient::battleStartedPush, this, [this](const QString& roomId, const QString& battleId) {
        if (!roomId.isEmpty()) {
            session_.roomId = roomId;
        }
        enterBattleLoading(battleId);
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
            showRoom();
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
    connect(matchmaking_, &MatchmakingWidget::matchFound, this, [this](const MatchFoundState& match) {
        session_.roomId = QString::fromStdString(match.roomId);
        session_.matchId = QString::fromStdString(match.matchId);
        room_->refreshRoomDetail();
        showRoom();
    });

    connectAndLogin(token_);
}

void MainWindow::connectAndLogin(const QString& token) {
    QString error;
    session_.state = ConnectionState::Connecting;
    setStatus("正在连接网关...");
    if (!gateway_.connectToGateway(config_, &error)) {
        QMessageBox::critical(this, "连接失败", error);
        setStatus("连接失败，请检查服务端地址或网络。");
        return;
    }

    session_.state = ConnectionState::Connected;
    setStatus("连接成功，正在登录...");
    if (!gateway_.login(session_.userId, token, &error)) {
        QMessageBox::critical(this, "登录失败", error);
        setStatus("登录失败，请检查账号或 token。");
        return;
    }

    session_.state = ConnectionState::InLobby;
    setStatus("已登录：" + session_.userId + "，可以创建、加入房间或直接匹配。");
    if (lobby_ != nullptr) {
        lobby_->refreshRoomList();
    }
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
        setStatus("重连成功，已恢复最新战斗状态。");
        return;
    }
    session_.state = session_.roomId.isEmpty() ? ConnectionState::InLobby : ConnectionState::InRoom;
    if (session_.roomId.isEmpty()) {
        showLobby();
    } else {
        showRoom();
    }
    setStatus("重连成功，已恢复本地上下文。");
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

void MainWindow::showRoom() {
    if (session_.roomId.isEmpty()) {
        showLobby();
        return;
    }
    if (room_ != nullptr) {
        room_->refreshRoomDetail();
    }
    showPage(1);
}

void MainWindow::showBattle() {
    showPage(2);
    battle_->setFocus(Qt::OtherFocusReason);
    setStatus("已进入战斗：" + (session_.battleId.isEmpty() ? "等待 battle id" : session_.battleId));
}

void MainWindow::applyTheme(const QString& themeName) {
    const auto accent = themeName == "沙漠橙" ? QString("#f59e0b")
                       : themeName == "森林绿" ? QString("#22c55e")
                                              : QString("#38bdf8");
    const auto accentDark = themeName == "沙漠橙" ? QString("#7c3f10")
                           : themeName == "森林绿" ? QString("#14532d")
                                                  : QString("#164e63");
    const auto bg = themeName == "沙漠橙" ? QString("#20150c")
                  : themeName == "森林绿" ? QString("#071a12")
                                         : QString("#0f1720");
    setStyleSheet(QString(R"(
        QMainWindow {
            background: %1;
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
            background: %3;
            border: 1px solid %2;
            border-radius: 12px;
            color: #cfe8ff;
            padding: 10px 12px;
        }
        QLineEdit, QTextEdit, QListWidget, QTableWidget, QComboBox, QProgressBar {
            background: #101a25;
            border: 1px solid #26384a;
            border-radius: 12px;
            color: #e8f1f8;
            selection-background-color: %3;
        }
        QLineEdit, QComboBox {
            padding: 10px 12px;
        }
        QTextEdit {
            padding: 10px;
        }
        QHeaderView::section {
            background: %3;
            border: 0;
            color: #edf6ff;
            padding: 9px;
        }
        QTableWidget::item {
            padding: 8px;
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
            border-color: %2;
        }
        QPushButton:pressed {
            background: #183047;
        }
        QStatusBar {
            background: #0b1118;
            color: #aebdca;
        }
        QMenuBar {
            background: %1;
            color: #dfe7ef;
        }
        QWidget#TopBar {
            background: #101a25;
            border: 1px solid #26384a;
            border-radius: 18px;
        }
    )").arg(bg, accent, accentDark));
}

void MainWindow::enterBattleLoading(const QString& battleId) {
    if (battleId.isEmpty()) {
        return;
    }
    if (loadingBattleId_ == battleId && stack_->currentIndex() != 2) {
        return;
    }
    loadingBattleId_ = battleId;
    session_.battleId = battleId;
    session_.state = ConnectionState::InRoom;
    showLoadingThenBattle(battleId);
}

void MainWindow::enterBattle(const QString& battleId) {
    if (battleId.isEmpty()) {
        return;
    }
    if (session_.battleId == battleId && session_.state == ConnectionState::InBattle &&
        stack_->currentIndex() == 2) {
        return;
    }
    loadingBattleId_.clear();
    session_.battleId = battleId;
    session_.state = ConnectionState::InBattle;
    showBattle();
}

void MainWindow::showLoadingThenBattle(const QString& battleId) {
    auto* loading = new QWidget(this);
    auto* layout = new QVBoxLayout(loading);
    layout->setContentsMargins(56, 56, 56, 56);
    layout->setSpacing(18);
    auto* title = new QLabel("正在同步进入战斗", loading);
    title->setObjectName("PageTitle");
    auto* hint = new QLabel(QString("Battle: %1\nRoom: %2\n玩家: %3\n所有成员会先停留在这里，等待服务端完成出生点、道具和首帧同步。")
                                .arg(battleId, session_.roomId, session_.userId),
                            loading);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    auto* progress = new QProgressBar(loading);
    progress->setRange(0, 100);
    progress->setValue(10);
    layout->addStretch(1);
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addWidget(progress);
    layout->addStretch(2);

    const int loadingIndex = stack_->addWidget(loading);
    showPage(loadingIndex);
    setStatus("战斗加载中：" + battleId);

    auto* timer = new QTimer(loading);
    timer->setInterval(80);
    connect(timer, &QTimer::timeout, this, [this, progress, loading, timer, battleId]() {
        progress->setValue(std::min(96, progress->value() + 7));
        if (session_.battleId == battleId && session_.state == ConnectionState::InBattle) {
            progress->setValue(100);
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
            if (!loadingBattleId_.isEmpty()) {
                loadingBattleId_.clear();
                showBattle();
            }
        }
    }

    if (!snapshot.finished) {
        return;
    }

    loadingBattleId_.clear();
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
    showPage(3);
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

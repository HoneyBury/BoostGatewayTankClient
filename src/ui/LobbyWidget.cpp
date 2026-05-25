#include "ui/LobbyWidget.h"

#include "tank/TankProtocol.h"

#include <QComboBox>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>

namespace bgtc {

LobbyWidget::LobbyWidget(AppConfig config, ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), config_(std::move(config)), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("大厅", this);
    title->setObjectName("PageTitle");
    lobbyState_ = new QLabel("浏览房间、创建房间或直接加入匹配。", this);
    lobbyState_->setObjectName("StatePill");
    lobbyState_->setWordWrap(true);
    themeBox_ = new QComboBox(this);
    themeBox_->addItems({"霓虹蓝", "沙漠橙", "森林绿"});
    header->addWidget(title);
    header->addWidget(lobbyState_, 1);
    header->addWidget(new QLabel("主题", this));
    header->addWidget(themeBox_);
    layout->addLayout(header);

    auto* hint = new QLabel("大厅会自动刷新房间；匹配面板支持模式和 MMR 配置，匹配成功后会自动进房并同步进入战斗。", this);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* actionRow = new QHBoxLayout();
    roomEdit_ = new QLineEdit(config_.defaultRoom, this);
    roomEdit_->setPlaceholderText("输入 room_id 创建或加入");
    auto* createButton = new QPushButton("创建房间", this);
    auto* refreshButton = new QPushButton("刷新列表", this);
    auto* leaderboardButton = new QPushButton("排行榜", this);
    auto* matchmakingPanelButton = new QPushButton("匹配面板", this);
    actionRow->addWidget(roomEdit_, 1);
    actionRow->addWidget(createButton);
    actionRow->addWidget(refreshButton);
    actionRow->addWidget(leaderboardButton);
    actionRow->addWidget(matchmakingPanelButton);
    layout->addLayout(actionRow);

    auto* matchmakingRow = new QHBoxLayout();
    modeBox_ = new QComboBox(this);
    modeBox_->addItems({"1v1", "2v2", "4v4"});
    modeBox_->setCurrentText(session_.matchMode);
    mmrBox_ = new QSpinBox(this);
    mmrBox_->setRange(0, 5000);
    mmrBox_->setSingleStep(50);
    mmrBox_->setValue(static_cast<int>(session_.matchmakingMmr));
    auto* matchJoinButton = new QPushButton("开始匹配", this);
    auto* matchLeaveButton = new QPushButton("取消匹配", this);
    matchmakingRow->addWidget(new QLabel("匹配模式", this));
    matchmakingRow->addWidget(modeBox_);
    matchmakingRow->addWidget(new QLabel("MMR", this));
    matchmakingRow->addWidget(mmrBox_);
    matchmakingRow->addWidget(matchJoinButton);
    matchmakingRow->addWidget(matchLeaveButton);
    matchmakingRow->addStretch(1);
    layout->addLayout(matchmakingRow);

    roomTable_ = new QTableWidget(this);
    roomTable_->setColumnCount(7);
    roomTable_->setHorizontalHeaderLabels({"房间", "房主", "人数", "已准备", "状态", "战斗", "操作"});
    roomTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    roomTable_->verticalHeader()->setVisible(false);
    roomTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    roomTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    roomTable_->setAlternatingRowColors(true);
    layout->addWidget(roomTable_, 1);

    log_ = new QTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumHeight(120);
    log_->setPlaceholderText("大厅事件和错误提示会显示在这里。");
    layout->addWidget(log_);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(2000);
    connect(refreshTimer_, &QTimer::timeout, this, [this]() {
        refreshRoomList();
        refreshMatchStatus();
        refreshLobbyStateCard();
    });
    refreshTimer_->start();

    connect(createButton, &QPushButton::clicked, this, &LobbyWidget::createRoom);
    connect(refreshButton, &QPushButton::clicked, this, &LobbyWidget::refreshRoomList);
    connect(leaderboardButton, &QPushButton::clicked, this, &LobbyWidget::leaderboardRequested);
    connect(matchmakingPanelButton, &QPushButton::clicked, this, &LobbyWidget::matchmakingRequested);
    connect(matchJoinButton, &QPushButton::clicked, this, &LobbyWidget::joinMatchmaking);
    connect(matchLeaveButton, &QPushButton::clicked, this, &LobbyWidget::leaveMatchmaking);
    connect(themeBox_, &QComboBox::currentTextChanged, this, &LobbyWidget::themeChanged);
    connect(modeBox_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        session_.matchMode = text;
        session_.lastMatchNote = QString("模式已切换到 %1。").arg(text);
        refreshLobbyStateCard();
    });
    connect(mmrBox_, &QSpinBox::valueChanged, this, [this](int value) {
        session_.matchmakingMmr = value;
        if (!session_.matchmakingQueued) {
            session_.lastMatchNote = QString("当前匹配 MMR：%1。").arg(value);
            refreshLobbyStateCard();
        }
    });
    connect(roomTable_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        const auto roomId = roomTable_->item(row, 0) != nullptr ? roomTable_->item(row, 0)->text() : QString{};
        if (!roomId.isEmpty()) {
            roomEdit_->setText(roomId);
            joinRoom(roomId);
        }
    });
    connect(&gateway_, &GatewayClient::pushReceived, this, &LobbyWidget::appendLog);
    connect(&gateway_, &GatewayClient::matchFoundPush, this, [this](const MatchFoundState& match) {
        const auto roomId = QString::fromStdString(match.roomId);
        if (roomId.isEmpty()) {
            return;
        }
        session_.matchId = QString::fromStdString(match.matchId);
        session_.matchMode = QString::fromStdString(match.mode);
        session_.matchmakingQueued = false;
        session_.matchmakingQueuedAtMs = 0;
        session_.lastMatchNote = "匹配成功，房间与战斗正在同步。";
        roomEdit_->setText(roomId);
        session_.roomId = roomId;
        session_.state = ConnectionState::InRoom;
        appendLog(QString("匹配成功：%1，模式 %2，已进入房间 %3。")
                      .arg(QString::fromStdString(match.matchId),
                           QString::fromStdString(match.mode),
                           roomId));
        refreshLobbyStateCard();
        emit matchRoomReceived(roomId);
        emit roomEntered();
    });

    refreshRoomList();
    refreshLobbyStateCard();
}

void LobbyWidget::createRoom() {
    const auto roomId = roomEdit_->text().trimmed();
    if (roomId.isEmpty()) {
        QMessageBox::warning(this, "创建房间失败", "房间号不能为空。");
        return;
    }

    QString error;
    if (!gateway_.createRoom(roomId, &error)) {
        appendLog("创建房间失败：" + error);
        QMessageBox::warning(this, "创建房间失败", error);
        return;
    }
    session_.roomId = roomId;
    session_.ready = false;
    session_.state = ConnectionState::InRoom;
    session_.lastMatchNote = "已创建房间，可以在房间页准备并开始战斗。";
    appendLog("创建房间成功：" + roomId);
    emit roomEntered();
}

void LobbyWidget::joinRoom(const QString& roomId) {
    const auto targetRoomId = roomId.trimmed();
    if (targetRoomId.isEmpty()) {
        QMessageBox::warning(this, "加入房间失败", "请先选择或输入房间号。");
        return;
    }

    QString error;
    if (!gateway_.joinRoom(targetRoomId, &error)) {
        appendLog("加入房间失败：" + error);
        QMessageBox::warning(this, "加入房间失败", error);
        return;
    }
    session_.roomId = targetRoomId;
    session_.ready = false;
    session_.state = ConnectionState::InRoom;
    session_.lastMatchNote = QString("已加入房间 %1。").arg(targetRoomId);
    appendLog("加入房间成功：" + targetRoomId);
    emit roomEntered();
}

void LobbyWidget::joinMatchmaking() {
    session_.matchMode = modeBox_->currentText();
    session_.matchmakingMmr = mmrBox_->value();
    QString error;
    const auto body = gateway_.joinMatchmaking(session_.userId,
                                               session_.matchmakingMmr,
                                               session_.matchMode,
                                               &error);
    if (!error.isEmpty()) {
        appendLog("加入匹配失败：" + error);
        QMessageBox::warning(this, "加入匹配失败", error);
        return;
    }
    session_.matchmakingQueued = true;
    session_.matchId.clear();
    session_.matchmakingQueuedAtMs = QDateTime::currentMSecsSinceEpoch();
    session_.lastMatchNote = "已进入匹配队列，系统会自动为你寻找合适对局。";
    refreshLobbyStateCard();
    appendLog("匹配已加入：" + body);
}

void LobbyWidget::leaveMatchmaking() {
    QString error;
    const auto body = gateway_.leaveMatchmaking(session_.userId, session_.matchMode, &error);
    if (!error.isEmpty()) {
        appendLog("取消匹配失败：" + error);
        QMessageBox::warning(this, "取消匹配失败", error);
        return;
    }
    session_.matchmakingQueued = false;
    session_.matchId.clear();
    session_.matchmakingQueuedAtMs = 0;
    session_.lastMatchNote = "匹配已取消，可以继续浏览大厅。";
    refreshLobbyStateCard();
    appendLog("匹配已取消：" + body);
}

void LobbyWidget::refreshMatchStatus() {
    QString error;
    const auto body = gateway_.queryMatchmakingStatus(session_.userId, session_.matchMode, &error);
    if (!error.isEmpty() || body.isEmpty()) {
        return;
    }
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    const auto root = doc.object();
    if (root.value("matched").toBool(false)) {
        session_.matchmakingQueued = false;
        session_.matchId = root.value("match_id").toString();
        session_.matchmakingQueuedAtMs = 0;
        session_.lastMatchNote = "匹配成功，房间和战斗正在同步创建。";
        refreshLobbyStateCard();
        return;
    }

    const auto queueSize = root.value("queue_size").toInt(0);
    session_.matchmakingQueued = queueSize > 0 && session_.roomId.isEmpty();
    if (session_.matchmakingQueued) {
        session_.lastMatchNote = queueSize == 1
            ? "当前只有你在队列中，正在等待下一位玩家。"
            : "队列人数已增长，正在拼装完整对局。";
    }
    refreshLobbyStateCard(queueSize);
}

void LobbyWidget::refreshRoomList() {
    QString error;
    const auto body = gateway_.queryRoomList(1, 50, {}, &error);
    if (!error.isEmpty()) {
        roomTable_->setRowCount(0);
        if (!session_.matchmakingQueued && session_.matchId.isEmpty()) {
            lobbyState_->setText("房间列表刷新失败，请确认服务端已启动。");
        }
        appendLog("刷新房间列表失败：" + error);
        return;
    }
    renderRoomList(body);
}

void LobbyWidget::renderRoomList(const QString& body) {
    roomTable_->setRowCount(0);
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (!doc.isObject()) {
        if (!session_.matchmakingQueued && session_.matchId.isEmpty()) {
            lobbyState_->setText("房间列表返回无法解析。");
        }
        appendLog("房间列表返回无法解析：" + body.left(160));
        return;
    }

    const auto rooms = doc.object().value("rooms").toArray();
    if (!session_.matchmakingQueued && session_.matchId.isEmpty() && session_.roomId.isEmpty()) {
        session_.lastMatchNote = QString("当前房间数：%1，可直接加入或创建。").arg(rooms.size());
        refreshLobbyStateCard();
    }
    if (rooms.isEmpty()) {
        roomTable_->setRowCount(1);
        setRoomCell(0, 0, "暂无房间");
        setRoomCell(0, 1, "可以创建一个新房间");
        return;
    }

    roomTable_->setRowCount(rooms.size());
    for (int row = 0; row < rooms.size(); ++row) {
        const auto room = rooms.at(row).toObject();
        const auto roomId = room.value("room_id").toString(room.value("id").toString());
        const auto owner = room.value("owner_user_id").toString(room.value("owner").toString("unknown"));
        const auto status = room.value("status").toString("unknown");
        const auto membersArray = room.value("members").toArray();
        int readyCount = 0;
        for (const auto& memberValue : membersArray) {
            if (memberValue.toObject().value("ready").toBool(false)) {
                ++readyCount;
            }
        }
        const auto activeBattle = room.value("active_battle_id").toString();
        setRoomCell(row, 0, roomId);
        setRoomCell(row, 1, owner);
        setRoomCell(row, 2, QString::number(membersArray.size()));
        setRoomCell(row, 3, QString::number(readyCount));
        setRoomCell(row, 4, status);
        setRoomCell(row, 5, activeBattle.isEmpty() ? "等待中" : "战斗中");

        auto* joinButton = new QPushButton("加入", roomTable_);
        joinButton->setProperty("roomId", roomId);
        joinButton->setMinimumHeight(34);
        joinButton->setStyleSheet("QPushButton{background:#0f766e;border:0;border-radius:17px;padding:6px 14px;font-weight:700;}"
                                  "QPushButton:hover{background:#14b8a6;}");
        connect(joinButton, &QPushButton::clicked, this, [this, joinButton]() {
            const auto roomId = joinButton->property("roomId").toString();
            roomEdit_->setText(roomId);
            joinRoom(roomId);
        });
        roomTable_->setCellWidget(row, 6, joinButton);
    }
}

void LobbyWidget::refreshLobbyStateCard(int queueSize) {
    if (!session_.matchId.isEmpty()) {
        lobbyState_->setText(QString("匹配完成：%1\n模式：%2\n%3")
                                 .arg(session_.matchId,
                                      session_.matchMode,
                                      session_.lastMatchNote.isEmpty() ? "正在同步房间与战斗。" : session_.lastMatchNote));
        return;
    }

    if (session_.matchmakingQueued) {
        const auto elapsedMs = session_.matchmakingQueuedAtMs > 0
            ? QDateTime::currentMSecsSinceEpoch() - session_.matchmakingQueuedAtMs
            : 0;
        const auto elapsedSec = std::max<qint64>(0, elapsedMs / 1000);
        lobbyState_->setText(QString("排队中\n模式：%1    MMR：%2    已等待：%3 秒\n队列人数：%4\n%5")
                                 .arg(session_.matchMode)
                                 .arg(session_.matchmakingMmr)
                                 .arg(elapsedSec)
                                 .arg(queueSize >= 0 ? queueSize : 0)
                                 .arg(session_.lastMatchNote.isEmpty() ? "系统会自动匹配并把你拉进房间。" : session_.lastMatchNote));
        return;
    }

    if (!session_.lastMatchNote.isEmpty()) {
        lobbyState_->setText(session_.lastMatchNote);
        return;
    }
    lobbyState_->setText("浏览房间、创建房间或直接加入匹配。");
}

void LobbyWidget::appendLog(const QString& text) {
    if (log_ != nullptr) {
        log_->append(text);
    }
}

void LobbyWidget::setRoomCell(int row, int column, const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    roomTable_->setItem(row, column, item);
}

}  // namespace bgtc

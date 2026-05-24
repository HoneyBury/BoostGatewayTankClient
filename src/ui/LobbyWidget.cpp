#include "ui/LobbyWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bgtc {

LobbyWidget::LobbyWidget(AppConfig config, ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), config_(std::move(config)), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(14);

    auto* title = new QLabel("房间大厅", this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto* hint = new QLabel("输入房间 ID 后创建或加入房间；准备后由房主开始战斗。", this);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    roomState_ = new QLabel("大厅：尚未进入房间", this);
    roomState_->setObjectName("StatePill");
    layout->addWidget(roomState_);

    capabilityState_ = new QLabel(
        "当前可用：创建、加入、离开、准备、开始战斗。房间列表/详情/房主管理仍等待服务端 SDK API。",
        this);
    capabilityState_->setObjectName("PageHint");
    capabilityState_->setWordWrap(true);
    layout->addWidget(capabilityState_);

    roomEdit_ = new QLineEdit(config_.defaultRoom, this);
    roomEdit_->setPlaceholderText("room_id，例如 default-room");
    layout->addWidget(roomEdit_);

    auto* row = new QGridLayout();
    row->setHorizontalSpacing(10);
    row->setVerticalSpacing(10);
    auto* listButton = new QPushButton("刷新房间列表", this);
    auto* createButton = new QPushButton("创建房间", this);
    auto* joinButton = new QPushButton("加入房间", this);
    auto* readyButton = new QPushButton("准备", this);
    auto* unreadyButton = new QPushButton("取消准备", this);
    auto* startButton = new QPushButton("开始战斗", this);
    auto* leaveButton = new QPushButton("离开房间", this);
    auto* adminButton = new QPushButton("房主管理", this);
    auto* leaderboardButton = new QPushButton("排行榜", this);

    row->addWidget(createButton, 0, 0);
    row->addWidget(joinButton, 0, 1);
    row->addWidget(readyButton, 0, 2);
    row->addWidget(startButton, 0, 3);
    row->addWidget(unreadyButton, 1, 0);
    row->addWidget(leaveButton, 1, 1);
    row->addWidget(listButton, 1, 2);
    row->addWidget(adminButton, 1, 3);
    row->addWidget(leaderboardButton, 1, 4);
    row->setColumnStretch(4, 1);
    layout->addLayout(row);

    roomList_ = new QListWidget(this);
    roomList_->addItem("操作提示：手动输入 room_id 后可以创建或加入房间。");
    roomList_->addItem("能力提示：房间列表、房间详情、邀请/踢人将在服务端 API 可用后接入。");
    layout->addWidget(roomList_);

    log_ = new QTextEdit(this);
    log_->setReadOnly(true);
    log_->setPlaceholderText("房间事件、推送消息和错误提示会显示在这里。");
    layout->addWidget(log_);

    connect(listButton, &QPushButton::clicked, this, &LobbyWidget::showUnsupportedRoomList);
    connect(createButton, &QPushButton::clicked, this, &LobbyWidget::createRoom);
    connect(joinButton, &QPushButton::clicked, this, &LobbyWidget::joinRoom);
    connect(readyButton, &QPushButton::clicked, this, &LobbyWidget::setReady);
    connect(unreadyButton, &QPushButton::clicked, this, &LobbyWidget::unsetReady);
    connect(startButton, &QPushButton::clicked, this, &LobbyWidget::startBattle);
    connect(leaveButton, &QPushButton::clicked, this, &LobbyWidget::leaveRoom);
    connect(adminButton, &QPushButton::clicked, this, &LobbyWidget::showUnsupportedRoomAdmin);
    connect(leaderboardButton, &QPushButton::clicked, this, &LobbyWidget::refreshLeaderboard);
    connect(&gateway_, &GatewayClient::pushReceived, this, &LobbyWidget::appendLog);
}

void LobbyWidget::createRoom() {
    QString error;
    const auto roomId = roomEdit_->text();
    if (gateway_.createRoom(roomId, &error)) {
        session_.roomId = roomId;
        session_.state = ConnectionState::InRoom;
        refreshRoomSummary();
        appendLog("创建房间成功：" + roomId + "，请点击“准备”。");
    } else {
        appendLog("创建房间失败：" + error);
    }
}

void LobbyWidget::joinRoom() {
    QString error;
    const auto roomId = roomEdit_->text();
    if (gateway_.joinRoom(roomId, &error)) {
        session_.roomId = roomId;
        session_.state = ConnectionState::InRoom;
        refreshRoomSummary();
        appendLog("加入房间成功：" + roomId + "，请确认准备状态。");
    } else {
        appendLog("加入房间失败：" + error);
    }
}

void LobbyWidget::leaveRoom() {
    if (!requireRoom("离开房间")) {
        return;
    }
    QString error;
    if (gateway_.leaveRoom(session_.roomId, &error)) {
        appendLog("离开房间：" + session_.roomId);
        session_.roomId.clear();
        session_.ready = false;
        session_.state = ConnectionState::InLobby;
        refreshRoomSummary();
    } else {
        appendLog("离开房间失败：" + error);
    }
}

void LobbyWidget::setReady() {
    if (!requireRoom("准备")) {
        return;
    }
    QString error;
    if (gateway_.setReady(true, &error)) {
        session_.ready = true;
        refreshRoomSummary();
        appendLog("已准备，等待房主开始战斗。");
    } else {
        appendLog("准备失败：" + error);
    }
}

void LobbyWidget::unsetReady() {
    if (!requireRoom("取消准备")) {
        return;
    }
    QString error;
    if (gateway_.setReady(false, &error)) {
        session_.ready = false;
        refreshRoomSummary();
        appendLog("已取消准备，可以调整后再次准备。");
    } else {
        appendLog("取消准备失败：" + error);
    }
}

void LobbyWidget::startBattle() {
    if (!requireRoom("开始战斗")) {
        return;
    }
    QString error;
    QString battleId;
    if (gateway_.startBattle(session_.roomId, &battleId, &error)) {
        appendLog("战斗开始：" + battleId);
        emit battleStarted(battleId);
    } else {
        appendLog("开始战斗失败：" + error);
    }
}

void LobbyWidget::refreshLeaderboard() {
    QString error;
    const auto body = gateway_.queryLeaderboardTop(10, &error);
    appendLog(error.isEmpty() ? "排行榜：" + body : "排行榜查询失败：" + error);
    emit leaderboardRequested();
}

void LobbyWidget::showUnsupportedRoomList() {
    roomList_->clear();
    roomList_->addItem("房间列表暂不可用：等待服务端 SDK 暴露 list_rooms/page/filter API。");
    roomList_->addItem("当前可用路径：复制或输入 room_id 后创建/加入房间。");
    appendLog(gateway_.unsupportedFeatureMessage("房间列表/分页/过滤"));
}

void LobbyWidget::showUnsupportedRoomAdmin() {
    appendLog(gateway_.unsupportedFeatureMessage("踢人/转让房主/房间详情"));
}

void LobbyWidget::appendLog(const QString& text) {
    log_->append(text);
}

void LobbyWidget::refreshRoomSummary() {
    if (session_.roomId.isEmpty()) {
        roomState_->setText("大厅：尚未进入房间");
        return;
    }
    const auto readyText = session_.ready ? "已准备" : "未准备";
    const auto battleText = session_.battleId.isEmpty() ? "未开始战斗" : "战斗：" + session_.battleId;
    roomState_->setText(QString("当前房间：%1    状态：%2    %3")
                            .arg(session_.roomId, readyText, battleText));
}

bool LobbyWidget::requireRoom(const QString& action) {
    if (!session_.roomId.isEmpty()) {
        return true;
    }
    appendLog(action + "失败：请先创建或加入房间。");
    return false;
}

}  // namespace bgtc

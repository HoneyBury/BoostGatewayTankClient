#include "ui/LobbyWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
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
        "当前可用：创建、加入、离开、准备、开始战斗、房间列表、房间详情、踢出成员、转让房主。",
        this);
    capabilityState_->setObjectName("PageHint");
    capabilityState_->setWordWrap(true);
    layout->addWidget(capabilityState_);

    roomEdit_ = new QLineEdit(config_.defaultRoom, this);
    roomEdit_->setPlaceholderText("room_id，例如 default-room");
    layout->addWidget(roomEdit_);

    adminUserEdit_ = new QLineEdit(this);
    adminUserEdit_->setPlaceholderText("房主管理目标 user_id，例如 player_2");
    layout->addWidget(adminUserEdit_);

    auto* row = new QGridLayout();
    row->setHorizontalSpacing(10);
    row->setVerticalSpacing(10);
    auto* listButton = new QPushButton("刷新房间列表", this);
    auto* detailButton = new QPushButton("房间详情", this);
    auto* createButton = new QPushButton("创建房间", this);
    auto* joinButton = new QPushButton("加入房间", this);
    auto* readyButton = new QPushButton("准备", this);
    auto* unreadyButton = new QPushButton("取消准备", this);
    auto* startButton = new QPushButton("开始战斗", this);
    auto* leaveButton = new QPushButton("离开房间", this);
    auto* kickButton = new QPushButton("踢出成员", this);
    auto* transferButton = new QPushButton("转让房主", this);
    auto* leaderboardButton = new QPushButton("排行榜", this);

    row->addWidget(createButton, 0, 0);
    row->addWidget(joinButton, 0, 1);
    row->addWidget(readyButton, 0, 2);
    row->addWidget(startButton, 0, 3);
    row->addWidget(unreadyButton, 1, 0);
    row->addWidget(leaveButton, 1, 1);
    row->addWidget(listButton, 1, 2);
    row->addWidget(detailButton, 1, 3);
    row->addWidget(kickButton, 1, 4);
    row->addWidget(transferButton, 1, 5);
    row->addWidget(leaderboardButton, 1, 6);
    row->setColumnStretch(4, 1);
    layout->addLayout(row);

    roomList_ = new QListWidget(this);
    roomList_->setContextMenuPolicy(Qt::CustomContextMenu);
    roomList_->addItem("点击“刷新房间列表”从 gateway 查询真实房间。");
    roomList_->addItem("选中房间后会自动填入 room_id，可直接加入或查看详情。");
    layout->addWidget(roomList_);

    log_ = new QTextEdit(this);
    log_->setReadOnly(true);
    log_->setPlaceholderText("房间事件、推送消息和错误提示会显示在这里。");
    layout->addWidget(log_);

    connect(listButton, &QPushButton::clicked, this, &LobbyWidget::refreshRoomList);
    connect(detailButton, &QPushButton::clicked, this, &LobbyWidget::refreshRoomDetail);
    connect(createButton, &QPushButton::clicked, this, &LobbyWidget::createRoom);
    connect(joinButton, &QPushButton::clicked, this, &LobbyWidget::joinRoom);
    connect(readyButton, &QPushButton::clicked, this, &LobbyWidget::setReady);
    connect(unreadyButton, &QPushButton::clicked, this, &LobbyWidget::unsetReady);
    connect(startButton, &QPushButton::clicked, this, &LobbyWidget::startBattle);
    connect(leaveButton, &QPushButton::clicked, this, &LobbyWidget::leaveRoom);
    connect(kickButton, &QPushButton::clicked, this, &LobbyWidget::kickRoomMember);
    connect(transferButton, &QPushButton::clicked, this, &LobbyWidget::transferRoomOwner);
    connect(leaderboardButton, &QPushButton::clicked, this, &LobbyWidget::refreshLeaderboard);
    connect(roomList_, &QListWidget::itemClicked, this, &LobbyWidget::selectRoomFromList);
    connect(roomList_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        auto* item = roomList_->itemAt(pos);
        if (item == nullptr || item->data(Qt::UserRole).toString().isEmpty()) {
            return;
        }
        roomList_->setCurrentItem(item);
        selectRoomFromList(item);
        QMenu menu(this);
        auto* joinAction = menu.addAction("加入房间");
        if (menu.exec(roomList_->viewport()->mapToGlobal(pos)) == joinAction) {
            joinRoom();
        }
    });
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
        QMessageBox::warning(this, "创建房间失败", error);
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
        QMessageBox::warning(this, "加入房间失败", error);
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
        QMessageBox::warning(this, "离开房间失败", error);
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
        QMessageBox::warning(this, "准备失败", error);
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
        QMessageBox::warning(this, "取消准备失败", error);
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
        QMessageBox::warning(this, "开始战斗失败", error);
    }
}

void LobbyWidget::refreshLeaderboard() {
    QString error;
    const auto body = gateway_.queryLeaderboardTop(10, &error);
    appendLog(error.isEmpty() ? "排行榜：" + body : "排行榜查询失败：" + error);
    emit leaderboardRequested();
}

void LobbyWidget::refreshRoomList() {
    QString error;
    const auto body = gateway_.queryRoomList(1, 50, {}, &error);
    if (!error.isEmpty()) {
        appendLog("刷新房间列表失败：" + error);
        roomList_->clear();
        roomList_->addItem("房间列表查询失败，请确认服务端 gateway/backend 已更新。");
        return;
    }
    renderRoomList(body);
    appendLog("房间列表已刷新。");
}

void LobbyWidget::refreshRoomDetail() {
    const auto roomId = roomEdit_->text().trimmed();
    if (roomId.isEmpty()) {
        appendLog("查询房间详情失败：请先输入或选择 room_id。");
        return;
    }
    QString error;
    const auto body = gateway_.queryRoomDetail(roomId, &error);
    appendLog(error.isEmpty() ? "房间详情：" + body : "查询房间详情失败：" + error);
}

void LobbyWidget::kickRoomMember() {
    if (!requireRoom("踢出成员")) {
        return;
    }
    const auto target = adminUserEdit_->text().trimmed();
    if (target.isEmpty()) {
        appendLog("踢出成员失败：请先输入目标 user_id。");
        return;
    }
    QString error;
    if (gateway_.kickRoomMember(target, &error)) {
        appendLog("已踢出成员：" + target);
        refreshRoomDetail();
    } else {
        appendLog("踢出成员失败：" + error);
    }
}

void LobbyWidget::transferRoomOwner() {
    if (!requireRoom("转让房主")) {
        return;
    }
    const auto target = adminUserEdit_->text().trimmed();
    if (target.isEmpty()) {
        appendLog("转让房主失败：请先输入目标 user_id。");
        return;
    }
    QString error;
    if (gateway_.transferRoomOwner(target, &error)) {
        appendLog("房主已转让给：" + target);
        refreshRoomDetail();
    } else {
        appendLog("转让房主失败：" + error);
    }
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

void LobbyWidget::selectRoomFromList(QListWidgetItem* item) {
    if (item == nullptr) {
        return;
    }
    const auto roomId = item->data(Qt::UserRole).toString();
    if (!roomId.isEmpty()) {
        roomEdit_->setText(roomId);
        appendLog("已选择房间：" + roomId);
    }
}

void LobbyWidget::renderRoomList(const QString& body) {
    roomList_->clear();
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        roomList_->addItem("房间列表返回无法解析：" + body.left(120));
        return;
    }

    const auto root = doc.object();
    const auto rooms = root.value("rooms").toArray();
    if (rooms.isEmpty()) {
        roomList_->addItem("暂无房间。可以创建一个新房间。");
        return;
    }

    for (const auto& value : rooms) {
        const auto room = value.toObject();
        const auto roomId = room.value("room_id").toString(room.value("id").toString());
        const auto owner = room.value("owner_user_id").toString(room.value("owner").toString("unknown"));
        const auto status = room.value("status").toString("unknown");
        const auto members = room.value("members").toArray().size();
        auto* item = new QListWidgetItem(
            QString("%1 | %2人 | owner=%3 | %4").arg(roomId).arg(members).arg(owner, status),
            roomList_);
        item->setData(Qt::UserRole, roomId);
    }
}

bool LobbyWidget::requireRoom(const QString& action) {
    if (!session_.roomId.isEmpty()) {
        return true;
    }
    appendLog(action + "失败：请先创建或加入房间。");
    return false;
}

}  // namespace bgtc

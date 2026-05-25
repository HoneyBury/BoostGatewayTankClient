#include "ui/RoomWidget.h"

#include <QComboBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace bgtc {

RoomWidget::RoomWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(14);

    auto* header = new QHBoxLayout();
    auto* title = new QLabel("房间", this);
    title->setObjectName("PageTitle");
    roomState_ = new QLabel("尚未进入房间", this);
    roomState_->setObjectName("StatePill");
    auto* themeBox = new QComboBox(this);
    themeBox->addItems({"霓虹蓝", "沙漠橙", "森林绿"});
    header->addWidget(title);
    header->addWidget(roomState_, 1);
    header->addWidget(new QLabel("主题", this));
    header->addWidget(themeBox);
    layout->addLayout(header);

    auto* hint = new QLabel("房间页会自动刷新成员状态；只要战斗创建成功，所有成员都会先进入统一加载流程。", this);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* actionRow = new QHBoxLayout();
    auto* readyButton = new QPushButton("准备", this);
    auto* unreadyButton = new QPushButton("取消准备", this);
    auto* startButton = new QPushButton("开始战斗", this);
    auto* refreshButton = new QPushButton("刷新成员", this);
    auto* returnBattleButton = new QPushButton("返回战斗", this);
    auto* leaveButton = new QPushButton("离开房间", this);
    actionRow->addWidget(readyButton);
    actionRow->addWidget(unreadyButton);
    actionRow->addWidget(startButton);
    actionRow->addWidget(refreshButton);
    actionRow->addWidget(returnBattleButton);
    actionRow->addStretch(1);
    actionRow->addWidget(leaveButton);
    layout->addLayout(actionRow);

    memberTable_ = new QTableWidget(this);
    memberTable_->setColumnCount(5);
    memberTable_->setHorizontalHeaderLabels({"玩家", "角色", "准备", "踢出", "转让房主"});
    memberTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    memberTable_->verticalHeader()->setVisible(false);
    memberTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    memberTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    memberTable_->setAlternatingRowColors(true);
    layout->addWidget(memberTable_, 1);

    log_ = new QTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumHeight(110);
    log_->setPlaceholderText("房间事件和错误提示会显示在这里。");
    layout->addWidget(log_);

    refreshTimer_ = new QTimer(this);
    refreshTimer_->setInterval(1800);
    connect(refreshTimer_, &QTimer::timeout, this, &RoomWidget::refreshRoomDetail);
    refreshTimer_->start();

    connect(readyButton, &QPushButton::clicked, this, [this]() { setReady(true); });
    connect(unreadyButton, &QPushButton::clicked, this, [this]() { setReady(false); });
    connect(startButton, &QPushButton::clicked, this, &RoomWidget::startBattle);
    connect(refreshButton, &QPushButton::clicked, this, &RoomWidget::refreshRoomDetail);
    connect(returnBattleButton, &QPushButton::clicked, this, &RoomWidget::returnToBattleRequested);
    connect(leaveButton, &QPushButton::clicked, this, &RoomWidget::leaveRoom);
    connect(themeBox, &QComboBox::currentTextChanged, this, &RoomWidget::themeChanged);
    connect(&gateway_, &GatewayClient::pushReceived, this, &RoomWidget::appendLog);
}

void RoomWidget::refreshRoomDetail() {
    if (!requireRoom("刷新房间")) {
        return;
    }
    QString error;
    const auto body = gateway_.queryRoomDetail(session_.roomId, &error);
    if (!error.isEmpty()) {
        appendLog("查询房间详情失败：" + error);
        return;
    }
    renderRoomDetail(body);
}

void RoomWidget::setReady(bool ready) {
    if (!requireRoom(ready ? "准备" : "取消准备")) {
        return;
    }
    QString error;
    if (!gateway_.setReady(ready, &error)) {
        QMessageBox::warning(this, ready ? "准备失败" : "取消准备失败", error);
        appendLog((ready ? "准备失败：" : "取消准备失败：") + error);
        return;
    }
    session_.ready = ready;
    appendLog(ready ? "已准备。" : "已取消准备。");
    refreshRoomDetail();
}

void RoomWidget::startBattle() {
    if (!requireRoom("开始战斗")) {
        return;
    }
    QString error;
    QString battleId;
    if (!gateway_.startBattle(session_.roomId, &battleId, &error)) {
        QMessageBox::warning(this, "开始战斗失败", error);
        appendLog("开始战斗失败：" + error);
        return;
    }
    session_.battleId = battleId;
    appendLog("战斗创建成功，正在等待所有成员进入统一加载界面：" + battleId);
    emit loadingRequested(battleId);
}

void RoomWidget::leaveRoom() {
    if (!requireRoom("离开房间")) {
        return;
    }
    QString error;
    const auto roomId = session_.roomId;
    if (!gateway_.leaveRoom(roomId, &error)) {
        QMessageBox::warning(this, "离开房间失败", error);
        appendLog("离开房间失败：" + error);
        return;
    }
    appendLog("已离开房间：" + roomId);
    session_.roomId.clear();
    session_.battleId.clear();
    session_.ready = false;
    session_.state = ConnectionState::InLobby;
    memberTable_->setRowCount(0);
    roomState_->setText("尚未进入房间");
    emit leftRoom();
}

void RoomWidget::kickMember(const QString& userId) {
    if (!requireRoom("踢出成员")) {
        return;
    }
    QString error;
    if (!gateway_.kickRoomMember(userId, &error)) {
        QMessageBox::warning(this, "踢出成员失败", error);
        appendLog("踢出成员失败：" + error);
        return;
    }
    appendLog("已踢出成员：" + userId);
    refreshRoomDetail();
}

void RoomWidget::transferOwner(const QString& userId) {
    if (!requireRoom("转让房主")) {
        return;
    }
    QString error;
    if (!gateway_.transferRoomOwner(userId, &error)) {
        QMessageBox::warning(this, "转让房主失败", error);
        appendLog("转让房主失败：" + error);
        return;
    }
    appendLog("房主已转让给：" + userId);
    refreshRoomDetail();
}

void RoomWidget::renderRoomDetail(const QString& body) {
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (!doc.isObject()) {
        return;
    }

    const auto room = doc.object().value("room").toObject();
    const auto owner = room.value("owner_user_id").toString();
    const auto status = room.value("status").toString("unknown");
    const auto activeBattle = room.value("active_battle_id").toString();
    const bool battleJustStarted = !activeBattle.isEmpty() && session_.battleId != activeBattle;
    session_.battleId = activeBattle;
    roomState_->setText(QString("房间：%1    房主：%2    状态：%3    %4")
                            .arg(room.value("room_id").toString(session_.roomId),
                                 owner,
                                 status,
                                 activeBattle.isEmpty() ? "未开始战斗" : "战斗：" + activeBattle));

    const auto members = room.value("members").toArray();
    memberTable_->setRowCount(members.size());
    for (int row = 0; row < members.size(); ++row) {
        const auto member = members.at(row).toObject();
        const auto userId = member.value("user_id").toString();
        const auto ready = member.value("ready").toBool(false);
        setMemberCell(row, 0, userId);
        setMemberCell(row, 1, userId == owner ? "房主" : "成员");
        setMemberCell(row, 2, ready ? "已准备" : "未准备");

        auto* kickButton = new QPushButton(userId == session_.userId ? "-" : "踢出", memberTable_);
        kickButton->setEnabled(userId != session_.userId);
        kickButton->setMinimumHeight(34);
        kickButton->setStyleSheet("QPushButton{background:#7c2d12;border:0;border-radius:17px;padding:6px 12px;font-weight:700;}"
                                  "QPushButton:hover{background:#ea580c;}");
        connect(kickButton, &QPushButton::clicked, this, [this, userId]() { kickMember(userId); });
        memberTable_->setCellWidget(row, 3, kickButton);

        auto* transferButton = new QPushButton(userId == owner ? "-" : "转让", memberTable_);
        transferButton->setEnabled(userId != owner);
        transferButton->setMinimumHeight(34);
        transferButton->setStyleSheet("QPushButton{background:#1d4ed8;border:0;border-radius:17px;padding:6px 12px;font-weight:700;}"
                                      "QPushButton:hover{background:#2563eb;}");
        connect(transferButton, &QPushButton::clicked, this, [this, userId]() { transferOwner(userId); });
        memberTable_->setCellWidget(row, 4, transferButton);
    }

    if (battleJustStarted) {
        appendLog("检测到房间战斗已创建，所有成员进入统一加载流程。");
        emit loadingRequested(activeBattle);
    }
}

void RoomWidget::setMemberCell(int row, int column, const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    memberTable_->setItem(row, column, item);
}

void RoomWidget::appendLog(const QString& text) {
    if (log_ != nullptr) {
        log_->append(text);
    }
}

bool RoomWidget::requireRoom(const QString& action) const {
    if (!session_.roomId.isEmpty()) {
        return true;
    }
    QMessageBox::warning(const_cast<RoomWidget*>(this), action + "失败", "请先在大厅创建或加入房间。");
    return false;
}

}  // namespace bgtc

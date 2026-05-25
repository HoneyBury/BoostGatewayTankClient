#include "ui/MatchmakingWidget.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace bgtc {

MatchmakingWidget::MatchmakingWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    setStyleSheet("background-color: #0f172a;");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // ── Title ──
    auto* title = new QLabel("匹配面板", this);
    title->setObjectName("PageTitle");
    title->setStyleSheet("color: #f8fafc; font-size: 22px; font-weight: 700;");
    layout->addWidget(title);

    // ── Hint ──
    auto* hint = new QLabel("选择模式和MMR后点击寻找比赛，系统会自动匹配对手并同步进入房间。", this);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    hint->setStyleSheet("color: #9fb0c2;");
    layout->addWidget(hint);

    // ── Config row (mode + MMR) ──
    auto* configRow = new QHBoxLayout();
    auto* modeLabel = new QLabel("模式", this);
    modeLabel->setStyleSheet("color: #f8fafc; font-weight: 600;");
    modeBox_ = new QComboBox(this);
    modeBox_->addItems({"1v1", "2v2", "4v4"});
    modeBox_->setCurrentText(session_.matchMode);
    modeBox_->setStyleSheet(
        "QComboBox{background:#101a25;border:1px solid #2dd4bf;border-radius:10px;"
        "color:#f8fafc;padding:8px 12px;}"
        "QComboBox:hover{border-color:#14b8a6;}");

    auto* mmrLabel = new QLabel("MMR", this);
    mmrLabel->setStyleSheet("color: #f8fafc; font-weight: 600;");
    mmrBox_ = new QSpinBox(this);
    mmrBox_->setRange(0, 5000);
    mmrBox_->setSingleStep(50);
    mmrBox_->setValue(static_cast<int>(session_.matchmakingMmr));
    mmrBox_->setStyleSheet(
        "QSpinBox{background:#101a25;border:1px solid #2dd4bf;border-radius:10px;"
        "color:#f8fafc;padding:8px 12px;}"
        "QSpinBox:hover{border-color:#14b8a6;}");

    configRow->addWidget(modeLabel);
    configRow->addWidget(modeBox_);
    configRow->addWidget(mmrLabel);
    configRow->addWidget(mmrBox_);
    configRow->addStretch(1);
    layout->addLayout(configRow);

    // ── Action row ──
    auto* actionRow = new QHBoxLayout();
    joinButton_ = new QPushButton("寻找比赛", this);
    joinButton_->setMinimumHeight(44);
    joinButton_->setStyleSheet(
        "QPushButton{background:#0f766e;border:0;border-radius:12px;"
        "color:#f8fafc;font-size:15px;font-weight:700;padding:10px 28px;}"
        "QPushButton:hover{background:#14b8a6;}");

    leaveButton_ = new QPushButton("取消匹配", this);
    leaveButton_->setMinimumHeight(44);
    leaveButton_->setStyleSheet(
        "QPushButton{background:#7c2d12;border:0;border-radius:12px;"
        "color:#f8fafc;font-size:15px;font-weight:700;padding:10px 28px;}"
        "QPushButton:hover{background:#9a3412;}");
    leaveButton_->setVisible(false);

    actionRow->addWidget(joinButton_);
    actionRow->addWidget(leaveButton_);
    actionRow->addStretch(1);
    layout->addLayout(actionRow);

    // ── Status label ──
    statusLabel_ = new QLabel("选择模式和MMR开始匹配", this);
    statusLabel_->setObjectName("StatePill");
    statusLabel_->setWordWrap(true);
    statusLabel_->setStyleSheet(
        "background:#164e63;border:1px solid #2dd4bf;border-radius:12px;"
        "color:#cfe8ff;padding:12px 16px;font-size:14px;");
    layout->addWidget(statusLabel_);

    // ── Queue info widgets (hidden initially) ──
    queueTimeLabel_ = new QLabel(this);
    queueTimeLabel_->setStyleSheet("color: #f8fafc; font-size: 14px;");
    queueTimeLabel_->setVisible(false);
    layout->addWidget(queueTimeLabel_);

    queueSizeLabel_ = new QLabel(this);
    queueSizeLabel_->setStyleSheet("color: #f8fafc; font-size: 14px;");
    queueSizeLabel_->setVisible(false);
    layout->addWidget(queueSizeLabel_);

    queueProgress_ = new QProgressBar(this);
    queueProgress_->setRange(0, 0);  // indeterminate / busy animation
    queueProgress_->setMinimumHeight(8);
    queueProgress_->setStyleSheet(
        "QProgressBar{background:#101a25;border:1px solid #2dd4bf;border-radius:4px;}"
        "QProgressBar::chunk{background:#2dd4bf;border-radius:3px;}");
    queueProgress_->setVisible(false);
    layout->addWidget(queueProgress_);

    layout->addStretch(1);

    // ── Match found overlay ──
    matchFoundOverlay_ = new QFrame(this);
    matchFoundOverlay_->setFrameShape(QFrame::StyledPanel);
    matchFoundOverlay_->setStyleSheet(
        "QFrame{background:#0f766e;border:2px solid #2dd4bf;border-radius:16px;}");
    matchFoundOverlay_->setVisible(false);
    auto* overlayLayout = new QVBoxLayout(matchFoundOverlay_);
    overlayLayout->setContentsMargins(24, 24, 24, 24);
    overlayLayout->setSpacing(14);

    matchFoundLabel_ = new QLabel("匹配成功!", matchFoundOverlay_);
    matchFoundLabel_->setStyleSheet("color: #f8fafc; font-size: 20px; font-weight: 700;");
    matchFoundLabel_->setAlignment(Qt::AlignCenter);
    overlayLayout->addWidget(matchFoundLabel_);

    matchPlayersLabel_ = new QLabel(matchFoundOverlay_);
    matchPlayersLabel_->setStyleSheet("color: #f8fafc; font-size: 14px;");
    matchPlayersLabel_->setWordWrap(true);
    overlayLayout->addWidget(matchPlayersLabel_);

    acceptMatchButton_ = new QPushButton("接受并进入房间", matchFoundOverlay_);
    acceptMatchButton_->setMinimumHeight(48);
    acceptMatchButton_->setStyleSheet(
        "QPushButton{background:#1c2f42;border:1px solid #2dd4bf;border-radius:14px;"
        "color:#f8fafc;font-size:16px;font-weight:700;padding:12px 32px;}"
        "QPushButton:hover{background:#25415a;border-color:#14b8a6;}");
    overlayLayout->addWidget(acceptMatchButton_);

    layout->addWidget(matchFoundOverlay_);

    // ── Timer ──
    pollTimer_ = new QTimer(this);

    // ── Connections ──
    connect(modeBox_, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        session_.matchMode = text;
    });
    connect(mmrBox_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        session_.matchmakingMmr = value;
    });
    connect(joinButton_, &QPushButton::clicked, this, &MatchmakingWidget::startQueue);
    connect(leaveButton_, &QPushButton::clicked, this, &MatchmakingWidget::leaveQueue);
    connect(pollTimer_, &QTimer::timeout, this, &MatchmakingWidget::pollStatus);
    connect(acceptMatchButton_, &QPushButton::clicked, this, [this]() {
        // The match data is stored in session_ by applyMatchFound
        // Emit the signal so MainWindow can navigate to the room
        MatchFoundState match;
        match.matchId = session_.matchId.toStdString();
        match.roomId = session_.roomId.toStdString();
        match.mode = session_.matchMode.toStdString();
        emit matchFound(match);
    });
    connect(&gateway_, &GatewayClient::matchFoundPush, this, &MatchmakingWidget::applyMatchFound);

    updateStatusDisplay();
}

void MatchmakingWidget::startQueue() {
    session_.matchMode = modeBox_->currentText();
    session_.matchmakingMmr = mmrBox_->value();

    QString error;
    gateway_.joinMatchmaking(session_.userId, session_.matchmakingMmr, session_.matchMode, &error);
    if (!error.isEmpty()) {
        statusLabel_->setText("加入匹配失败：" + error);
        return;
    }

    inQueue_ = true;
    session_.matchmakingQueued = true;
    session_.matchId.clear();
    queueTimer_.start();
    pollTimer_->start(2000);

    updateStatusDisplay();
}

void MatchmakingWidget::leaveQueue() {
    QString error;
    gateway_.leaveMatchmaking(session_.userId, session_.matchMode, &error);
    if (!error.isEmpty()) {
        statusLabel_->setText("取消匹配失败：" + error);
    }

    resetState();
    emit matchCancelled();
}

void MatchmakingWidget::pollStatus() {
    if (!inQueue_) {
        return;
    }

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

    // Check if matched
    if (root.value("matched").toBool(false)) {
        session_.matchmakingQueued = false;
        session_.matchId = root.value("match_id").toString();
        session_.roomId = root.value("room_id").toString();
        session_.matchMode = root.value("mode").toString(session_.matchMode);

        MatchFoundState match;
        match.matchId = session_.matchId.toStdString();
        match.roomId = session_.roomId.toStdString();
        match.mode = session_.matchMode.toStdString();
        applyMatchFound(match);
        return;
    }

    // Update queue info
    const auto queueSize = root.value("queue_size").toInt(0);
    const auto elapsedSec = queueTimer_.elapsed() / 1000;
    queueTimeLabel_->setText(QString("已等待：%1 秒").arg(elapsedSec));
    queueSizeLabel_->setText(QString("队列人数：%1").arg(queueSize > 0 ? queueSize : 1));
}

void MatchmakingWidget::applyMatchFound(const MatchFoundState& match) {
    inQueue_ = false;
    session_.matchmakingQueued = false;
    pollTimer_->stop();

    session_.matchId = QString::fromStdString(match.matchId);
    session_.roomId = QString::fromStdString(match.roomId);
    session_.matchMode = QString::fromStdString(match.mode);

    // Build players display text
    QString playersText;
    if (match.players.empty()) {
        playersText = "等待玩家信息...";
    } else {
        playersText = "玩家列表：\n";
        for (const auto& player : match.players) {
            playersText += QString("  • %1 (MMR: %2)\n")
                               .arg(QString::fromStdString(player.userId))
                               .arg(player.mmr);
        }
    }

    matchFoundLabel_->setText(QString("匹配成功!\nMatch: %1\n模式: %2")
                                  .arg(QString::fromStdString(match.matchId),
                                       QString::fromStdString(match.mode)));
    matchPlayersLabel_->setText(playersText);

    updateStatusDisplay();
}

void MatchmakingWidget::updateStatusDisplay() {
    const bool matched = !session_.matchId.isEmpty() && !inQueue_;

    joinButton_->setVisible(!inQueue_ && !matched);
    leaveButton_->setVisible(inQueue_);
    queueTimeLabel_->setVisible(inQueue_);
    queueSizeLabel_->setVisible(inQueue_);
    queueProgress_->setVisible(inQueue_);
    matchFoundOverlay_->setVisible(matched);

    if (matched) {
        modeBox_->setEnabled(false);
        mmrBox_->setEnabled(false);
        statusLabel_->setText("匹配已完成，请点击下方按钮进入房间。");
    } else if (inQueue_) {
        modeBox_->setEnabled(false);
        mmrBox_->setEnabled(false);
        statusLabel_->setText("正在搜索对手...");
    } else {
        modeBox_->setEnabled(true);
        mmrBox_->setEnabled(true);
        statusLabel_->setText("选择模式和MMR开始匹配");
    }
}

void MatchmakingWidget::resetState() {
    inQueue_ = false;
    session_.matchmakingQueued = false;
    session_.matchId.clear();
    pollTimer_->stop();

    modeBox_->setEnabled(true);
    mmrBox_->setEnabled(true);
    updateStatusDisplay();
}

}  // namespace bgtc

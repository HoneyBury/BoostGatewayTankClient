#include "ui/LobbyWidget.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bgtc {

LobbyWidget::LobbyWidget(AppConfig config, ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), config_(std::move(config)), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    roomEdit_ = new QLineEdit(config_.defaultRoom, this);
    layout->addWidget(roomEdit_);

    auto* row = new QHBoxLayout();
    auto* createButton = new QPushButton("Create Room", this);
    auto* joinButton = new QPushButton("Join Room", this);
    auto* readyButton = new QPushButton("Ready", this);
    auto* startButton = new QPushButton("Start Battle", this);
    auto* leaveButton = new QPushButton("Leave Room", this);
    auto* leaderboardButton = new QPushButton("Leaderboard", this);
    row->addWidget(createButton);
    row->addWidget(joinButton);
    row->addWidget(readyButton);
    row->addWidget(startButton);
    row->addWidget(leaveButton);
    row->addWidget(leaderboardButton);
    layout->addLayout(row);

    log_ = new QTextEdit(this);
    log_->setReadOnly(true);
    layout->addWidget(log_);

    connect(createButton, &QPushButton::clicked, this, &LobbyWidget::createRoom);
    connect(joinButton, &QPushButton::clicked, this, &LobbyWidget::joinRoom);
    connect(readyButton, &QPushButton::clicked, this, &LobbyWidget::setReady);
    connect(startButton, &QPushButton::clicked, this, &LobbyWidget::startBattle);
    connect(leaveButton, &QPushButton::clicked, this, &LobbyWidget::leaveRoom);
    connect(leaderboardButton, &QPushButton::clicked, this, &LobbyWidget::refreshLeaderboard);
    connect(&gateway_, &GatewayClient::pushReceived, this, &LobbyWidget::appendLog);
}

void LobbyWidget::createRoom() {
    QString error;
    const auto roomId = roomEdit_->text();
    if (gateway_.createRoom(roomId, &error)) {
        session_.roomId = roomId;
        session_.state = ConnectionState::InRoom;
        appendLog("Created room: " + roomId);
    } else {
        appendLog("Create room failed: " + error);
    }
}

void LobbyWidget::joinRoom() {
    QString error;
    const auto roomId = roomEdit_->text();
    if (gateway_.joinRoom(roomId, &error)) {
        session_.roomId = roomId;
        session_.state = ConnectionState::InRoom;
        appendLog("Joined room: " + roomId);
    } else {
        appendLog("Join room failed: " + error);
    }
}

void LobbyWidget::leaveRoom() {
    QString error;
    if (gateway_.leaveRoom(session_.roomId, &error)) {
        appendLog("Left room: " + session_.roomId);
        session_.roomId.clear();
        session_.state = ConnectionState::InLobby;
    } else {
        appendLog("Leave room failed: " + error);
    }
}

void LobbyWidget::setReady() {
    QString error;
    if (gateway_.setReady(true, &error)) {
        appendLog("Ready.");
    } else {
        appendLog("Ready failed: " + error);
    }
}

void LobbyWidget::startBattle() {
    QString error;
    QString battleId;
    if (gateway_.startBattle(session_.roomId, &battleId, &error)) {
        appendLog("Battle started: " + battleId);
        emit battleStarted(battleId);
    } else {
        appendLog("Start battle failed: " + error);
    }
}

void LobbyWidget::refreshLeaderboard() {
    QString error;
    const auto body = gateway_.queryLeaderboardTop(10, &error);
    appendLog(error.isEmpty() ? "Leaderboard: " + body : "Leaderboard failed: " + error);
}

void LobbyWidget::appendLog(const QString& text) {
    log_->append(text);
}

}  // namespace bgtc

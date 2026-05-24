#include "sdk/GatewayClient.h"

#include <QMetaObject>

#include <chrono>

namespace bgtc {
namespace {

constexpr auto kDefaultTimeout = std::chrono::seconds(5);

std::string toStdString(const QString& value) {
    return value.toUtf8().toStdString();
}

}  // namespace

GatewayClient::GatewayClient(QObject* parent)
    : QObject(parent), client_(std::make_unique<boost_gateway::sdk::SdkClient>()) {
    installCallbacks();
}

GatewayClient::~GatewayClient() {
    disconnectFromGateway();
}

bool GatewayClient::connectToGateway(const AppConfig& config, QString* errorMessage) {
    if (!client_->connect(toStdString(config.host), config.port, kDefaultTimeout)) {
        if (errorMessage) {
            *errorMessage = "connect failed";
        }
        return false;
    }
    return true;
}

void GatewayClient::disconnectFromGateway() {
    if (client_ && client_->is_connected()) {
        client_->stop_heartbeat();
        client_->disconnect();
    }
}

bool GatewayClient::login(const QString& userId, const QString& token, QString* errorMessage) {
    const auto result = client_->login(toStdString(userId), toStdString(token), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return false;
    }
    client_->start_heartbeat();
    return true;
}

bool GatewayClient::createRoom(const QString& roomId, QString* errorMessage) {
    const auto result = client_->create_room(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return false;
    }
    return true;
}

bool GatewayClient::joinRoom(const QString& roomId, QString* errorMessage) {
    const auto result = client_->join_room(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return false;
    }
    return true;
}

bool GatewayClient::leaveRoom(const QString& roomId, QString* errorMessage) {
    const auto result = client_->leave_room(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return false;
    }
    return true;
}

bool GatewayClient::setReady(bool ready, QString* errorMessage) {
    const auto result = client_->set_ready(ready, kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return false;
    }
    return true;
}

bool GatewayClient::startBattle(const QString& roomId, QString* battleId, QString* errorMessage) {
    const auto result = client_->start_battle(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return false;
    }
    if (battleId) {
        *battleId = QString::fromStdString(result.battle_id);
    }
    return true;
}

bool GatewayClient::sendTankInput(const TankInput& input, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeTankInput(input), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return false;
    }
    return true;
}

QString GatewayClient::queryLeaderboardTop(std::size_t limit, QString* errorMessage) {
    const auto result = client_->leaderboard_top(limit, kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        return {};
    }
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::formatError(std::int32_t code, const std::string& message) {
    return QString("code=%1 message=%2").arg(code).arg(QString::fromStdString(message));
}

void GatewayClient::installCallbacks() {
    client_->on_push([this](const boost_gateway::sdk::PushMessage& message) {
        const auto body = QString::fromStdString(message.body);
        QMetaObject::invokeMethod(this, [this, body]() {
            emit pushReceived(body);
            if (auto snapshot = decodeTankSnapshot(body.toStdString())) {
                emit tankSnapshotReceived(*snapshot);
            }
        }, Qt::QueuedConnection);
    });

    client_->on_disconnect([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            emit disconnected();
        }, Qt::QueuedConnection);
    });
}

}  // namespace bgtc

#include "sdk/GatewayClient.h"

#include <boost_gateway/sdk/version.h>

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
        recordError("connect failed");
        return false;
    }
    diagnostics_.gatewayHost = config.host;
    diagnostics_.gatewayPort = config.port;
    recordEvent("connected");
    return true;
}

bool GatewayClient::reconnectToGateway(const AppConfig& config,
                                       const QString& userId,
                                       const QString& token,
                                       QString* errorMessage) {
    ++diagnostics_.reconnectAttempts;
    publishDiagnostics();
    disconnectFromGateway();
    if (!connectToGateway(config, errorMessage)) {
        return false;
    }
    if (!login(userId, token, errorMessage)) {
        return false;
    }
    recordEvent("reconnected");
    return true;
}

void GatewayClient::disconnectFromGateway() {
    if (client_ && client_->is_connected()) {
        client_->stop_heartbeat();
        client_->disconnect();
        recordEvent("disconnected");
    }
}

bool GatewayClient::isConnected() const {
    return client_ && client_->is_connected();
}

ClientDiagnostics GatewayClient::diagnostics() const {
    return diagnostics_;
}

QString GatewayClient::sdkVersion() const {
    return QString::fromUtf8(BOOST_GATEWAY_SDK_VERSION);
}

bool GatewayClient::registerUser(const QString&,
                                 const QString&,
                                 const QString&,
                                 QString* errorMessage) {
    const auto message = unsupportedFeatureMessage("注册账号");
    if (errorMessage) {
        *errorMessage = message;
    }
    recordError(message);
    return false;
}

bool GatewayClient::login(const QString& userId, const QString& token, QString* errorMessage) {
    const auto result = client_->login(toStdString(userId), toStdString(token), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "login failed");
        return false;
    }
    client_->start_heartbeat();
    recordEvent("logged in: " + userId);
    return true;
}

bool GatewayClient::createRoom(const QString& roomId, QString* errorMessage) {
    const auto result = client_->create_room(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "create room failed");
        return false;
    }
    recordEvent("created room: " + roomId);
    return true;
}

bool GatewayClient::joinRoom(const QString& roomId, QString* errorMessage) {
    const auto result = client_->join_room(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "join room failed");
        return false;
    }
    recordEvent("joined room: " + roomId);
    return true;
}

bool GatewayClient::leaveRoom(const QString& roomId, QString* errorMessage) {
    const auto result = client_->leave_room(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "leave room failed");
        return false;
    }
    recordEvent("left room: " + roomId);
    return true;
}

bool GatewayClient::setReady(bool ready, QString* errorMessage) {
    const auto result = client_->set_ready(ready, kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "ready failed");
        return false;
    }
    recordEvent(ready ? "ready" : "unready");
    return true;
}

bool GatewayClient::startBattle(const QString& roomId, QString* battleId, QString* errorMessage) {
    const auto result = client_->start_battle(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "start battle failed");
        return false;
    }
    if (battleId) {
        *battleId = QString::fromStdString(result.battle_id);
    }
    recordEvent("battle started: " + QString::fromStdString(result.battle_id));
    return true;
}

bool GatewayClient::sendTankInput(const TankInput& input, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeTankInput(input), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "send tank input failed");
        return false;
    }
    ++diagnostics_.inputsSent;
    publishDiagnostics();
    return true;
}

bool GatewayClient::sendLegacyMoveInput(int x, int y, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeLegacyMoveInput(x, y), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "send legacy move failed");
        return false;
    }
    ++diagnostics_.inputsSent;
    publishDiagnostics();
    return true;
}

bool GatewayClient::sendAttackInput(const QString& targetUserId, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeLegacyAttackInput(toStdString(targetUserId)), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "send attack failed");
        return false;
    }
    ++diagnostics_.inputsSent;
    publishDiagnostics();
    return true;
}

bool GatewayClient::sendFinishInput(const QString& reason, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeLegacyFinishInput(reason.toStdString()), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "send finish failed");
        return false;
    }
    ++diagnostics_.inputsSent;
    publishDiagnostics();
    return true;
}

QString GatewayClient::queryLeaderboardTop(std::size_t limit, QString* errorMessage) {
    const auto result = client_->leaderboard_top(limit, kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "leaderboard top failed");
        return {};
    }
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::queryLeaderboardRank(const QString& userId, QString* errorMessage) {
    const auto result = client_->leaderboard_rank(toStdString(userId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "leaderboard rank failed");
        return {};
    }
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::unsupportedFeatureMessage(const QString& feature) const {
    return feature + " 需要服务端 SDK 暴露通用 API 后接入，当前客户端已预留入口。";
}

QString GatewayClient::formatError(std::int32_t code, const std::string& message) {
    return QString("code=%1 message=%2").arg(code).arg(QString::fromStdString(message));
}

void GatewayClient::installCallbacks() {
    diagnostics_.sdkVersion = sdkVersion();
    client_->on_push([this](const boost_gateway::sdk::PushMessage& message) {
        const auto body = QString::fromStdString(message.body);
        QMetaObject::invokeMethod(this, [this, body]() {
            ++diagnostics_.pushesReceived;
            emit pushReceived(body);
            if (auto snapshot = decodeTankSnapshot(body.toStdString())) {
                ++diagnostics_.snapshotsReceived;
                diagnostics_.latestFrame = snapshot->frame;
                emit tankSnapshotReceived(*snapshot);
            }
            publishDiagnostics();
        }, Qt::QueuedConnection);
    });

    client_->on_disconnect([this]() {
        QMetaObject::invokeMethod(this, [this]() {
            recordEvent("unexpected disconnect");
            emit disconnected();
        }, Qt::QueuedConnection);
    });
}

void GatewayClient::recordError(const QString& error) {
    diagnostics_.lastError = error;
    diagnostics_.lastEvent = "error";
    publishDiagnostics();
}

void GatewayClient::recordEvent(const QString& event) {
    diagnostics_.lastEvent = event;
    publishDiagnostics();
}

void GatewayClient::publishDiagnostics() {
    emit diagnosticsChanged(diagnostics_);
}

}  // namespace bgtc

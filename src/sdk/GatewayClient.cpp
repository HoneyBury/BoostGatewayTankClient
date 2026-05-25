#include "sdk/GatewayClient.h"

#include <boost_gateway/sdk/version.h>
#include <boost_gateway/sdk/protocol/message.h>

#include <QMetaObject>

#include <chrono>

namespace bgtc {
namespace {

constexpr auto kDefaultTimeout = std::chrono::seconds(5);
constexpr auto kBattleTimeout = std::chrono::milliseconds(120);

std::string toStdString(const QString& value) {
    return value.toUtf8().toStdString();
}

struct ResumeBody {
    QString roomId;
    bool inBattle = false;
};

ResumeBody parseResumeBody(const QString& body) {
    if (!body.startsWith("session_resumed:")) {
        return ResumeBody{body, false};
    }

    const auto parts = body.split(':');
    ResumeBody parsed;
    if (parts.size() >= 2) {
        parsed.roomId = parts.at(1);
    }
    for (const auto& part : parts) {
        if (part == "battle=1" || part == "battle=true") {
            parsed.inBattle = true;
        }
    }
    return parsed;
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

bool GatewayClient::registerUser(const QString& userId,
                                 const QString& credential,
                                 const QString& displayName,
                                 QString* errorMessage) {
    const auto result = client_->register_account(
        toStdString(userId), toStdString(credential), toStdString(displayName), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "register failed");
        return false;
    }
    recordEvent("registered: " + userId);
    return true;
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

bool GatewayClient::kickRoomMember(const QString& targetUserId, QString* errorMessage) {
    const auto result = client_->room_kick(toStdString(targetUserId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "room kick failed");
        return false;
    }
    recordEvent("kicked room member: " + targetUserId);
    return true;
}

bool GatewayClient::transferRoomOwner(const QString& newOwnerId, QString* errorMessage) {
    const auto result = client_->room_transfer_owner(toStdString(newOwnerId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "room owner transfer failed");
        return false;
    }
    recordEvent("transferred room owner: " + newOwnerId);
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
    const auto result = client_->send_battle_input(encodeTankInput(input), kBattleTimeout);
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
    const auto result = client_->send_battle_input(encodeLegacyMoveInput(x, y), kBattleTimeout);
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

bool GatewayClient::sendFireDirectionInput(int dx, int dy, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeLegacyFireInput(dx, dy), kBattleTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "send fire input failed");
        return false;
    }
    ++diagnostics_.inputsSent;
    publishDiagnostics();
    return true;
}

bool GatewayClient::sendAttackInput(const QString& targetUserId, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeLegacyAttackInput(toStdString(targetUserId)), kBattleTimeout);
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
    const auto result = client_->send_battle_input(encodeLegacyFinishInput(reason.toStdString()), kBattleTimeout);
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

bool GatewayClient::sendPickupInput(const QString& itemId, QString* errorMessage) {
    const auto result = client_->send_battle_input(encodeLegacyPickupInput(toStdString(itemId)), kBattleTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "send pickup failed");
        return false;
    }
    ++diagnostics_.inputsSent;
    publishDiagnostics();
    return true;
}

QString GatewayClient::queryRoomList(std::size_t page,
                                     std::size_t pageSize,
                                     const QString& status,
                                     QString* errorMessage) {
    const auto result = client_->room_list(page, pageSize, toStdString(status), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "room list failed");
        return {};
    }
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::queryRoomDetail(const QString& roomId, QString* errorMessage) {
    const auto result = client_->room_detail(toStdString(roomId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "room detail failed");
        return {};
    }
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::queryBattleState(const QString& battleId, QString* errorMessage) {
    const auto result = client_->battle_state(toStdString(battleId), kBattleTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "battle state failed");
        return {};
    }
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::loadReplay(const QString& battleId, QString* errorMessage) {
    const auto result = client_->replay_load(toStdString(battleId), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "replay load failed");
        return {};
    }
    return QString::fromStdString(result.response_body);
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
    const auto raw = QString::fromStdString(message);
    const auto normalized = raw.trimmed();
    const auto translated = [&]() -> QString {
        if (normalized.contains("connect failed")) return "连接网关失败，请确认地址、端口和后端服务状态。";
        if (normalized.contains("login failed")) return "登录失败，请检查账号或令牌。";
        if (normalized.contains("register failed")) return "注册失败，请检查账号信息或后端注册服务。";
        if (normalized.contains("create room failed")) return "创建房间失败，请确认房间号是否可用。";
        if (normalized.contains("join room failed")) return "加入房间失败，请确认房间存在且可加入。";
        if (normalized.contains("leave room failed")) return "离开房间失败，请稍后重试。";
        if (normalized.contains("room kick failed")) return "踢出成员失败，可能不是房主或目标玩家已离开。";
        if (normalized.contains("room owner transfer failed")) return "转让房主失败，请确认目标成员仍在房间内。";
        if (normalized.contains("ready failed")) return "设置准备状态失败，请确认当前仍在房间内。";
        if (normalized.contains("start battle failed")) return "开始战斗失败，请确认所有成员都已准备。";
        if (normalized.contains("battle state failed")) return "同步战斗状态失败，请检查战斗是否已创建。";
        if (normalized.contains("room_not_found")) return "未找到对应房间。";
        if (normalized.contains("battle_not_found")) return "未找到对应战斗。";
        if (normalized.contains("not_room_owner")) return "只有房主才能执行这个操作。";
        if (normalized.contains("not_all_ready")) return "还有成员未准备，暂时不能开始战斗。";
        if (normalized.contains("matchmaking_backend_unavailable")) return "匹配服务暂时不可用，请稍后重试。";
        if (normalized.contains("invalid_match_join")) return "匹配请求无效，请检查模式和分数。";
        if (normalized.contains("session kicked")) return "当前会话已被替换，请重新登录。";
        if (normalized.isEmpty()) return "请求失败。";
        return normalized;
    }();
    return QString("错误码 %1：%2").arg(code).arg(translated);
}

QString GatewayClient::joinMatchmaking(const QString& userId,
                                       qint64 mmr,
                                       const QString& mode,
                                       QString* errorMessage) {
    const auto result = client_->match_join(toStdString(userId), mmr, toStdString(mode), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "match join failed");
        return {};
    }
    recordEvent("joined matchmaking: " + mode);
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::leaveMatchmaking(const QString& userId,
                                        const QString& mode,
                                        QString* errorMessage) {
    const auto result = client_->match_leave(toStdString(userId), toStdString(mode), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "match leave failed");
        return {};
    }
    recordEvent("left matchmaking: " + mode);
    return QString::fromStdString(result.response_body);
}

QString GatewayClient::queryMatchmakingStatus(const QString& userId,
                                              const QString& mode,
                                              QString* errorMessage) {
    const auto result = client_->match_status(toStdString(userId), toStdString(mode), kDefaultTimeout);
    if (!result.ok) {
        if (errorMessage) {
            *errorMessage = formatError(result.error_code, result.error_message);
        }
        recordError(errorMessage ? *errorMessage : "match status failed");
        return {};
    }
    return QString::fromStdString(result.response_body);
}

void GatewayClient::installCallbacks() {
    diagnostics_.sdkVersion = sdkVersion();
    client_->on_push([this](const boost_gateway::sdk::PushMessage& message) {
        const auto body = QString::fromStdString(message.body);
        const auto messageId = message.message_id;
        QMetaObject::invokeMethod(this, [this, body, messageId]() {
            ++diagnostics_.pushesReceived;
            emit pushReceived(body);
            if (messageId == boost_gateway::sdk::protocol::kSessionResumedPush) {
                const auto resumed = parseResumeBody(body);
                recordEvent("session resumed: " + resumed.roomId);
                emit sessionResumed(resumed.roomId, resumed.inBattle);
            } else if (messageId == boost_gateway::sdk::protocol::kSessionKickedPush) {
                recordError("session kicked: " + body);
                emit sessionKicked(body);
            }
            if (auto battleState = decodeBattleStateEvent(body.toStdString());
                battleState.has_value() && battleState->kind == "started" &&
                !battleState->battleId.empty()) {
                emit battleStartedPush(QString::fromStdString(battleState->roomId),
                                       QString::fromStdString(battleState->battleId));
            }
            if (auto match = decodeMatchFoundState(body.toStdString())) {
                emit matchFoundPush(*match);
            }
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

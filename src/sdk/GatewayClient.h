#pragma once

#include "core/AppConfig.h"
#include "core/Diagnostics.h"
#include "tank/TankProtocol.h"

#include <boost_gateway/sdk/client.h>

#include <QObject>
#include <QString>

#include <memory>

namespace bgtc {

class GatewayClient final : public QObject {
    Q_OBJECT

public:
    explicit GatewayClient(QObject* parent = nullptr);
    ~GatewayClient() override;

    bool connectToGateway(const AppConfig& config, QString* errorMessage = nullptr);
    bool reconnectToGateway(const AppConfig& config,
                            const QString& userId,
                            const QString& token,
                            QString* errorMessage = nullptr);
    void disconnectFromGateway();

    [[nodiscard]] bool isConnected() const;
    [[nodiscard]] ClientDiagnostics diagnostics() const;
    [[nodiscard]] QString sdkVersion() const;

    bool registerUser(const QString& userId,
                      const QString& credential,
                      const QString& displayName,
                      QString* errorMessage = nullptr);
    bool login(const QString& userId, const QString& token, QString* errorMessage = nullptr);
    bool createRoom(const QString& roomId, QString* errorMessage = nullptr);
    bool joinRoom(const QString& roomId, QString* errorMessage = nullptr);
    bool leaveRoom(const QString& roomId, QString* errorMessage = nullptr);
    bool kickRoomMember(const QString& targetUserId, QString* errorMessage = nullptr);
    bool transferRoomOwner(const QString& newOwnerId, QString* errorMessage = nullptr);
    bool setReady(bool ready, QString* errorMessage = nullptr);
    bool startBattle(const QString& roomId, QString* battleId, QString* errorMessage = nullptr);
    bool sendTankInput(const TankInput& input, QString* errorMessage = nullptr);
    bool sendLegacyMoveInput(int x, int y, QString* errorMessage = nullptr);
    bool sendFireDirectionInput(int dx, int dy, QString* errorMessage = nullptr);
    bool sendAttackInput(const QString& targetUserId, QString* errorMessage = nullptr);
    bool sendFinishInput(const QString& reason, QString* errorMessage = nullptr);
    bool sendPickupInput(const QString& itemId, QString* errorMessage = nullptr);
    QString queryRoomList(std::size_t page = 1,
                          std::size_t pageSize = 20,
                          const QString& status = {},
                          QString* errorMessage = nullptr);
    QString queryRoomDetail(const QString& roomId, QString* errorMessage = nullptr);
    QString queryBattleState(const QString& battleId, QString* errorMessage = nullptr);
    QString loadReplay(const QString& battleId, QString* errorMessage = nullptr);
    QString queryLeaderboardTop(std::size_t limit, QString* errorMessage = nullptr);
    QString queryLeaderboardRank(const QString& userId, QString* errorMessage = nullptr);

    [[nodiscard]] QString unsupportedFeatureMessage(const QString& feature) const;

signals:
    void pushReceived(QString body);
    void tankSnapshotReceived(bgtc::TankSnapshot snapshot);
    void sessionResumed(QString roomId, bool inBattle);
    void sessionKicked(QString reason);
    void disconnected();
    void diagnosticsChanged(bgtc::ClientDiagnostics diagnostics);

private:
    static QString formatError(std::int32_t code, const std::string& message);
    void installCallbacks();
    void recordError(const QString& error);
    void recordEvent(const QString& event);
    void publishDiagnostics();

    std::unique_ptr<boost_gateway::sdk::SdkClient> client_;
    ClientDiagnostics diagnostics_;
};

}  // namespace bgtc

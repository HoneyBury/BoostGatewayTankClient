#pragma once

#include "core/AppConfig.h"
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
    void disconnectFromGateway();

    bool login(const QString& userId, const QString& token, QString* errorMessage = nullptr);
    bool createRoom(const QString& roomId, QString* errorMessage = nullptr);
    bool joinRoom(const QString& roomId, QString* errorMessage = nullptr);
    bool leaveRoom(const QString& roomId, QString* errorMessage = nullptr);
    bool setReady(bool ready, QString* errorMessage = nullptr);
    bool startBattle(const QString& roomId, QString* battleId, QString* errorMessage = nullptr);
    bool sendTankInput(const TankInput& input, QString* errorMessage = nullptr);
    QString queryLeaderboardTop(std::size_t limit, QString* errorMessage = nullptr);

signals:
    void pushReceived(QString body);
    void tankSnapshotReceived(bgtc::TankSnapshot snapshot);
    void disconnected();

private:
    static QString formatError(std::int32_t code, const std::string& message);
    void installCallbacks();

    std::unique_ptr<boost_gateway::sdk::SdkClient> client_;
};

}  // namespace bgtc

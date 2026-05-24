#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankWorldModel.h"

#include <QWidget>

namespace bgtc {

class BattleWidget final : public QWidget {
    Q_OBJECT

public:
    BattleWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

public slots:
    void applySnapshot(bgtc::TankSnapshot snapshot);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void sendMove(int dx, int dy);
    void sendFire(int direction);
    void drawPanel(QPainter& painter);

    ClientSession& session_;
    GatewayClient& gateway_;
    TankSnapshot snapshot_;
    std::uint64_t nextSeq_ = 1;
    QString lastInputError_;
};

}  // namespace bgtc

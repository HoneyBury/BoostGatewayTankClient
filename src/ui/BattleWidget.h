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
    void drawSettlement(QPainter& painter);
    [[nodiscard]] const TankState* findLocalTank() const;
    [[nodiscard]] QString findFirstOpponentUserId() const;
    [[nodiscard]] QPoint tankToScreen(const TankState& tank) const;

    ClientSession& session_;
    GatewayClient& gateway_;
    TankSnapshot snapshot_;
    std::uint64_t nextSeq_ = 1;
    int fallbackX_ = 0;
    int fallbackY_ = 0;
    QString lastInput_;
    QString lastInputError_;
};

}  // namespace bgtc

#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankWorldModel.h"

#include <QWidget>
#include <unordered_map>

class QKeyEvent;
class QPainter;
class QPixmap;
class QTimer;

namespace bgtc {

class BattleWidget final : public QWidget {
    Q_OBJECT

public:
    BattleWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

    [[nodiscard]] bgtc::TankSnapshot currentSnapshot() const;

public slots:
    void applySnapshot(bgtc::TankSnapshot snapshot);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    void sendMove(int dx, int dy);
    void sendFire(int direction);
    void pollBattleState();
    void updateMovementVector();
    void setMovementKey(int key, bool pressed);
    void predictLocalTank(int gridX, int gridY, int dx, int dy);
    void drawTank(QPainter& painter, const QRect& mapRect, const TankState& tank);
    void drawBullet(QPainter& painter, const QRect& mapRect, const BulletState& bullet);
    void drawPanel(QPainter& painter);
    void drawSettlement(QPainter& painter);
    [[nodiscard]] const TankState* findLocalTank() const;
    [[nodiscard]] QString findFirstOpponentUserId() const;
    [[nodiscard]] QString findFirstItemId() const;
    [[nodiscard]] QPoint tankToScreen(const TankState& tank) const;
    [[nodiscard]] QPoint tankToGrid(const TankState& tank) const;

    ClientSession& session_;
    GatewayClient& gateway_;
    TankSnapshot snapshot_;
    QTimer* movementTimer_ = nullptr;
    QTimer* snapshotPollTimer_ = nullptr;
    std::uint64_t nextSeq_ = 1;
    int fallbackGridX_ = 0;
    int fallbackGridY_ = 0;
    int activeDx_ = 0;
    int activeDy_ = 0;
    int facingDx_ = 1;
    int facingDy_ = 0;
    std::unordered_map<std::string, int> lastHpByUser_;
    std::unordered_map<std::string, int> hitFlashByUser_;
    bool keyUp_ = false;
    bool keyDown_ = false;
    bool keyLeft_ = false;
    bool keyRight_ = false;
    bool inputBusy_ = false;
    QString lastInput_;
    QString lastInputError_;
};

}  // namespace bgtc

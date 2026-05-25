#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankWorldModel.h"

#include <QElapsedTimer>
#include <QWidget>
#include <unordered_map>

class QKeyEvent;
class QPainter;
class QTimer;

namespace bgtc {

struct FloatingText {
    std::string message;
    double worldX = 0.0;
    double worldY = 0.0;
    QColor color;
    int age = 0;
    static constexpr int kMaxAge = 45;
};

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
    void sendFire();
    void pollBattleState();
    void advanceAnimation();
    void updateMovementVector();
    void setMovementKey(int key, bool pressed);
    void predictLocalTank(double nextX, double nextY, int dx, int dy);
    void drawTank(QPainter& painter, const QRect& mapRect, const TankState& tank);
    void drawBullet(QPainter& painter, const QRect& mapRect, const BulletState& bullet);
    void drawPanel(QPainter& painter);
    void drawSettlement(QPainter& painter);
    [[nodiscard]] const TankState* findLocalTank() const;
    [[nodiscard]] QString findFirstItemId() const;
    [[nodiscard]] QPointF worldToScreen(double x, double y) const;
    [[nodiscard]] TankSnapshot interpolatedSnapshot() const;
    [[nodiscard]] double interpolationAlpha() const;

    ClientSession& session_;
    GatewayClient& gateway_;
    TankSnapshot snapshot_;
    TankSnapshot previousSnapshot_;
    QTimer* movementTimer_ = nullptr;
    QTimer* snapshotPollTimer_ = nullptr;
    QTimer* renderTimer_ = nullptr;
    QElapsedTimer interpolationClock_;
    QElapsedTimer renderStepClock_;
    std::uint64_t nextSeq_ = 1;
    double fallbackX_ = 50.0;
    double fallbackY_ = 50.0;
    int activeDx_ = 0;
    int activeDy_ = 0;
    int facingDx_ = 1;
    int facingDy_ = 0;
    std::unordered_map<std::string, int> lastHpByUser_;
    std::unordered_map<std::string, int> hitFlashByUser_;
    std::vector<FloatingText> floatingTexts_;
    int shakeTicks_ = 0;
    bool keyUp_ = false;
    bool keyDown_ = false;
    bool keyLeft_ = false;
    bool keyRight_ = false;
    bool inputBusy_ = false;
    QString lastInput_;
    QString lastInputError_;
};

}  // namespace bgtc

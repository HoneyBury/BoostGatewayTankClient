#include "ui/BattleWidget.h"

#include "tank/TankProtocol.h"

#include <QKeyEvent>
#include <QPainter>

namespace bgtc {
namespace {

constexpr int kCell = 32;
constexpr int kMapWidth = 20;
constexpr int kMapHeight = 15;

}  // namespace

BattleWidget::BattleWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(kMapWidth * kCell + 260, kMapHeight * kCell + 40);
}

void BattleWidget::applySnapshot(TankSnapshot snapshot) {
    snapshot_ = std::move(snapshot);
    update();
}

void BattleWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#17212b"));

    painter.setPen(QColor("#34495e"));
    for (int x = 0; x <= kMapWidth; ++x) {
        painter.drawLine(x * kCell, 0, x * kCell, kMapHeight * kCell);
    }
    for (int y = 0; y <= kMapHeight; ++y) {
        painter.drawLine(0, y * kCell, kMapWidth * kCell, y * kCell);
    }

    for (const auto& bullet : snapshot_.bullets) {
        painter.setBrush(QColor("#ffd166"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(bullet.x * kCell + 10, bullet.y * kCell + 10, 12, 12);
    }

    for (const auto& tank : snapshot_.tanks) {
        painter.setBrush(tank.userId == session_.userId.toStdString() ? QColor("#06d6a0") : QColor("#ef476f"));
        painter.setPen(QColor("#ffffff"));
        painter.drawRoundedRect(tank.x * kCell + 4, tank.y * kCell + 4, kCell - 8, kCell - 8, 4, 4);
        painter.drawText(tank.x * kCell + 4,
                         tank.y * kCell + 18,
                         QString::fromStdString(tank.userId.substr(0, 2)));
    }

    const int panelX = kMapWidth * kCell + 24;
    painter.setPen(QColor("#f8f9fa"));
    painter.drawText(panelX, 30, "Battle: " + session_.battleId);
    painter.drawText(panelX, 55, QString("Frame: %1").arg(snapshot_.frame));
    painter.drawText(panelX, 80, snapshot_.finished ? "Finished" : "Running");
    painter.drawText(panelX, 120, "Controls:");
    painter.drawText(panelX, 145, "WASD / Arrows: move");
    painter.drawText(panelX, 170, "Space: fire up");
}

void BattleWidget::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
            sendMove(0, -1);
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            sendMove(0, 1);
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            sendMove(-1, 0);
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            sendMove(1, 0);
            break;
        case Qt::Key_Space:
            sendFire(0);
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void BattleWidget::sendMove(int dx, int dy) {
    QString error;
    gateway_.sendTankInput(makeMoveInput(nextSeq_++, dx, dy), &error);
}

void BattleWidget::sendFire(int direction) {
    QString error;
    gateway_.sendTankInput(makeFireInput(nextSeq_++, direction), &error);
}

}  // namespace bgtc

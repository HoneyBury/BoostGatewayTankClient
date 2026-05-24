#include "ui/BattleWidget.h"

#include "tank/TankProtocol.h"

#include <QKeyEvent>
#include <QPainter>
#include <algorithm>

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

    for (const auto& item : snapshot_.items) {
        painter.setBrush(QColor("#8ecae6"));
        painter.setPen(QColor("#023047"));
        painter.drawRect(item.x * kCell + 7, item.y * kCell + 7, kCell - 14, kCell - 14);
        painter.drawText(item.x * kCell + 8,
                         item.y * kCell + 22,
                         QString::fromStdString(item.type.substr(0, 2)));
    }

    for (const auto& tank : snapshot_.tanks) {
        const auto screen = tankToScreen(tank);
        painter.setBrush(tank.userId == session_.userId.toStdString() ? QColor("#06d6a0") : QColor("#ef476f"));
        painter.setPen(QColor("#ffffff"));
        painter.drawRoundedRect(screen.x() + 4, screen.y() + 4, kCell - 8, kCell - 8, 4, 4);
        painter.drawText(screen.x() + 4,
                         screen.y() + 18,
                         QString::fromStdString(tank.userId.substr(0, 2)));
    }

    drawPanel(painter);
    if (snapshot_.finished) {
        drawSettlement(painter);
    }
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
        case Qt::Key_F: {
            QString error;
            if (!gateway_.sendFinishInput("surrender", &error)) {
                lastInputError_ = error;
                update();
            }
            break;
        }
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void BattleWidget::sendMove(int dx, int dy) {
    const auto* localTank = findLocalTank();
    const int currentX = localTank ? localTank->x : fallbackX_;
    const int currentY = localTank ? localTank->y : fallbackY_;
    const int targetX = std::clamp(currentX + dx * kCell, 0, 1000);
    const int targetY = std::clamp(currentY + dy * kCell, 0, 1000);

    QString error;
    if (!gateway_.sendLegacyMoveInput(targetX, targetY, &error)) {
        lastInputError_ = error;
        update();
        return;
    }
    ++nextSeq_;
    fallbackX_ = targetX;
    fallbackY_ = targetY;
    lastInput_ = QString("move:%1,%2").arg(targetX).arg(targetY);
    lastInputError_.clear();
    update();
}

void BattleWidget::sendFire(int direction) {
    (void)direction;
    const auto targetUserId = findFirstOpponentUserId();
    if (targetUserId.isEmpty()) {
        lastInputError_ = "当前 snapshot 中没有可攻击目标";
        update();
        return;
    }

    QString error;
    if (!gateway_.sendAttackInput(targetUserId, &error)) {
        lastInputError_ = error;
        update();
        return;
    }
    ++nextSeq_;
    lastInput_ = "attack:" + targetUserId;
    lastInputError_.clear();
    update();
}

void BattleWidget::drawPanel(QPainter& painter) {
    const int panelX = kMapWidth * kCell + 24;
    painter.setPen(QColor("#f8f9fa"));
    painter.drawText(panelX, 30, "Battle: " + session_.battleId);
    painter.drawText(panelX, 55, QString("Frame: %1").arg(snapshot_.frame));
    painter.drawText(panelX, 80, snapshot_.finished ? "已结束" : "运行中");
    painter.drawText(panelX, 105, QString("Tanks: %1").arg(snapshot_.tanks.size()));
    painter.drawText(panelX, 130, QString("Bullets: %1").arg(snapshot_.bullets.size()));
    painter.drawText(panelX, 155, QString("Items: %1").arg(snapshot_.items.size()));
    if (const auto* localTank = findLocalTank()) {
        painter.drawText(panelX, 180, QString("HP: %1  Score: %2").arg(localTank->hp).arg(localTank->score));
        painter.drawText(panelX, 205, QString("Pos: %1,%2").arg(localTank->x).arg(localTank->y));
    }
    if (snapshot_.battleState.has_value()) {
        painter.drawText(panelX, 230, "State: " + QString::fromStdString(snapshot_.battleState->kind));
    }
    painter.drawText(panelX, 265, "操作:");
    painter.drawText(panelX, 290, "WASD / 方向键：移动");
    painter.drawText(panelX, 315, "空格：攻击最近目标");
    painter.drawText(panelX, 340, "F：结束战斗");
    painter.drawText(panelX, 375, "规则：以服务端 snapshot 为准");
    if (!lastInput_.isEmpty()) {
        painter.drawText(panelX, 400, "最近输入: " + lastInput_.left(24));
    }
    if (!lastInputError_.isEmpty()) {
        painter.setPen(QColor("#ffb703"));
        painter.drawText(panelX, 435, "最近输入错误:");
        painter.drawText(panelX, 460, lastInputError_.left(28));
    }
}

void BattleWidget::drawSettlement(QPainter& painter) {
    const QRect card(72, 70, kMapWidth * kCell - 144, 250);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor("#f8f9fa"));
    painter.setBrush(QColor(20, 28, 38, 235));
    painter.drawRoundedRect(card, 14, 14);

    painter.setPen(QColor("#ffd166"));
    painter.drawText(card.adjusted(24, 28, -24, -24), "战斗结算");
    painter.setPen(QColor("#f8f9fa"));
    painter.drawText(card.left() + 24,
                     card.top() + 72,
                     "胜利玩家: " + QString::fromStdString(snapshot_.winnerUserId.empty() ? "未知" : snapshot_.winnerUserId));
    painter.drawText(card.left() + 24,
                     card.top() + 102,
                     "结束原因: " + QString::fromStdString(snapshot_.finishReason.empty() ? "unknown" : snapshot_.finishReason));
    painter.drawText(card.left() + 24,
                     card.top() + 132,
                     QString("总帧数: %1").arg(snapshot_.totalFrames > 0 ? snapshot_.totalFrames : snapshot_.frame));

    int y = card.top() + 168;
    painter.setPen(QColor("#8ecae6"));
    painter.drawText(card.left() + 24, y, "分数:");
    painter.setPen(QColor("#f8f9fa"));
    for (const auto& score : snapshot_.scores) {
        y += 24;
        painter.drawText(card.left() + 44,
                         y,
                         QString::fromStdString(score.userId) + QString(": %1").arg(score.score));
    }
    if (snapshot_.scores.empty()) {
        painter.drawText(card.left() + 44, y + 24, "服务端未返回 scores 字段");
    }
}

const TankState* BattleWidget::findLocalTank() const {
    const auto userId = session_.userId.toStdString();
    for (const auto& tank : snapshot_.tanks) {
        if (tank.userId == userId) {
            return &tank;
        }
    }
    return nullptr;
}

QString BattleWidget::findFirstOpponentUserId() const {
    const auto userId = session_.userId.toStdString();
    for (const auto& tank : snapshot_.tanks) {
        if (tank.userId != userId && tank.alive) {
            return QString::fromStdString(tank.userId);
        }
    }
    return {};
}

QPoint BattleWidget::tankToScreen(const TankState& tank) const {
    const int x = std::clamp(tank.x / kCell, 0, kMapWidth - 1) * kCell;
    const int y = std::clamp(tank.y / kCell, 0, kMapHeight - 1) * kCell;
    return QPoint(x, y);
}

}  // namespace bgtc

#include "ui/BattleWidget.h"

#include "tank/TankProtocol.h"

#include <QFont>
#include <QKeyEvent>
#include <QPainter>
#include <algorithm>

namespace bgtc {
namespace {

constexpr int kCell = 32;
constexpr int kMapWidth = 20;
constexpr int kMapHeight = 15;
constexpr int kMapMargin = 18;
constexpr int kPanelWidth = 280;

}  // namespace

BattleWidget::BattleWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(kMapWidth * kCell + kPanelWidth + kMapMargin * 3, kMapHeight * kCell + kMapMargin * 2);
}

void BattleWidget::applySnapshot(TankSnapshot snapshot) {
    snapshot_ = std::move(snapshot);
    update();
}

TankSnapshot BattleWidget::currentSnapshot() const {
    return snapshot_;
}

void BattleWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#0f1720"));

    const QRect mapRect(kMapMargin, kMapMargin, kMapWidth * kCell, kMapHeight * kCell);
    painter.setPen(QColor("#26384a"));
    painter.setBrush(QColor("#13202b"));
    painter.drawRoundedRect(mapRect.adjusted(-1, -1, 1, 1), 14, 14);

    painter.setPen(QColor("#203243"));
    for (int x = 0; x <= kMapWidth; ++x) {
        painter.drawLine(mapRect.left() + x * kCell, mapRect.top(), mapRect.left() + x * kCell, mapRect.bottom());
    }
    for (int y = 0; y <= kMapHeight; ++y) {
        painter.drawLine(mapRect.left(), mapRect.top() + y * kCell, mapRect.right(), mapRect.top() + y * kCell);
    }

    for (const auto& bullet : snapshot_.bullets) {
        painter.setBrush(QColor("#ffd166"));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(mapRect.left() + bullet.x * kCell + 10, mapRect.top() + bullet.y * kCell + 10, 12, 12);
    }

    for (const auto& item : snapshot_.items) {
        painter.setBrush(QColor("#5cc8ff"));
        painter.setPen(QColor("#bdefff"));
        painter.drawRoundedRect(mapRect.left() + item.x * kCell + 7,
                                mapRect.top() + item.y * kCell + 7,
                                kCell - 14,
                                kCell - 14,
                                5,
                                5);
        painter.setPen(QColor("#062338"));
        painter.drawText(mapRect.left() + item.x * kCell + 8,
                         mapRect.top() + item.y * kCell + 22,
                         QString::fromStdString(item.type.substr(0, 2)));
    }

    for (const auto& tank : snapshot_.tanks) {
        const auto screen = tankToScreen(tank);
        const bool isLocal = tank.userId == session_.userId.toStdString();
        painter.setBrush(isLocal ? QColor("#26d9a1") : QColor("#ff5c7a"));
        painter.setPen(QPen(isLocal ? QColor("#b8ffe9") : QColor("#ffd2dc"), 2));
        painter.drawRoundedRect(mapRect.left() + screen.x() + 4,
                                mapRect.top() + screen.y() + 4,
                                kCell - 8,
                                kCell - 8,
                                6,
                                6);
        painter.setPen(QColor("#ffffff"));
        painter.drawText(mapRect.left() + screen.x() + 7,
                         mapRect.top() + screen.y() + 20,
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
        case Qt::Key_E: {
            const auto itemId = findFirstItemId();
            if (itemId.isEmpty()) {
                lastInputError_ = "当前 snapshot 中没有可拾取道具";
                update();
                break;
            }
            QString error;
            if (!gateway_.sendPickupInput(itemId, &error)) {
                lastInputError_ = error;
                update();
                break;
            }
            ++nextSeq_;
            lastInput_ = "pickup:" + itemId;
            lastInputError_.clear();
            update();
            break;
        }
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
    const QRect panel(kMapMargin * 2 + kMapWidth * kCell, kMapMargin, kPanelWidth, kMapHeight * kCell);
    painter.setPen(QColor("#26384a"));
    painter.setBrush(QColor("#111c28"));
    painter.drawRoundedRect(panel, 16, 16);

    const int panelX = panel.left() + 20;
    int y = panel.top() + 34;
    QFont titleFont = painter.font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor("#f7fbff"));
    painter.drawText(panelX, y, "战斗面板");

    QFont bodyFont = painter.font();
    bodyFont.setPointSize(bodyFont.pointSize() - 3);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);

    y += 34;
    painter.setPen(QColor("#9fb0c2"));
    painter.drawText(panelX, y, "Battle ID");
    y += 22;
    painter.setPen(QColor("#e8f1f8"));
    painter.drawText(panelX, y, session_.battleId.isEmpty() ? "等待开始" : session_.battleId.left(24));

    y += 36;
    painter.setPen(QColor("#9fb0c2"));
    painter.drawText(panelX, y, "实时状态");
    y += 24;
    painter.setPen(snapshot_.finished ? QColor("#ffd166") : QColor("#26d9a1"));
    painter.drawText(panelX, y, snapshot_.finished ? "已结束" : "运行中");

    y += 28;
    painter.setPen(QColor("#e8f1f8"));
    painter.drawText(panelX, y, QString("Frame  %1").arg(snapshot_.frame));
    y += 24;
    painter.drawText(panelX, y, QString("Tanks  %1    Bullets  %2").arg(snapshot_.tanks.size()).arg(snapshot_.bullets.size()));
    y += 24;
    painter.drawText(panelX, y, QString("Items  %1").arg(snapshot_.items.size()));
    if (!snapshot_.buffs.empty()) {
        y += 24;
        painter.drawText(panelX, y, QString("Buffs  %1").arg(snapshot_.buffs.size()));
    }

    if (const auto* localTank = findLocalTank()) {
        y += 34;
        painter.setPen(QColor("#9fb0c2"));
        painter.drawText(panelX, y, "我的坦克");
        y += 24;
        painter.setPen(QColor("#e8f1f8"));
        painter.drawText(panelX, y, QString("HP %1    Score %2").arg(localTank->hp).arg(localTank->score));
        y += 24;
        painter.drawText(panelX, y, QString("Position %1, %2").arg(localTank->x).arg(localTank->y));
    }
    if (snapshot_.battleState.has_value()) {
        y += 30;
        painter.setPen(QColor("#9fb0c2"));
        painter.drawText(panelX, y, "服务端阶段");
        y += 24;
        painter.setPen(QColor("#e8f1f8"));
        painter.drawText(panelX, y, QString::fromStdString(snapshot_.battleState->kind).left(24));
    }

    y += 36;
    painter.setPen(QColor("#9fb0c2"));
    painter.drawText(panelX, y, "操作");
    y += 24;
    painter.setPen(QColor("#e8f1f8"));
    painter.drawText(panelX, y, "WASD / 方向键：移动");
    y += 24;
    painter.drawText(panelX, y, "空格：攻击最近目标");
    y += 24;
    painter.drawText(panelX, y, "E：拾取首个道具");
    y += 24;
    painter.drawText(panelX, y, "F：结束战斗");
    y += 30;
    painter.setPen(QColor("#9fb0c2"));
    painter.drawText(panelX, y, "规则：以服务端 snapshot 为准");

    if (!lastInput_.isEmpty()) {
        y += 30;
        painter.setPen(QColor("#26d9a1"));
        painter.drawText(panelX, y, "最近输入：" + lastInput_.left(24));
    }
    if (!lastInputError_.isEmpty()) {
        painter.setPen(QColor("#ffb703"));
        y += 30;
        painter.drawText(panelX, y, "最近输入错误：");
        y += 24;
        painter.drawText(panelX, y, lastInputError_.left(30));
    }
}

QString BattleWidget::findFirstItemId() const {
    if (snapshot_.items.empty()) {
        return {};
    }
    return QString::fromStdString(snapshot_.items.front().id);
}

void BattleWidget::drawSettlement(QPainter& painter) {
    const QRect card(kMapMargin + 72, kMapMargin + 54, kMapWidth * kCell - 144, 270);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor("#35536d"));
    painter.setBrush(QColor(17, 28, 40, 242));
    painter.drawRoundedRect(card, 18, 18);

    QFont titleFont = painter.font();
    titleFont.setPointSize(titleFont.pointSize() + 5);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor("#ffd166"));
    painter.drawText(card.adjusted(24, 28, -24, -24), "战斗结算");
    QFont bodyFont = painter.font();
    bodyFont.setPointSize(bodyFont.pointSize() - 5);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);

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

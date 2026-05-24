#include "ui/BattleWidget.h"

#include "tank/TankProtocol.h"

#include <QFont>
#include <QCoreApplication>
#include <QEventLoop>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QTimer>
#include <algorithm>

namespace bgtc {
namespace {

constexpr int kCell = 32;
constexpr int kWorldCell = 50;
constexpr int kMapWidth = 20;
constexpr int kMapHeight = 15;
constexpr int kMapMargin = 18;
constexpr int kPanelWidth = 340;
constexpr int kMoveIntervalMs = 70;
constexpr int kSnapshotPollMs = 80;

int clampGridX(int x) {
    return std::clamp(x, 0, kMapWidth - 1);
}

int clampGridY(int y) {
    return std::clamp(y, 0, kMapHeight - 1);
}

int gridToWorld(int value) {
    return value * kWorldCell;
}

int worldToGrid(int value, int maxGrid) {
    return std::clamp((value + kWorldCell / 2) / kWorldCell, 0, maxGrid);
}

void drawTankBody(QPainter& painter, const QRect& rect, const QColor& body, const QColor& accent) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(body);
    painter.setPen(QPen(accent.lighter(130), 2));
    painter.drawRoundedRect(rect.adjusted(4, 5, -4, -5), 7, 7);
    painter.setBrush(body.darker(125));
    painter.drawRoundedRect(rect.adjusted(8, 10, -8, -10), 5, 5);
    painter.setBrush(accent);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect.center(), 5, 5);
    painter.restore();
}

QPoint bulletToScreen(const BulletState& bullet) {
    return QPoint(worldToGrid(bullet.x, kMapWidth - 1) * kCell + kCell / 2,
                  worldToGrid(bullet.y, kMapHeight - 1) * kCell + kCell / 2);
}

}  // namespace

BattleWidget::BattleWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(kMapWidth * kCell + kPanelWidth + kMapMargin * 3, kMapHeight * kCell + kMapMargin * 2);

    movementTimer_ = new QTimer(this);
    movementTimer_->setInterval(kMoveIntervalMs);
    connect(movementTimer_, &QTimer::timeout, this, [this]() {
        if ((activeDx_ != 0 || activeDy_ != 0) && !snapshot_.finished) {
            sendMove(activeDx_, activeDy_);
        }
    });

    snapshotPollTimer_ = new QTimer(this);
    snapshotPollTimer_->setInterval(kSnapshotPollMs);
    connect(snapshotPollTimer_, &QTimer::timeout, this, &BattleWidget::pollBattleState);
    snapshotPollTimer_->start();
}

void BattleWidget::applySnapshot(TankSnapshot snapshot) {
    snapshot_ = std::move(snapshot);
    if (const auto* localTank = findLocalTank()) {
        const auto grid = tankToGrid(*localTank);
        fallbackGridX_ = grid.x();
        fallbackGridY_ = grid.y();
        facingDx_ = localTank->directionX;
        facingDy_ = localTank->directionY;
    }
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
        drawTank(painter, mapRect, tank);
    }

    for (const auto& bullet : snapshot_.bullets) {
        drawBullet(painter, mapRect, bullet);
    }

    drawPanel(painter);
    if (snapshot_.finished) {
        drawSettlement(painter);
    }
}

void BattleWidget::keyPressEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        QWidget::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
        case Qt::Key_S:
        case Qt::Key_Down:
        case Qt::Key_A:
        case Qt::Key_Left:
        case Qt::Key_D:
        case Qt::Key_Right:
            setMovementKey(event->key(), true);
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

void BattleWidget::keyReleaseEvent(QKeyEvent* event) {
    if (event->isAutoRepeat()) {
        QWidget::keyReleaseEvent(event);
        return;
    }

    switch (event->key()) {
        case Qt::Key_W:
        case Qt::Key_Up:
        case Qt::Key_S:
        case Qt::Key_Down:
        case Qt::Key_A:
        case Qt::Key_Left:
        case Qt::Key_D:
        case Qt::Key_Right:
            setMovementKey(event->key(), false);
            break;
        default:
            QWidget::keyReleaseEvent(event);
            break;
    }
}

void BattleWidget::setMovementKey(int key, bool pressed) {
    switch (key) {
        case Qt::Key_W:
        case Qt::Key_Up:
            keyUp_ = pressed;
            break;
        case Qt::Key_S:
        case Qt::Key_Down:
            keyDown_ = pressed;
            break;
        case Qt::Key_A:
        case Qt::Key_Left:
            keyLeft_ = pressed;
            break;
        case Qt::Key_D:
        case Qt::Key_Right:
            keyRight_ = pressed;
            break;
        default:
            break;
    }

    updateMovementVector();
    if (activeDx_ != 0 || activeDy_ != 0) {
        if (!movementTimer_->isActive()) {
            movementTimer_->start();
        }
        sendMove(activeDx_, activeDy_);
    } else {
        movementTimer_->stop();
    }
}

void BattleWidget::updateMovementVector() {
    const int rawDx = (keyRight_ ? 1 : 0) - (keyLeft_ ? 1 : 0);
    const int rawDy = (keyDown_ ? 1 : 0) - (keyUp_ ? 1 : 0);

    // 单帧只允许一个轴移动，避免 W+D 这类组合键产生斜向跳格。
    if (rawDx != 0) {
        activeDx_ = rawDx;
        activeDy_ = 0;
        facingDx_ = rawDx;
        facingDy_ = 0;
        return;
    }
    activeDx_ = 0;
    activeDy_ = rawDy;
    if (rawDy != 0) {
        facingDx_ = 0;
        facingDy_ = rawDy;
    }
}

void BattleWidget::sendMove(int dx, int dy) {
    if (inputBusy_) {
        return;
    }

    const auto* localTank = findLocalTank();
    int currentGridX = fallbackGridX_;
    int currentGridY = fallbackGridY_;
    if (localTank != nullptr) {
        const auto grid = tankToGrid(*localTank);
        currentGridX = grid.x();
        currentGridY = grid.y();
    }

    const int multiplier = localTank != nullptr ? std::max(1, localTank->speedMultiplier) : 1;
    const int targetGridX = clampGridX(currentGridX + dx * multiplier);
    const int targetGridY = clampGridY(currentGridY + dy * multiplier);
    if (targetGridX == currentGridX && targetGridY == currentGridY) {
        return;
    }

    const int targetX = gridToWorld(targetGridX);
    const int targetY = gridToWorld(targetGridY);

    ++nextSeq_;
    fallbackGridX_ = targetGridX;
    fallbackGridY_ = targetGridY;
    predictLocalTank(targetGridX, targetGridY, dx, dy);
    lastInput_ = QString("move:%1,%2").arg(targetGridX).arg(targetGridY);
    lastInputError_.clear();
    update();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    inputBusy_ = true;
    QString error;
    if (!gateway_.sendLegacyMoveInput(targetX, targetY, &error)) {
        inputBusy_ = false;
        lastInputError_ = error;
        update();
        return;
    }
    inputBusy_ = false;
    update();
}

void BattleWidget::sendFire(int direction) {
    (void)direction;
    QString error;
    if (!gateway_.sendFireDirectionInput(facingDx_, facingDy_, &error)) {
        lastInputError_ = error;
        update();
        return;
    }
    ++nextSeq_;
    lastInput_ = QString("fire:%1,%2").arg(facingDx_).arg(facingDy_);
    lastInputError_.clear();
    update();
}

void BattleWidget::pollBattleState() {
    if (session_.battleId.isEmpty() || snapshot_.finished) {
        return;
    }
    QString error;
    const auto body = gateway_.queryBattleState(session_.battleId, &error);
    if (!error.isEmpty() || body.isEmpty()) {
        return;
    }
    if (auto snapshot = decodeTankSnapshot(body.toStdString())) {
        applySnapshot(*snapshot);
    }
}

void BattleWidget::drawPanel(QPainter& painter) {
    const QRect panel(kMapMargin * 2 + kMapWidth * kCell, kMapMargin, kPanelWidth, kMapHeight * kCell);
    painter.setPen(QColor("#26384a"));
    painter.setBrush(QColor("#111c28"));
    painter.drawRoundedRect(panel, 16, 16);

    const int panelX = panel.left() + 20;
    int y = panel.top() + 34;
    QFont titleFont = painter.font();
    titleFont.setPixelSize(20);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor("#f7fbff"));
    painter.drawText(panelX, y, "战斗面板");

    QFont bodyFont = painter.font();
    bodyFont.setPixelSize(15);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);

    y += 34;
    painter.setPen(QColor("#9fb0c2"));
    painter.drawText(panelX, y, "Battle ID");
    y += 22;
    painter.setPen(QColor("#e8f1f8"));
    painter.drawText(panelX, y, session_.battleId.isEmpty() ? "等待开始" : session_.battleId.left(30));

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
        const auto grid = tankToGrid(*localTank);
        y += 34;
        painter.setPen(QColor("#9fb0c2"));
        painter.drawText(panelX, y, "我的坦克");
        y += 24;
        painter.setPen(QColor("#e8f1f8"));
        painter.drawText(panelX, y, QString("HP %1    Score %2").arg(localTank->hp).arg(localTank->score));
        y += 24;
        painter.drawText(panelX, y, QString("Grid %1, %2").arg(grid.x()).arg(grid.y()));
        y += 24;
        painter.drawText(panelX, y, QString("Speed x%1").arg(std::max(1, localTank->speedMultiplier)));
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
    painter.drawText(panelX, y, "WASD / 方向键：按网格移动");
    y += 24;
    painter.drawText(panelX, y, "空格：按朝向发射子弹");
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
        painter.drawText(panelX, y, "最近输入：" + lastInput_.left(28));
    }
    if (!lastInputError_.isEmpty()) {
        painter.setPen(QColor("#ffb703"));
        y += 30;
        painter.drawText(panelX, y, "最近输入错误：");
        y += 24;
        painter.drawText(panelX, y, lastInputError_.left(34));
    }
}

QString BattleWidget::findFirstItemId() const {
    if (snapshot_.items.empty()) {
        return {};
    }
    return QString::fromStdString(snapshot_.items.front().id);
}

void BattleWidget::predictLocalTank(int gridX, int gridY, int dx, int dy) {
    const auto userId = session_.userId.toStdString();
    for (auto& tank : snapshot_.tanks) {
        if (tank.userId == userId) {
            tank.x = gridToWorld(gridX);
            tank.y = gridToWorld(gridY);
            tank.directionX = dx;
            tank.directionY = dy;
            return;
        }
    }
}

void BattleWidget::drawTank(QPainter& painter, const QRect& mapRect, const TankState& tank) {
    const auto screen = tankToScreen(tank);
    const bool isLocal = tank.userId == session_.userId.toStdString();
    const QRect rect(mapRect.left() + screen.x(), mapRect.top() + screen.y(), kCell, kCell);
    const QColor body = isLocal ? QColor("#26d9a1") : QColor("#ff5c7a");
    const QColor accent = isLocal ? QColor("#b8ffe9") : QColor("#ffd2dc");
    drawTankBody(painter, rect, body, accent);

    const QPoint center = rect.center();
    const int dirX = tank.directionX == 0 && tank.directionY == 0 ? 1 : tank.directionX;
    const int dirY = tank.directionX == 0 && tank.directionY == 0 ? 0 : tank.directionY;
    painter.setPen(QPen(accent, 4, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, center + QPoint(dirX * 17, dirY * 17));

    painter.setPen(QColor("#ffffff"));
    QFont labelFont = painter.font();
    labelFont.setPixelSize(11);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.drawText(rect.adjusted(5, 7, -5, -5), Qt::AlignCenter, QString::fromStdString(tank.userId.substr(0, 2)));
}

void BattleWidget::drawBullet(QPainter& painter, const QRect& mapRect, const BulletState& bullet) {
    const auto pos = bulletToScreen(bullet);
    const QPoint center(mapRect.left() + pos.x(), mapRect.top() + pos.y());
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#ffd166"));
    painter.drawEllipse(center, 6, 6);
    painter.setBrush(QColor(255, 209, 102, 80));
    painter.drawEllipse(center - QPoint(bullet.dx * 8, bullet.dy * 8), 10, 10);
    painter.restore();
}

void BattleWidget::drawSettlement(QPainter& painter) {
    const QRect card(kMapMargin + 72, kMapMargin + 54, kMapWidth * kCell - 144, 270);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor("#35536d"));
    painter.setBrush(QColor(17, 28, 40, 242));
    painter.drawRoundedRect(card, 18, 18);

    QFont titleFont = painter.font();
    titleFont.setPixelSize(24);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor("#ffd166"));
    painter.drawText(card.adjusted(24, 28, -24, -24), "战斗结算");
    QFont bodyFont = painter.font();
    bodyFont.setPixelSize(16);
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
    const auto grid = tankToGrid(tank);
    const int x = grid.x() * kCell;
    const int y = grid.y() * kCell;
    return QPoint(x, y);
}

QPoint BattleWidget::tankToGrid(const TankState& tank) const {
    return QPoint(worldToGrid(tank.x, kMapWidth - 1), worldToGrid(tank.y, kMapHeight - 1));
}

}  // namespace bgtc

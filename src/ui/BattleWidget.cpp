#include "ui/BattleWidget.h"

#include "tank/TankProtocol.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFont>
#include <QKeyEvent>
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace bgtc {
namespace {

constexpr double kWorldWidth = 1000.0;
constexpr double kWorldHeight = 1000.0;
constexpr int kMapMargin = 18;
constexpr int kPanelWidth = 340;
constexpr int kMoveIntervalMs = 45;
constexpr int kSnapshotPollMs = 50;
constexpr int kRenderIntervalMs = 16;
constexpr double kClientMoveStep = 90.0;
constexpr double kInterpolationWindowMs = 80.0;
constexpr double kFixedRenderStepMs = 16.0;

double clampWorldX(double x) {
    return std::clamp(x, 0.0, kWorldWidth);
}

double clampWorldY(double y) {
    return std::clamp(y, 0.0, kWorldHeight);
}

double lerp(double a, double b, double t) {
    return a + (b - a) * std::clamp(t, 0.0, 1.0);
}

void drawTankBody(QPainter& painter, const QRectF& rect, const QColor& body, const QColor& accent) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(body);
    painter.setPen(QPen(accent.lighter(130), 2));
    painter.drawRoundedRect(rect.adjusted(4, 5, -4, -5), 10, 10);
    painter.setBrush(body.darker(125));
    painter.drawRoundedRect(rect.adjusted(9, 11, -9, -11), 7, 7);
    painter.setBrush(accent);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(rect.center(), 6, 6);
    painter.restore();
}

}  // namespace

BattleWidget::BattleWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(980, 700);

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

    renderTimer_ = new QTimer(this);
    renderTimer_->setInterval(kRenderIntervalMs);
    connect(renderTimer_, &QTimer::timeout, this, &BattleWidget::advanceAnimation);
    renderTimer_->start();

    interpolationClock_.start();
    renderStepClock_.start();
}

void BattleWidget::applySnapshot(TankSnapshot snapshot) {
    for (const auto& tank : snapshot.tanks) {
        const auto last = lastHpByUser_.find(tank.userId);
        if (last != lastHpByUser_.end() && tank.hp < last->second) {
            hitFlashByUser_[tank.userId] = 6;
            // Trigger screen shake for local tank hits
            if (tank.userId == session_.userId.toStdString()) {
                shakeTicks_ = 8;
            }
        }
        lastHpByUser_[tank.userId] = tank.hp;
    }
    for (auto& [userId, ticks] : hitFlashByUser_) {
        (void)userId;
        if (ticks > 0) {
            --ticks;
        }
    }
    previousSnapshot_ = snapshot_;
    snapshot_ = std::move(snapshot);
    if (const auto* localTank = findLocalTank()) {
        fallbackX_ = localTank->x;
        fallbackY_ = localTank->y;
        facingDx_ = localTank->directionX;
        facingDy_ = localTank->directionY;
    }
    // Generate floating texts from new events
    for (const auto& event : snapshot.events) {
        FloatingText ft;
        ft.worldX = 0.0;
        ft.worldY = 0.0;
        // Try to find position from the affected tank
        for (const auto& tank : snapshot.tanks) {
            if (tank.userId == event.actor) {
                ft.worldX = tank.x;
                ft.worldY = tank.y;
                break;
            }
        }
        if (event.type == "hit" || event.type == "damage") {
            ft.message = "-" + std::to_string(event.damage);
            ft.color = QColor("#ef4444");
        } else if (event.type == "kill") {
            ft.message = "KILL";
            ft.color = QColor("#fbbf24");
        } else if (event.type == "death") {
            ft.message = "DEATH";
            ft.color = QColor("#ef4444");
        } else if (event.type == "item_pickup") {
            ft.message = "+" + event.buffType;
            ft.color = QColor("#4ade80");
        } else if (event.type == "buff_expired") {
            ft.message = "-" + event.buffType;
            ft.color = QColor("#9fb0c2");
        } else if (event.type == "heal") {
            ft.message = "+" + std::to_string(event.damage) + " HP";
            ft.color = QColor("#4ade80");
        } else {
            ft.message = event.type;
            ft.color = QColor("#dfe7ef");
        }
        floatingTexts_.push_back(ft);
    }

    interpolationClock_.restart();
    update();
}

TankSnapshot BattleWidget::currentSnapshot() const {
    return snapshot_;
}

void BattleWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor("#0f1720"));

    // Screen shake offset
    int shakeDx = 0;
    int shakeDy = 0;
    if (shakeTicks_ > 0) {
        const double intensity = static_cast<double>(shakeTicks_) / 8.0;
        shakeDx = static_cast<int>(std::sin(shakeTicks_ * 1.7) * 4.0 * intensity);
        shakeDy = static_cast<int>(std::cos(shakeTicks_ * 2.3) * 4.0 * intensity);
    }

    const int mapSide = std::min(height() - kMapMargin * 2, width() - kPanelWidth - kMapMargin * 3);
    const QRect mapRect(kMapMargin + shakeDx, kMapMargin + shakeDy, std::max(420, mapSide), std::max(420, mapSide));
    painter.setPen(QColor("#26384a"));
    painter.setBrush(QColor("#13202b"));
    painter.drawRoundedRect(mapRect.adjusted(-1, -1, 1, 1), 14, 14);

    painter.setPen(QColor("#1c3144"));
    for (int x = 0; x <= 10; ++x) {
        const int screenX = mapRect.left() + x * mapRect.width() / 10;
        painter.drawLine(screenX, mapRect.top(), screenX, mapRect.bottom());
    }
    for (int y = 0; y <= 10; ++y) {
        const int screenY = mapRect.top() + y * mapRect.height() / 10;
        painter.drawLine(mapRect.left(), screenY, mapRect.right(), screenY);
    }

    const auto frame = interpolatedSnapshot();
    for (const auto& item : frame.items) {
        const auto pos = worldToScreen(item.x, item.y);
        QRectF itemRect(pos.x() - 14.0, pos.y() - 14.0, 28.0, 28.0);
        painter.setBrush(QColor("#5cc8ff"));
        painter.setPen(QColor("#bdefff"));
        painter.drawRoundedRect(itemRect, 7, 7);
        painter.setPen(QColor("#062338"));
        painter.drawText(itemRect, Qt::AlignCenter, QString::fromStdString(item.type.substr(0, 2)));

        // Countdown overlay
        if (item.remainingTicks > 0) {
            const int secs = item.remainingTicks / 60;
            QColor cdColor = QColor("#4ade80");
            if (secs <= 3) cdColor = QColor("#ef4444");
            else if (secs <= 8) cdColor = QColor("#fbbf24");
            painter.setPen(cdColor);
            QFont cdFont = painter.font();
            cdFont.setPixelSize(10);
            cdFont.setBold(true);
            painter.setFont(cdFont);
            painter.drawText(QRectF(pos.x() - 14.0, pos.y() + 3.0, 28.0, 12.0),
                             Qt::AlignHCenter | Qt::AlignTop,
                             QString::number(secs) + "s");
        }
    }

    for (const auto& tank : frame.tanks) {
        drawTank(painter, mapRect, tank);
    }
    for (const auto& bullet : frame.bullets) {
        drawBullet(painter, mapRect, bullet);
    }

    // Floating texts (events)
    for (const auto& ft : floatingTexts_) {
        const auto pos = worldToScreen(ft.worldX, ft.worldY);
        const double alpha = 1.0 - static_cast<double>(ft.age) / FloatingText::kMaxAge;
        const double riseY = pos.y() - ft.age * 1.2;
        QColor ftColor = ft.color;
        ftColor.setAlphaF(alpha);
        painter.save();
        painter.setPen(ftColor);
        QFont ftFont = painter.font();
        ftFont.setPixelSize(14);
        ftFont.setBold(true);
        painter.setFont(ftFont);
        painter.drawText(QPointF(pos.x() - 30.0, riseY), QString::fromStdString(ft.message));
        painter.restore();
    }

    drawPanel(painter);
    if (frame.finished) {
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
            sendFire();
            break;
        case Qt::Key_E: {
            const auto itemId = findFirstItemId();
            if (itemId.isEmpty()) {
                lastInputError_ = "当前没有可拾取的道具。";
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

void BattleWidget::sendMove(int dx, int dy) {
    if (inputBusy_) {
        return;
    }

    const auto* localTank = findLocalTank();
    double currentX = fallbackX_;
    double currentY = fallbackY_;
    if (localTank != nullptr) {
        currentX = localTank->x;
        currentY = localTank->y;
    }

    const int multiplier = localTank != nullptr ? std::max(1, localTank->speedMultiplier) : 1;
    const double step = kClientMoveStep * static_cast<double>(multiplier);
    const double nextX = clampWorldX(currentX + dx * step);
    const double nextY = clampWorldY(currentY + dy * step);
    if (std::abs(nextX - currentX) < 0.1 && std::abs(nextY - currentY) < 0.1) {
        return;
    }

    ++nextSeq_;
    fallbackX_ = nextX;
    fallbackY_ = nextY;
    predictLocalTank(nextX, nextY, dx, dy);
    lastInput_ = QString("move:%1,%2").arg(nextX, 0, 'f', 1).arg(nextY, 0, 'f', 1);
    lastInputError_.clear();
    update();
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    inputBusy_ = true;
    QString error;
    if (!gateway_.sendLegacyMoveInput(static_cast<int>(std::lround(nextX)),
                                      static_cast<int>(std::lround(nextY)),
                                      &error)) {
        inputBusy_ = false;
        lastInputError_ = error;
        update();
        return;
    }
    inputBusy_ = false;
}

void BattleWidget::sendFire() {
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

void BattleWidget::advanceAnimation() {
    // Age floating texts, remove expired
    for (auto& ft : floatingTexts_) {
        ++ft.age;
    }
    floatingTexts_.erase(
        std::remove_if(floatingTexts_.begin(), floatingTexts_.end(),
                       [](const FloatingText& ft) { return ft.age >= FloatingText::kMaxAge; }),
        floatingTexts_.end());

    // Decrement shake
    if (shakeTicks_ > 0) {
        --shakeTicks_;
    }

    if (renderStepClock_.elapsed() >= static_cast<qint64>(kFixedRenderStepMs)) {
        renderStepClock_.restart();
        update();
    }
}

void BattleWidget::updateMovementVector() {
    activeDx_ = (keyRight_ ? 1 : 0) - (keyLeft_ ? 1 : 0);
    activeDy_ = (keyDown_ ? 1 : 0) - (keyUp_ ? 1 : 0);
    if (activeDx_ != 0 || activeDy_ != 0) {
        if (std::abs(activeDx_) >= std::abs(activeDy_)) {
            facingDx_ = activeDx_ == 0 ? facingDx_ : (activeDx_ > 0 ? 1 : -1);
            facingDy_ = activeDx_ == 0 ? facingDy_ : 0;
        } else {
            facingDx_ = 0;
            facingDy_ = activeDy_ > 0 ? 1 : -1;
        }
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

void BattleWidget::predictLocalTank(double nextX, double nextY, int dx, int dy) {
    for (auto& tank : snapshot_.tanks) {
        if (tank.userId == session_.userId.toStdString()) {
            tank.x = nextX;
            tank.y = nextY;
            tank.directionX = dx == 0 ? tank.directionX : (dx > 0 ? 1 : -1);
            tank.directionY = dy == 0 ? tank.directionY : (dy > 0 ? 1 : -1);
            break;
        }
    }
}

void BattleWidget::drawTank(QPainter& painter, const QRect& mapRect, const TankState& tank) {
    const auto pos = worldToScreen(tank.x, tank.y);
    const QRectF tankRect(pos.x() - 18.0, pos.y() - 18.0, 36.0, 36.0);
    const bool isLocal = tank.userId == session_.userId.toStdString();
    const bool isDead = !tank.alive;

    QColor body = isLocal ? QColor("#2dd4bf") : QColor("#60a5fa");
    QColor accent = isLocal ? QColor("#99f6e4") : QColor("#dbeafe");
    if (isDead) {
        body = QColor("#475569");
        accent = QColor("#64748b");
    } else if (hitFlashByUser_[tank.userId] > 0) {
        body = QColor("#ef4444");
        accent = QColor("#fecaca");
    }
    drawTankBody(painter, tankRect, body, accent);

    // Muzzle
    painter.save();
    painter.setPen(QColor("#f8fafc"));
    painter.setBrush(Qt::NoBrush);
    const QPointF center = tankRect.center();
    const QPointF muzzle(center.x() + tank.directionX * 20.0, center.y() + tank.directionY * 20.0);
    painter.setPen(QPen(isDead ? QColor("#64748b") : accent, 5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(center, muzzle);

    // User ID label
    painter.setPen(QColor("#dfe7ef"));
    QFont idFont = painter.font();
    idFont.setPixelSize(10);
    painter.setFont(idFont);
    painter.drawText(QRectF(tankRect.left() - 28.0, tankRect.bottom() + 2.0, 92.0, 16.0),
                     Qt::AlignCenter,
                     QString::fromStdString(tank.userId));

    // K/D stats
    painter.setPen(QColor("#9fb0c2"));
    QFont kdFont = painter.font();
    kdFont.setPixelSize(9);
    painter.setFont(kdFont);
    painter.drawText(QRectF(tankRect.left() - 28.0, tankRect.bottom() + 16.0, 92.0, 14.0),
                     Qt::AlignCenter,
                     QString("K:%1 D:%2").arg(tank.kills).arg(tank.deaths));

    // HP bar
    painter.setPen(QColor("#dfe7ef"));
    QFont hpFont = painter.font();
    hpFont.setPixelSize(10);
    painter.setFont(hpFont);
    QColor hpColor = tank.hp > 60 ? QColor("#4ade80") : tank.hp > 30 ? QColor("#fbbf24") : QColor("#ef4444");
    painter.setPen(hpColor);
    painter.drawText(QRectF(tankRect.left() - 10.0, tankRect.top() - 22.0, 80.0, 20.0),
                     Qt::AlignCenter,
                     QString("HP %1").arg(tank.hp));

    // DEAD marker
    if (isDead) {
        painter.setPen(QColor("#ef4444"));
        QFont deadFont = painter.font();
        deadFont.setPixelSize(12);
        deadFont.setBold(true);
        painter.setFont(deadFont);
        painter.drawText(tankRect, Qt::AlignCenter, "DEAD");
    }

    painter.restore();

    // Buff pills above tank
    double pillX = tankRect.left();
    for (const auto& buff : snapshot_.buffs) {
        if (buff.userId != tank.userId) continue;
        const int ticks = buff.remainingTicks;
        const int secs = ticks / 60;
        QColor pillBg = buff.type == "speed" ? QColor("#4ade80") : QColor("#fbbf24");
        QRectF pillRect(pillX, tankRect.top() - 6.0, 52.0, 14.0);
        painter.setBrush(pillBg);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(pillRect, 4, 4);
        painter.setPen(QColor("#062338"));
        QFont pillFont = painter.font();
        pillFont.setPixelSize(8);
        pillFont.setBold(true);
        painter.setFont(pillFont);
        const auto label = QString::fromStdString(buff.type) + " " + QString::number(secs) + "s";
        painter.drawText(pillRect, Qt::AlignCenter, label);
        pillX += 56.0;
    }

    (void)mapRect;
}

void BattleWidget::drawBullet(QPainter& painter, const QRect& mapRect, const BulletState& bullet) {
    (void)mapRect;
    const auto pos = worldToScreen(bullet.x, bullet.y);
    const double speed = std::sqrt(bullet.dx * bullet.dx + bullet.dy * bullet.dy);
    const double ndx = speed > 0.01 ? bullet.dx / speed : 0.0;
    const double ndy = speed > 0.01 ? bullet.dy / speed : 0.0;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Trail: 3 fading dots behind the bullet
    for (int i = 1; i <= 3; ++i) {
        const int alpha = 180 - i * 50;
        QColor trailColor("#fde047");
        trailColor.setAlpha(std::max(40, alpha));
        painter.setBrush(trailColor);
        painter.setPen(Qt::NoPen);
        const double trailR = 7.0 - i * 1.5;
        const QPointF trailPos(pos.x() - ndx * 8.0 * i, pos.y() - ndy * 8.0 * i);
        painter.drawEllipse(trailPos, trailR, trailR);
    }

    // Main bullet body
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#fde047"));
    painter.drawEllipse(pos, 7, 7);
    painter.setBrush(QColor("#fff7ae"));
    painter.drawEllipse(QPointF(pos.x() - bullet.dx * 5.0, pos.y() - bullet.dy * 5.0), 4, 4);

    // Direction arrow
    painter.setBrush(QColor("#fbbf24"));
    QPointF arrowTip(pos.x() + ndx * 8.0, pos.y() + ndy * 8.0);
    QPointF arrowLeft(pos.x() - ndx * 3.0 - ndy * 4.0, pos.y() - ndy * 3.0 + ndx * 4.0);
    QPointF arrowRight(pos.x() - ndx * 3.0 + ndy * 4.0, pos.y() - ndy * 3.0 - ndx * 4.0);
    QPolygonF arrow;
    arrow << arrowTip << arrowLeft << arrowRight;
    painter.drawPolygon(arrow);

    painter.restore();
}

void BattleWidget::drawPanel(QPainter& painter) {
    const QRect panel(width() - kPanelWidth - kMapMargin, kMapMargin, kPanelWidth, height() - kMapMargin * 2);
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
    painter.drawText(panelX, y, QString("Frame  %1").arg(snapshot_.frame));
    y += 28;
    painter.drawText(panelX, y, QString("Tanks  %1    Bullets  %2").arg(snapshot_.tanks.size()).arg(snapshot_.bullets.size()));
    y += 28;
    painter.drawText(panelX, y, QString("Battle  %1").arg(session_.battleId.isEmpty() ? "等待中" : session_.battleId));
    y += 28;
    painter.drawText(panelX, y, QString("输入  %1").arg(lastInput_.isEmpty() ? "无" : lastInput_));
    y += 28;
    painter.setPen(lastInputError_.isEmpty() ? QColor("#9fb0c2") : QColor("#fca5a5"));
    painter.drawText(QRect(panelX, y, panel.width() - 40, 48),
                     Qt::TextWordWrap,
                     lastInputError_.isEmpty() ? "WASD/方向键移动，空格射击，E 拾取，F 投降。"
                                               : "提示：" + lastInputError_);

    if (const auto* localTank = findLocalTank()) {
        painter.setPen(QColor("#dfe7ef"));
        y += 70;
        painter.drawText(panelX, y, QString("位置 %.1f, %.1f").arg(localTank->x).arg(localTank->y));
        y += 28;
        painter.drawText(panelX, y, QString("朝向 %1, %2").arg(localTank->directionX).arg(localTank->directionY));
        y += 28;
        painter.drawText(panelX, y, QString("速度 x%1").arg(std::max(1, localTank->speedMultiplier)));
        y += 40;
    } else {
        y += 70;
    }

    painter.setPen(QColor("#9fb0c2"));
    painter.drawText(QRect(panelX, y, panel.width() - 40, panel.bottom() - y - 20),
                     Qt::TextWordWrap,
                     "现在渲染使用场景坐标和平滑插值，不再按客户端网格跳格显示。"
                     "\n所有客户端通过固定轮询窗口和插值系数减小不同帧率下的视觉误差。");

    // Per-player scoreboard
    y += 60;
    painter.setPen(QColor("#f8fafc"));
    QFont sbHeader = painter.font();
    sbHeader.setPixelSize(13);
    sbHeader.setBold(true);
    painter.setFont(sbHeader);
    painter.drawText(panelX, y, "玩家列表");
    y += 22;

    QFont sbFont = painter.font();
    sbFont.setPixelSize(11);
    sbFont.setBold(false);
    painter.setFont(sbFont);

    // Header row
    painter.setPen(QColor("#62748c"));
    painter.drawText(panelX, y, "ID");
    painter.drawText(panelX + 105, y, "HP");
    painter.drawText(panelX + 145, y, "K");
    painter.drawText(panelX + 170, y, "D");
    painter.drawText(panelX + 195, y, "Score");
    y += 2;
    painter.setPen(QColor("#26384a"));
    painter.drawLine(panelX, y, panelX + 300, y);
    y += 16;

    for (const auto& tank : snapshot_.tanks) {
        const bool isLocal = tank.userId == session_.userId.toStdString();
        const bool isAlive = tank.alive;
        painter.setPen(isLocal ? QColor("#2dd4bf") : (isAlive ? QColor("#dfe7ef") : QColor("#64748b")));
        const auto shortId = QString::fromStdString(tank.userId).left(12);
        painter.drawText(panelX, y, shortId);

        // HP bar
        const int barX = panelX + 105;
        const int barW = 35;
        const int barH = 10;
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#1e293b"));
        painter.drawRoundedRect(QRect(barX, y - 2, barW, barH), 3, 3);
        const double hpFrac = std::clamp(tank.hp / 100.0, 0.0, 1.0);
        QColor hpBar = hpFrac > 0.6 ? QColor("#4ade80") : hpFrac > 0.3 ? QColor("#fbbf24") : QColor("#ef4444");
        painter.setBrush(hpBar);
        painter.drawRoundedRect(QRect(barX, y - 2, static_cast<int>(barW * hpFrac), barH), 3, 3);

        painter.setPen(QColor("#9fb0c2"));
        painter.drawText(panelX + 145, y, QString::number(tank.kills));
        painter.drawText(panelX + 170, y, QString::number(tank.deaths));
        painter.drawText(panelX + 195, y, QString::number(tank.score));
        y += 18;
        if (y > panel.bottom() - 30) break;
    }
}

void BattleWidget::drawSettlement(QPainter& painter) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const bool hasScores = !snapshot_.scores.empty();
    const int overlayW = hasScores ? 480 : 380;
    const int overlayH = hasScores ? 360 : 220;
    QRect overlay((width() - overlayW) / 2, (height() - overlayH) / 2, overlayW, overlayH);
    painter.setBrush(QColor(7, 18, 31, 230));
    painter.setPen(QPen(QColor("#38bdf8"), 2));
    painter.drawRoundedRect(overlay, 18, 18);

    QFont titleFont = painter.font();
    titleFont.setPixelSize(26);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.setPen(QColor("#f8fafc"));
    painter.drawText(overlay.adjusted(0, 30, 0, 0), Qt::AlignTop | Qt::AlignHCenter, "战斗结束");

    QFont bodyFont = painter.font();
    bodyFont.setPixelSize(17);
    bodyFont.setBold(false);
    painter.setFont(bodyFont);
    const auto winner = snapshot_.winnerUserId.empty() ? "未知" : QString::fromStdString(snapshot_.winnerUserId);
    const auto reason = snapshot_.finishReason.empty() ? "unknown" : QString::fromStdString(snapshot_.finishReason);

    int yOff = 90;
    painter.drawText(overlay.adjusted(40, yOff, -40, -40),
                     Qt::AlignTop | Qt::AlignHCenter,
                     QString("胜者：%1    原因：%2    总帧数：%3")
                         .arg(winner)
                         .arg(reason)
                         .arg(snapshot_.totalFrames > 0 ? snapshot_.totalFrames : snapshot_.frame));

    // Per-player scoreboard
    if (hasScores) {
        yOff += 42;
        painter.setPen(QColor("#9fb0c2"));
        QFont headerFont = painter.font();
        headerFont.setPixelSize(14);
        headerFont.setBold(true);
        painter.setFont(headerFont);
        const int tableX = overlay.left() + 60;
        painter.drawText(tableX, overlay.top() + yOff, "玩家");
        painter.drawText(tableX + 150, overlay.top() + yOff, "Kills");
        painter.drawText(tableX + 210, overlay.top() + yOff, "Deaths");
        painter.drawText(tableX + 280, overlay.top() + yOff, "Score");

        yOff += 26;
        QFont rowFont = painter.font();
        rowFont.setPixelSize(13);
        rowFont.setBold(false);
        painter.setFont(rowFont);

        for (const auto& score : snapshot_.scores) {
            int kills = 0;
            int deaths = 0;
            for (const auto& tank : snapshot_.tanks) {
                if (tank.userId == score.userId) {
                    kills = tank.kills;
                    deaths = tank.deaths;
                    break;
                }
            }
            const bool isWinner = score.userId == snapshot_.winnerUserId;
            painter.setPen(isWinner ? QColor("#fbbf24") : QColor("#dfe7ef"));
            painter.drawText(tableX, overlay.top() + yOff,
                             QString::fromStdString(score.userId) + (isWinner ? " 👑" : ""));
            painter.drawText(tableX + 150, overlay.top() + yOff, QString::number(kills));
            painter.drawText(tableX + 210, overlay.top() + yOff, QString::number(deaths));
            painter.drawText(tableX + 280, overlay.top() + yOff, QString::number(score.score));
            yOff += 24;
        }
    }

    painter.restore();
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

QString BattleWidget::findFirstItemId() const {
    if (snapshot_.items.empty()) {
        return {};
    }
    return QString::fromStdString(snapshot_.items.front().id);
}

QPointF BattleWidget::worldToScreen(double x, double y) const {
    const int mapSide = std::min(height() - kMapMargin * 2, width() - kPanelWidth - kMapMargin * 3);
    const QRect mapRect(kMapMargin, kMapMargin, std::max(420, mapSide), std::max(420, mapSide));
    const double sx = mapRect.left() + (x / kWorldWidth) * mapRect.width();
    const double sy = mapRect.top() + (y / kWorldHeight) * mapRect.height();
    return QPointF(sx, sy);
}

TankSnapshot BattleWidget::interpolatedSnapshot() const {
    if (previousSnapshot_.frame == 0 || previousSnapshot_.frame == snapshot_.frame) {
        return snapshot_;
    }

    TankSnapshot blended = snapshot_;
    const double t = interpolationAlpha();
    std::unordered_map<std::string, TankState> prevTanks;
    for (const auto& tank : previousSnapshot_.tanks) {
        prevTanks[tank.userId] = tank;
    }
    for (auto& tank : blended.tanks) {
        const auto it = prevTanks.find(tank.userId);
        if (it == prevTanks.end()) {
            continue;
        }
        tank.x = lerp(it->second.x, tank.x, t);
        tank.y = lerp(it->second.y, tank.y, t);
    }

    std::unordered_map<std::string, BulletState> prevBullets;
    for (const auto& bullet : previousSnapshot_.bullets) {
        prevBullets[bullet.id] = bullet;
    }
    for (auto& bullet : blended.bullets) {
        const auto it = prevBullets.find(bullet.id);
        if (it == prevBullets.end()) {
            continue;
        }
        bullet.x = lerp(it->second.x, bullet.x, t);
        bullet.y = lerp(it->second.y, bullet.y, t);
    }
    return blended;
}

double BattleWidget::interpolationAlpha() const {
    return std::clamp(static_cast<double>(interpolationClock_.elapsed()) / kInterpolationWindowMs, 0.0, 1.0);
}

}  // namespace bgtc

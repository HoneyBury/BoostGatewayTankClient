#include "ui/ReplayWidget.h"

#include "tank/TankProtocol.h"
#include "ui/BattleWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace bgtc {

ReplayWidget::ReplayWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    status_ = new QLabel("回放模块：可加载当前 battle_id 的服务端回放帧。", this);
    auto* loadButton = new QPushButton("加载当前 Battle 回放", this);
    playButton_ = new QPushButton("播放", this);
    prevButton_ = new QPushButton("上一帧", this);
    nextButton_ = new QPushButton("下一帧", this);
    speedBox_ = new QComboBox(this);
    speedBox_->addItems({"0.5x", "1x", "2x", "4x"});
    speedBox_->setCurrentIndex(1);
    auto* controls = new QHBoxLayout();
    controls->addWidget(loadButton);
    controls->addWidget(playButton_);
    controls->addWidget(prevButton_);
    controls->addWidget(nextButton_);
    controls->addWidget(speedBox_);
    controls->addStretch(1);
    preview_ = new BattleWidget(session_, gateway_, this);
    preview_->setMinimumHeight(360);
    output_ = new QTextEdit(this);
    output_->setReadOnly(true);
    output_->setPlaceholderText("这里会显示回放帧、时间轴和播放状态。");
    layout->addWidget(status_);
    layout->addLayout(controls);
    layout->addWidget(preview_, 2);
    layout->addWidget(output_, 1);
    playbackTimer_ = new QTimer(this);
    connect(loadButton, &QPushButton::clicked, this, &ReplayWidget::loadReplay);
    connect(playButton_, &QPushButton::clicked, this, &ReplayWidget::togglePlayback);
    connect(prevButton_, &QPushButton::clicked, this, &ReplayWidget::previousFrame);
    connect(nextButton_, &QPushButton::clicked, this, &ReplayWidget::nextFrame);
    connect(speedBox_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ReplayWidget::setSpeed);
    connect(playbackTimer_, &QTimer::timeout, this, &ReplayWidget::advanceFrame);
}

void ReplayWidget::loadReplay() {
    if (session_.battleId.isEmpty()) {
        output_->setPlainText("当前没有 battle_id。请先完成一局战斗，或等待重连恢复战斗状态。");
        return;
    }

    QString error;
    const auto body = gateway_.loadReplay(session_.battleId, &error);
    if (!error.isEmpty()) {
        output_->setPlainText("加载回放失败：" + error + "\n当前 battle_id: " + session_.battleId);
        return;
    }

    loadReplayPayload(body);
}

void ReplayWidget::loadReplayPayload(const QString& body) {
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (doc.isObject()) {
        auto timeline = decodeReplayTimeline(body.toStdString());
        if (timeline.has_value()) {
            timeline_ = std::move(*timeline);
            currentFrameIndex_ = 0;
            renderCurrentFrame();
        }
        const auto root = doc.object();
        const auto replay = root.value("replay").toObject();
        output_->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
        return;
    }
    output_->setPlainText(body);
}

void ReplayWidget::togglePlayback() {
    setPlaying(!playbackTimer_->isActive());
}

void ReplayWidget::previousFrame() {
    if (timeline_.frames.empty()) {
        return;
    }
    setPlaying(false);
    currentFrameIndex_ = std::max(0, currentFrameIndex_ - 1);
    renderCurrentFrame();
}

void ReplayWidget::nextFrame() {
    if (timeline_.frames.empty()) {
        return;
    }
    setPlaying(false);
    currentFrameIndex_ = std::min(static_cast<int>(timeline_.frames.size()) - 1, currentFrameIndex_ + 1);
    renderCurrentFrame();
}

void ReplayWidget::setSpeed(int index) {
    switch (index) {
        case 0:
            playbackIntervalMs_ = 1000;
            break;
        case 2:
            playbackIntervalMs_ = 250;
            break;
        case 3:
            playbackIntervalMs_ = 125;
            break;
        default:
            playbackIntervalMs_ = 500;
            break;
    }
    if (playbackTimer_->isActive()) {
        playbackTimer_->start(playbackIntervalMs_);
    }
}

void ReplayWidget::advanceFrame() {
    if (timeline_.frames.empty()) {
        setPlaying(false);
        return;
    }
    if (currentFrameIndex_ + 1 >= static_cast<int>(timeline_.frames.size())) {
        setPlaying(false);
        return;
    }
    ++currentFrameIndex_;
    renderCurrentFrame();
}

void ReplayWidget::renderCurrentFrame() {
    if (timeline_.frames.empty()) {
        status_->setText("回放尚未加载。");
        return;
    }
    const auto& frame = timeline_.frames.at(currentFrameIndex_);
    preview_->applySnapshot(frame.snapshot);
    status_->setText(QString("回放已加载：battle=%1，frame=%2/%3，timestamp=%4")
                         .arg(QString::fromStdString(timeline_.battleId))
                         .arg(currentFrameIndex_ + 1)
                         .arg(timeline_.frames.size())
                         .arg(frame.timestamp));
}

void ReplayWidget::setPlaying(bool playing) {
    if (playing && !timeline_.frames.empty()) {
        playbackTimer_->start(playbackIntervalMs_);
        playButton_->setText("暂停");
        return;
    }
    playbackTimer_->stop();
    playButton_->setText("播放");
}

}  // namespace bgtc

#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankWorldModel.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTextEdit;
class QTimer;

namespace bgtc {

class BattleWidget;


class ReplayWidget final : public QWidget {
    Q_OBJECT

public:
    ReplayWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);
    void loadReplayPayload(const QString& body);

private:
    void loadReplay();
    void togglePlayback();
    void previousFrame();
    void nextFrame();
    void setSpeed(int index);
    void advanceFrame();
    void renderCurrentFrame();
    void setPlaying(bool playing);

    ClientSession& session_;
    GatewayClient& gateway_;
    ReplayTimeline timeline_;
    int currentFrameIndex_ = 0;
    int playbackIntervalMs_ = 500;
    QLabel* status_ = nullptr;
    BattleWidget* preview_ = nullptr;
    QTextEdit* output_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* prevButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QComboBox* speedBox_ = nullptr;
    QTimer* playbackTimer_ = nullptr;
};

}  // namespace bgtc

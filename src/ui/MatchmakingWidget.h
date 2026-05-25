#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankWorldModel.h"

#include <QElapsedTimer>
#include <QWidget>

class QComboBox;
class QFrame;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTimer;

namespace bgtc {

class MatchmakingWidget final : public QWidget {
    Q_OBJECT

public:
    MatchmakingWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

signals:
    void matchFound(bgtc::MatchFoundState match);
    void matchCancelled();

private:
    void startQueue();
    void leaveQueue();
    void pollStatus();
    void applyMatchFound(const MatchFoundState& match);
    void updateStatusDisplay();
    void resetState();

    ClientSession& session_;
    GatewayClient& gateway_;

    // Idle state widgets
    QComboBox* modeBox_ = nullptr;
    QSpinBox* mmrBox_ = nullptr;
    QPushButton* joinButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    // Queued state widgets
    QPushButton* leaveButton_ = nullptr;
    QLabel* queueTimeLabel_ = nullptr;
    QLabel* queueSizeLabel_ = nullptr;
    QProgressBar* queueProgress_ = nullptr;

    // Match found overlay
    QFrame* matchFoundOverlay_ = nullptr;
    QLabel* matchFoundLabel_ = nullptr;
    QLabel* matchPlayersLabel_ = nullptr;
    QPushButton* acceptMatchButton_ = nullptr;

    QTimer* pollTimer_ = nullptr;
    QElapsedTimer queueTimer_;
    bool inQueue_ = false;
};

}  // namespace bgtc

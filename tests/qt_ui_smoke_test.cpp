#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankWorldModel.h"
#include "ui/BattleWidget.h"
#include "ui/LeaderboardWidget.h"
#include "ui/LobbyWidget.h"

#include <QApplication>
#include <QTimer>

#include <cassert>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen") : qgetenv("QT_QPA_PLATFORM"));

    QApplication app(argc, argv);

    bgtc::AppConfig config;
    bgtc::ClientSession session;
    session.userId = "qt_ui_smoke_alice";
    session.displayName = session.userId;
    session.roomId = "qt_ui_smoke_room";
    session.battleId = "qt_ui_smoke_battle";
    session.state = bgtc::ConnectionState::InBattle;

    bgtc::GatewayClient gateway;
    bgtc::LobbyWidget lobby(config, session, gateway);
    bgtc::BattleWidget battle(session, gateway);
    bgtc::LeaderboardWidget leaderboard(session, gateway);

    bgtc::TankSnapshot frame;
    frame.frame = 3;
    frame.tanks.push_back(bgtc::TankState{.userId = session.userId.toStdString(), .x = 64, .y = 96, .hp = 88, .score = 2});
    frame.tanks.push_back(bgtc::TankState{.userId = "qt_ui_smoke_bob", .x = 128, .y = 96, .hp = 100, .score = 1});
    battle.applySnapshot(frame);

    bgtc::TankSnapshot settlement;
    settlement.frame = 4;
    settlement.totalFrames = 4;
    settlement.finished = true;
    settlement.finishReason = "user_requested";
    settlement.winnerUserId = session.userId.toStdString();
    settlement.scores.push_back(bgtc::BattleScoreState{.userId = session.userId.toStdString(), .score = 2});
    battle.applySnapshot(settlement);

    lobby.resize(760, 520);
    battle.resize(960, 560);
    leaderboard.resize(760, 520);
    lobby.show();
    battle.show();
    leaderboard.show();

    QTimer::singleShot(100, &app, &QCoreApplication::quit);
    const auto rc = app.exec();
    assert(rc == 0);
    std::cout << "qt ui smoke test passed\n";
    return 0;
}

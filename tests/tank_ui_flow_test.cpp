#include "core/ClientProfile.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"
#include "tank/TankProtocol.h"
#include "ui/ReplayWidget.h"
#include "ui/SettingsWidget.h"

#include <QApplication>
#include <QDir>
#include <QMetaObject>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>

#include <cassert>
#include <iostream>

namespace {

QString replayPayload() {
    return R"json({
      "status":"ok",
      "battle_id":"ui_flow_battle",
      "replay":{
        "battle_id":"ui_flow_battle",
        "frame_count":2,
        "frames":[
          {"battle_id":"ui_flow_battle","frame_number":1,"timestamp":100,
           "snapshot":{"battle_id":"ui_flow_battle","kind":"frame_advanced","frame_number":1,
             "participants":[{"user_id":"ui_flow_alice","pos_x":32,"pos_y":64,"hp":100,"score":0}]}},
          {"battle_id":"ui_flow_battle","frame_number":2,"timestamp":200,
           "snapshot":{"battle_id":"ui_flow_battle","kind":"frame_advanced","frame_number":2,
             "participants":[{"user_id":"ui_flow_alice","pos_x":96,"pos_y":64,"hp":95,"score":1}]}}
        ]
      }
    })json";
}

}  // namespace

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", qgetenv("QT_QPA_PLATFORM").isEmpty() ? QByteArray("offscreen") : qgetenv("QT_QPA_PLATFORM"));
    QTemporaryDir tempDir;
    assert(tempDir.isValid());
    qputenv("BGTC_PROFILE_PATH", QFile::encodeName(tempDir.filePath("profile.ini")));

    QApplication app(argc, argv);

    bgtc::ClientProfile profile;
    profile.config.host = "10.0.0.7";
    profile.config.port = 9301;
    profile.config.defaultRoom = "ui_flow_room";
    profile.config.playerPrefix = "ui_flow";
    profile.userId = "ui_flow_alice";
    profile.token = "token:ui_flow_alice";
    bgtc::saveClientProfile(profile);
    const auto loaded = bgtc::loadClientProfile();
    assert(loaded.config.host == profile.config.host);
    assert(loaded.config.port == profile.config.port);
    assert(loaded.config.defaultRoom == profile.config.defaultRoom);
    assert(loaded.userId == profile.userId);
    assert(loaded.token == profile.token);

    const auto timeline = bgtc::decodeReplayTimeline(replayPayload().toStdString());
    assert(timeline.has_value());
    assert(timeline->frames.size() == 2);
    assert(timeline->frames.at(1).snapshot.frame == 2);
    assert(timeline->frames.at(1).snapshot.tanks.at(0).x == 96);

    bgtc::ClientSession session;
    session.userId = "ui_flow_alice";
    session.battleId = "ui_flow_battle";
    bgtc::GatewayClient gateway;
    bgtc::ReplayWidget replay(session, gateway);
    bgtc::SettingsWidget settings(profile.config, session, gateway);
    replay.loadReplayPayload(replayPayload());
    replay.show();
    settings.show();
    QTimer::singleShot(50, &app, &QCoreApplication::quit);
    const auto rc = app.exec();
    assert(rc == 0);

    std::cout << "tank ui flow test passed\n";
    return 0;
}

#pragma once

#include <QString>

#include <cstdint>

namespace bgtc {

struct AppConfig {
    QString host = "127.0.0.1";
    std::uint16_t port = 9201;
    QString defaultRoom = "tank_demo_room";
    QString playerPrefix = "qt_player";

    static AppConfig fromEnvironment();
};

}  // namespace bgtc

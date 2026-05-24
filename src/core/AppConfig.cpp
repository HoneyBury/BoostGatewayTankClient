#include "core/AppConfig.h"

#include <QProcessEnvironment>

namespace bgtc {

AppConfig AppConfig::fromEnvironment() {
    const auto env = QProcessEnvironment::systemEnvironment();
    AppConfig config;
    config.host = env.value("BGTC_GATEWAY_HOST", config.host);
    config.defaultRoom = env.value("BGTC_DEFAULT_ROOM", config.defaultRoom);
    config.playerPrefix = env.value("BGTC_PLAYER_PREFIX", config.playerPrefix);

    bool ok = false;
    const auto port = env.value("BGTC_GATEWAY_PORT").toUShort(&ok);
    if (ok && port > 0) {
        config.port = port;
    }
    return config;
}

}  // namespace bgtc

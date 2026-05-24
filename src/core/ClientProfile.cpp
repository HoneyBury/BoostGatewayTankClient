#include "core/ClientProfile.h"

#include <QSettings>
#include <QByteArray>

namespace bgtc {
namespace {

QSettings profileSettings() {
    const auto explicitPath = qgetenv("BGTC_PROFILE_PATH");
    if (!explicitPath.isEmpty()) {
        return QSettings(QString::fromUtf8(explicitPath), QSettings::IniFormat);
    }
    return QSettings("HoneyBury", "BoostGatewayTankClient");
}

}  // namespace

ClientProfile loadClientProfile() {
    ClientProfile profile;
    profile.config = AppConfig::fromEnvironment();

    auto settings = profileSettings();
    profile.config.host = settings.value("gateway/host", profile.config.host).toString();
    profile.config.port = static_cast<std::uint16_t>(
        settings.value("gateway/port", profile.config.port).toUInt());
    profile.config.defaultRoom = settings.value("client/defaultRoom", profile.config.defaultRoom).toString();
    profile.config.playerPrefix = settings.value("client/playerPrefix", profile.config.playerPrefix).toString();
    profile.userId = settings.value("auth/userId", profile.config.playerPrefix + "_1").toString();
    profile.token = settings.value("auth/token", "token:" + profile.userId).toString();
    return profile;
}

void saveClientProfile(const ClientProfile& profile) {
    auto settings = profileSettings();
    settings.setValue("gateway/host", profile.config.host);
    settings.setValue("gateway/port", profile.config.port);
    settings.setValue("client/defaultRoom", profile.config.defaultRoom);
    settings.setValue("client/playerPrefix", profile.config.playerPrefix);
    settings.setValue("auth/userId", profile.userId);
    settings.setValue("auth/token", profile.token);
    settings.sync();
}

QString clientProfileStoragePath() {
    return profileSettings().fileName();
}

}  // namespace bgtc

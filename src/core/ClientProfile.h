#pragma once

#include "core/AppConfig.h"

#include <QString>

namespace bgtc {

struct ClientProfile {
    AppConfig config;
    QString userId;
    QString token;
};

ClientProfile loadClientProfile();
void saveClientProfile(const ClientProfile& profile);
QString clientProfileStoragePath();

}  // namespace bgtc

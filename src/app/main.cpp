#include "core/ClientProfile.h"
#include "core/Logger.h"
#include "ui/LoginWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("BoostGateway Tank Client");
    QApplication::setApplicationVersion(BGTC_APP_VERSION);

    bgtc::Logger::instance().init();

    const auto profile = bgtc::loadClientProfile();
    bgtc::LoginWindow window(profile.config, profile.userId, profile.token);
    window.resize(520, 360);
    window.show();

    return QApplication::exec();
}

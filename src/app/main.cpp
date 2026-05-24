#include "core/AppConfig.h"
#include "ui/LoginWindow.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("BoostGateway Tank Client");
    QApplication::setApplicationVersion(BGTC_APP_VERSION);

    auto config = bgtc::AppConfig::fromEnvironment();
    bgtc::LoginWindow window(config);
    window.resize(520, 360);
    window.show();

    return QApplication::exec();
}

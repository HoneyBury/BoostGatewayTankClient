#pragma once

#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;

namespace bgtc {

class SettingsWidget final : public QWidget {
    Q_OBJECT

public:
    SettingsWidget(AppConfig& config,
                   ClientSession& session,
                   GatewayClient& gateway,
                   QWidget* parent = nullptr);

private:
    void refresh();

    AppConfig& config_;
    ClientSession& session_;
    GatewayClient& gateway_;
    QLabel* summary_ = nullptr;
};

}  // namespace bgtc

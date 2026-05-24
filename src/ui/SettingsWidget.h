#pragma once

#include "core/AppConfig.h"
#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QSpinBox;

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
    void saveProfile();

    AppConfig& config_;
    ClientSession& session_;
    GatewayClient& gateway_;
    QLineEdit* hostEdit_ = nullptr;
    QSpinBox* portEdit_ = nullptr;
    QLineEdit* defaultRoomEdit_ = nullptr;
    QLineEdit* playerPrefixEdit_ = nullptr;
    QLineEdit* tokenEdit_ = nullptr;
    QLabel* summary_ = nullptr;
};

}  // namespace bgtc

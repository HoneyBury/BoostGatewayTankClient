#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;
class QTextEdit;

namespace bgtc {

class ReplayWidget final : public QWidget {
    Q_OBJECT

public:
    ReplayWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

private:
    void loadReplay();

    ClientSession& session_;
    GatewayClient& gateway_;
    QLabel* status_ = nullptr;
    QTextEdit* output_ = nullptr;
};

}  // namespace bgtc

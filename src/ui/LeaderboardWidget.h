#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QTextEdit;

namespace bgtc {

class LeaderboardWidget final : public QWidget {
    Q_OBJECT

public:
    LeaderboardWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);

private:
    void refreshTop();
    void refreshMine();
    void setOutput(const QString& text);

    ClientSession& session_;
    GatewayClient& gateway_;
    QTextEdit* output_ = nullptr;
};

}  // namespace bgtc

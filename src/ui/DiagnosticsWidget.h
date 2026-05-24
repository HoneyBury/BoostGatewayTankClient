#pragma once

#include "core/Diagnostics.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;
class QTextEdit;

namespace bgtc {

class DiagnosticsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsWidget(GatewayClient& gateway, QWidget* parent = nullptr);

private:
    void applyDiagnostics(const ClientDiagnostics& diagnostics);

    GatewayClient& gateway_;
    QTextEdit* output_ = nullptr;
};

}  // namespace bgtc

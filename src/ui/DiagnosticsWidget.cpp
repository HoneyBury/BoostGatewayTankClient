#include "ui/DiagnosticsWidget.h"

#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bgtc {

DiagnosticsWidget::DiagnosticsWidget(GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    auto* refreshButton = new QPushButton("刷新诊断信息", this);
    output_ = new QTextEdit(this);
    output_->setReadOnly(true);
    layout->addWidget(refreshButton);
    layout->addWidget(output_, 1);

    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        applyDiagnostics(gateway_.diagnostics());
    });
    connect(&gateway_, &GatewayClient::diagnosticsChanged,
            this, &DiagnosticsWidget::applyDiagnostics);
    applyDiagnostics(gateway_.diagnostics());
}

void DiagnosticsWidget::applyDiagnostics(const ClientDiagnostics& d) {
    output_->setPlainText(QString(
        "SDK version: %1\n"
        "Gateway: %2:%3\n"
        "Pushes received: %4\n"
        "Snapshots received: %5\n"
        "Inputs sent: %6\n"
        "Reconnect attempts: %7\n"
        "Latest frame: %8\n"
        "Last event: %9\n"
        "Last error: %10\n")
        .arg(d.sdkVersion)
        .arg(d.gatewayHost)
        .arg(d.gatewayPort)
        .arg(d.pushesReceived)
        .arg(d.snapshotsReceived)
        .arg(d.inputsSent)
        .arg(d.reconnectAttempts)
        .arg(d.latestFrame)
        .arg(d.lastEvent)
        .arg(d.lastError));
}

}  // namespace bgtc

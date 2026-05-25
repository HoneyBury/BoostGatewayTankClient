#include "ui/DiagnosticsWidget.h"
#include "core/Logger.h"

#include <QDateTime>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace bgtc {

// ── MiniGraph ──────────────────────────────────────────────────────

MiniGraph::MiniGraph(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(120);
    setStyleSheet("background:#101a25;border:1px solid #26384a;border-radius:8px;");
}

void MiniGraph::addPoint(const TimeSeriesPoint& point) {
    points_.push_back(point);
    while (points_.size() > kMaxPoints) points_.pop_front();
    update();
}

void MiniGraph::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor("#101a25"));

    if (points_.size() < 2) return;

    const double h = height();
    const double w = width();
    const double margin = 8.0;
    const double graphW = w - margin * 2;
    const double graphH = h - margin * 2;
    const double step = graphW / static_cast<double>(points_.size() - 1);

    // Find max for scaling
    double maxVal = 1.0;
    for (const auto& pt : points_) {
        maxVal = std::max(maxVal, std::max(pt.pushRate, static_cast<double>(pt.latencyMs)));
    }

    // Draw push rate line (cyan)
    p.setPen(QPen(QColor("#2dd4bf"), 2));
    for (size_t i = 1; i < points_.size(); ++i) {
        const double x1 = margin + static_cast<double>(i - 1) * step;
        const double y1 = margin + graphH - (points_[i - 1].pushRate / maxVal) * graphH;
        const double x2 = margin + static_cast<double>(i) * step;
        const double y2 = margin + graphH - (points_[i].pushRate / maxVal) * graphH;
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // Draw latency line (yellow)
    p.setPen(QPen(QColor("#fbbf24"), 2, Qt::DashLine));
    for (size_t i = 1; i < points_.size(); ++i) {
        const double x1 = margin + static_cast<double>(i - 1) * step;
        const double y1 = margin + graphH - (static_cast<double>(points_[i - 1].latencyMs) / maxVal) * graphH;
        const double x2 = margin + static_cast<double>(i) * step;
        const double y2 = margin + graphH - (static_cast<double>(points_[i].latencyMs) / maxVal) * graphH;
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    // Legend
    p.setPen(QColor("#2dd4bf"));
    p.drawText(QPointF(margin + 4, margin + 12), "push/s");
    p.setPen(QColor("#fbbf24"));
    p.drawText(QPointF(margin + 64, margin + 12), "lat(ms)");
}

// ── DiagnosticsWidget ──────────────────────────────────────────────

DiagnosticsWidget::DiagnosticsWidget(GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), gateway_(gateway) {
    setStyleSheet("background:#0f172a;");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    auto* title = new QLabel("诊断面板", this);
    title->setStyleSheet("color:#f8fafc;font-size:22px;font-weight:700;");
    mainLayout->addWidget(title);

    // Status grid
    auto* grid = new QHBoxLayout();
    auto* leftCol = new QVBoxLayout();
    auto* rightCol = new QVBoxLayout();

    auto makeLabel = [this](const QString& text) {
        auto* lbl = new QLabel(text, this);
        lbl->setStyleSheet("color:#dfe7ef;font-size:13px;padding:4px 8px;");
        return lbl;
    };

    sdkLabel_ = makeLabel("SDK: --");
    gatewayLabel_ = makeLabel("Gateway: --");
    uptimeLabel_ = makeLabel("Uptime: --");
    pushRateLabel_ = makeLabel("Push rate: --");
    frameRateLabel_ = makeLabel("Latest frame: --");
    latencyLabel_ = makeLabel("Latency: --");
    reconnectLabel_ = makeLabel("Reconnects: 0");
    errorCountLabel_ = makeLabel("Errors: 0");
    sessionLabel_ = makeLabel("Room/Battle: --");

    leftCol->addWidget(sdkLabel_);
    leftCol->addWidget(gatewayLabel_);
    leftCol->addWidget(uptimeLabel_);
    leftCol->addWidget(pushRateLabel_);
    leftCol->addWidget(frameRateLabel_);
    rightCol->addWidget(latencyLabel_);
    rightCol->addWidget(reconnectLabel_);
    rightCol->addWidget(errorCountLabel_);
    rightCol->addWidget(sessionLabel_);
    grid->addLayout(leftCol);
    grid->addLayout(rightCol);
    mainLayout->addLayout(grid);

    // Time-series graph
    graph_ = new MiniGraph(this);
    mainLayout->addWidget(graph_);

    // Buttons
    auto* btnRow = new QHBoxLayout();
    exportLogBtn_ = new QPushButton("导出日志", this);
    exportLogBtn_->setMinimumHeight(36);
    exportLogBtn_->setStyleSheet(
        "QPushButton{background:#1c2f42;border:1px solid #34506a;border-radius:10px;"
        "color:#edf6ff;padding:8px 18px;font-size:13px;}"
        "QPushButton:hover{background:#25415a;}");

    exportBugReportBtn_ = new QPushButton("导出 Bug 报告", this);
    exportBugReportBtn_->setMinimumHeight(36);
    exportBugReportBtn_->setStyleSheet(
        "QPushButton{background:#7c2d12;border:1px solid #9a3412;border-radius:10px;"
        "color:#edf6ff;padding:8px 18px;font-size:13px;}"
        "QPushButton:hover{background:#9a3412;}");

    btnRow->addWidget(exportLogBtn_);
    btnRow->addWidget(exportBugReportBtn_);
    btnRow->addStretch(1);
    mainLayout->addLayout(btnRow);

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet("color:#9fb0c2;font-size:12px;");
    statusLabel_->setWordWrap(true);
    mainLayout->addWidget(statusLabel_);

    mainLayout->addStretch(1);

    // Connections
    connect(exportLogBtn_, &QPushButton::clicked, this, &DiagnosticsWidget::exportLog);
    connect(exportBugReportBtn_, &QPushButton::clicked, this, &DiagnosticsWidget::exportBugReport);
    connect(&gateway_, &GatewayClient::diagnosticsChanged, this, &DiagnosticsWidget::applyDiagnostics);

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &DiagnosticsWidget::refreshDisplay);
    refreshTimer_->start(1000);

    applyDiagnostics(gateway_.diagnostics());
}

void DiagnosticsWidget::applyDiagnostics(const ClientDiagnostics& d) {
    sdkLabel_->setText(QString("SDK: %1").arg(d.sdkVersion));
    gatewayLabel_->setText(QString("Gateway: %1:%2").arg(d.gatewayHost).arg(d.gatewayPort));

    if (d.connectedSince.isValid()) {
        const auto elapsed = d.connectedSince.msecsTo(QDateTime::currentDateTime());
        const int sec = static_cast<int>(elapsed / 1000) % 60;
        const int min = static_cast<int>(elapsed / 60000) % 60;
        const int hr  = static_cast<int>(elapsed / 3600000);
        uptimeLabel_->setText(QString("Uptime: %1h %2m %3s").arg(hr).arg(min).arg(sec));
    }

    pushRateLabel_->setText(QString("Push rate: %1/s").arg(d.averagePushRate, 0, 'f', 1));
    frameRateLabel_->setText(QString("Latest frame: %1").arg(d.latestFrame));
    latencyLabel_->setText(QString("Latency: %1ms").arg(d.averageLatencyMs, 0, 'f', 1));
    reconnectLabel_->setText(QString("Reconnects: %1").arg(d.reconnectAttempts));
    errorCountLabel_->setText(QString("Errors: %1").arg(d.errorsThisSession));
    sessionLabel_->setText(QString("Room: %1  Battle: %2")
        .arg(d.currentRoomId.isEmpty() ? "--" : d.currentRoomId,
             d.currentBattleId.isEmpty() ? "--" : d.currentBattleId));

    // Time-series
    TimeSeriesPoint pt;
    pt.timestamp = QDateTime::currentMSecsSinceEpoch();
    pt.frame = d.latestFrame;
    pt.latencyMs = static_cast<qint64>(d.averageLatencyMs);
    pt.pushRate = d.averagePushRate;
    graph_->addPoint(pt);
}

void DiagnosticsWidget::refreshDisplay() {
    applyDiagnostics(gateway_.diagnostics());
}

void DiagnosticsWidget::exportLog() {
    const auto path = QFileDialog::getSaveFileName(this, "导出日志", "tank-client-log.txt", "Text (*.txt)");
    if (path.isEmpty()) return;

    if (Logger::instance().exportToFile(path)) {
        statusLabel_->setStyleSheet("color:#4ade80;font-size:12px;");
        statusLabel_->setText("日志已导出到：" + path);
    } else {
        statusLabel_->setStyleSheet("color:#fca5a5;font-size:12px;");
        statusLabel_->setText("导出失败：无法写入文件");
    }
}

void DiagnosticsWidget::exportBugReport() {
    const auto dir = QFileDialog::getExistingDirectory(this, "选择 Bug 报告保存目录");
    if (dir.isEmpty()) return;

    const auto ts = QDateTime::currentDateTime().toString("yyyy-MM-dd-HHmmss");
    const auto reportDir = dir + "/bug-report-" + ts;
    QDir().mkpath(reportDir);

    // Write log
    Logger::instance().exportToFile(reportDir + "/client.log");

    // Write diagnostics JSON
    const auto& d = gateway_.diagnostics();
    QFile diagFile(reportDir + "/diagnostics.json");
    if (diagFile.open(QIODevice::WriteOnly)) {
        QString json = QString(
            "{\n"
            "  \"sdk_version\": \"%1\",\n"
            "  \"gateway\": \"%2:%3\",\n"
            "  \"pushes_received\": %4,\n"
            "  \"snapshots_received\": %5,\n"
            "  \"inputs_sent\": %6,\n"
            "  \"reconnect_attempts\": %7,\n"
            "  \"errors\": %8,\n"
            "  \"latest_frame\": %9,\n"
            "  \"push_rate\": %10,\n"
            "  \"latency_ms\": %11,\n"
            "  \"room_id\": \"%12\",\n"
            "  \"battle_id\": \"%13\"\n"
            "}\n")
            .arg(d.sdkVersion)
            .arg(d.gatewayHost).arg(d.gatewayPort)
            .arg(d.pushesReceived).arg(d.snapshotsReceived)
            .arg(d.inputsSent).arg(d.reconnectAttempts)
            .arg(d.errorsThisSession).arg(d.latestFrame)
            .arg(d.averagePushRate, 0, 'f', 2).arg(d.averageLatencyMs, 0, 'f', 2)
            .arg(d.currentRoomId, d.currentBattleId);
        diagFile.write(json.toUtf8());
        diagFile.close();
    }

    statusLabel_->setStyleSheet("color:#4ade80;font-size:12px;");
    statusLabel_->setText("Bug 报告已保存到：" + reportDir);
}

}  // namespace bgtc

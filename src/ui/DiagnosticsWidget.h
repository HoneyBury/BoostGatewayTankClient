#pragma once

#include "core/Diagnostics.h"
#include "sdk/GatewayClient.h"

#include <QWidget>
#include <deque>

class QLabel;
class QPushButton;
class QTimer;

namespace bgtc {

struct TimeSeriesPoint {
    qint64 timestamp = 0;
    int frame = 0;
    qint64 latencyMs = 0;
    double pushRate = 0.0;
};

class MiniGraph final : public QWidget {
    Q_OBJECT
public:
    explicit MiniGraph(QWidget* parent = nullptr);
    void addPoint(const TimeSeriesPoint& point);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::deque<TimeSeriesPoint> points_;
    static constexpr size_t kMaxPoints = 120;
};

class DiagnosticsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsWidget(GatewayClient& gateway, QWidget* parent = nullptr);

private:
    void applyDiagnostics(const ClientDiagnostics& d);
    void refreshDisplay();
    void exportLog();
    void exportBugReport();

    GatewayClient& gateway_;
    QLabel* sdkLabel_ = nullptr;
    QLabel* gatewayLabel_ = nullptr;
    QLabel* uptimeLabel_ = nullptr;
    QLabel* pushRateLabel_ = nullptr;
    QLabel* frameRateLabel_ = nullptr;
    QLabel* latencyLabel_ = nullptr;
    QLabel* reconnectLabel_ = nullptr;
    QLabel* errorCountLabel_ = nullptr;
    QLabel* sessionLabel_ = nullptr;
    MiniGraph* graph_ = nullptr;
    QPushButton* exportLogBtn_ = nullptr;
    QPushButton* exportBugReportBtn_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
    QLabel* statusLabel_ = nullptr;
};

}  // namespace bgtc

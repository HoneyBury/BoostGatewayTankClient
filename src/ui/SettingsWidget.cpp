#include "ui/SettingsWidget.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace bgtc {

SettingsWidget::SettingsWidget(AppConfig& config,
                               ClientSession& session,
                               GatewayClient& gateway,
                               QWidget* parent)
    : QWidget(parent), config_(config), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    summary_ = new QLabel(this);
    summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* refreshButton = new QPushButton("刷新配置摘要", this);
    layout->addWidget(summary_);
    layout->addWidget(refreshButton);
    layout->addStretch(1);
    connect(refreshButton, &QPushButton::clicked, this, &SettingsWidget::refresh);
    refresh();
}

void SettingsWidget::refresh() {
    summary_->setText(QString(
        "当前配置\n\n"
        "Gateway: %1:%2\n"
        "默认房间: %3\n"
        "当前用户: %4\n"
        "当前房间: %5\n"
        "当前 Battle: %6\n"
        "SDK version: %7\n\n"
        "后续设置项：音量、按键、渲染比例、服务器 profile、日志导出。")
        .arg(config_.host)
        .arg(config_.port)
        .arg(config_.defaultRoom)
        .arg(session_.userId)
        .arg(session_.roomId)
        .arg(session_.battleId)
        .arg(gateway_.sdkVersion()));
}

}  // namespace bgtc

#include "ui/SettingsWidget.h"

#include "core/ClientProfile.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace bgtc {

SettingsWidget::SettingsWidget(AppConfig& config,
                               ClientSession& session,
                               GatewayClient& gateway,
                               QWidget* parent)
    : QWidget(parent), config_(config), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("客户端 Profile", this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto* form = new QFormLayout();
    hostEdit_ = new QLineEdit(config_.host, this);
    portEdit_ = new QSpinBox(this);
    portEdit_->setRange(1, 65535);
    portEdit_->setValue(config_.port);
    defaultRoomEdit_ = new QLineEdit(config_.defaultRoom, this);
    playerPrefixEdit_ = new QLineEdit(config_.playerPrefix, this);
    tokenEdit_ = new QLineEdit("token:" + session_.userId, this);
    form->addRow("Gateway Host", hostEdit_);
    form->addRow("Gateway Port", portEdit_);
    form->addRow("默认房间", defaultRoomEdit_);
    form->addRow("玩家前缀", playerPrefixEdit_);
    form->addRow("当前 Token", tokenEdit_);
    layout->addLayout(form);

    summary_ = new QLabel(this);
    summary_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto* refreshButton = new QPushButton("刷新配置摘要", this);
    auto* saveButton = new QPushButton("保存 Profile", this);
    layout->addWidget(summary_);
    layout->addWidget(saveButton);
    layout->addWidget(refreshButton);
    layout->addStretch(1);
    connect(refreshButton, &QPushButton::clicked, this, &SettingsWidget::refresh);
    connect(saveButton, &QPushButton::clicked, this, &SettingsWidget::saveProfile);
    refresh();
}

void SettingsWidget::refresh() {
    hostEdit_->setText(config_.host);
    portEdit_->setValue(config_.port);
    defaultRoomEdit_->setText(config_.defaultRoom);
    playerPrefixEdit_->setText(config_.playerPrefix);
    summary_->setText(QString(
        "当前配置\n\n"
        "Gateway: %1:%2\n"
        "默认房间: %3\n"
        "当前用户: %4\n"
        "当前房间: %5\n"
        "当前 Battle: %6\n"
        "SDK version: %7\n"
        "Profile 文件: %8\n\n"
        "后续设置项：音量、按键、渲染比例、日志导出。")
        .arg(config_.host)
        .arg(config_.port)
        .arg(config_.defaultRoom)
        .arg(session_.userId)
        .arg(session_.roomId)
        .arg(session_.battleId)
        .arg(gateway_.sdkVersion())
        .arg(clientProfileStoragePath()));
}

void SettingsWidget::saveProfile() {
    config_.host = hostEdit_->text().trimmed();
    config_.port = static_cast<std::uint16_t>(portEdit_->value());
    config_.defaultRoom = defaultRoomEdit_->text().trimmed();
    config_.playerPrefix = playerPrefixEdit_->text().trimmed();
    saveClientProfile(ClientProfile{config_, session_.userId, tokenEdit_->text()});
    refresh();
}

}  // namespace bgtc

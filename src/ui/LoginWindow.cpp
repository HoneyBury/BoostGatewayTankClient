#include "ui/LoginWindow.h"

#include "ui/MainWindow.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bgtc {

LoginWindow::LoginWindow(AppConfig config, QWidget* parent)
    : QWidget(parent), config_(std::move(config)) {
    setWindowTitle("BoostGateway Tank Client - 登录");

    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("BoostGateway Tank Client 多人坦克客户端", this);
    title->setObjectName("titleLabel");
    layout->addWidget(title);

    auto* form = new QFormLayout();
    hostEdit_ = new QLineEdit(config_.host, this);
    portEdit_ = new QLineEdit(QString::number(config_.port), this);
    userEdit_ = new QLineEdit(config_.playerPrefix + "_1", this);
    tokenEdit_ = new QLineEdit("token:" + userEdit_->text(), this);
    form->addRow("服务地址", hostEdit_);
    form->addRow("服务端口", portEdit_);
    form->addRow("用户 ID", userEdit_);
    form->addRow("Token", tokenEdit_);
    layout->addLayout(form);

    statusLabel_ = new QLabel("请输入服务端地址和账号信息。", this);
    layout->addWidget(statusLabel_);

    loginButton_ = new QPushButton("连接并登录", this);
    registerButton_ = new QPushButton("注册账号（待服务端 SDK API）", this);
    layout->addWidget(loginButton_);
    layout->addWidget(registerButton_);
    connect(loginButton_, &QPushButton::clicked, this, &LoginWindow::handleLogin);
    connect(registerButton_, &QPushButton::clicked, this, &LoginWindow::handleRegisterHint);
    connect(userEdit_, &QLineEdit::textChanged, this, [this](const QString& user) {
        tokenEdit_->setText("token:" + user);
    });
}

void LoginWindow::handleLogin() {
    bool ok = false;
    const auto port = portEdit_->text().toUShort(&ok);
    if (!ok || port == 0) {
        QMessageBox::warning(this, "端口无效", "Gateway port 必须是有效端口。");
        return;
    }

    config_.host = hostEdit_->text();
    config_.port = port;

    auto* mainWindow = new MainWindow(config_, userEdit_->text(), tokenEdit_->text());
    mainWindow->resize(1100, 720);
    mainWindow->show();
    close();
}

void LoginWindow::handleRegisterHint() {
    QMessageBox::information(
        this,
        "注册能力说明",
        "客户端已预留注册入口。当前公共 SDK 暂未暴露 register 通用 API，"
        "后续应由服务端 SDK 补齐后在 GatewayClient::registerUser() 接入。");
}

void LoginWindow::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

}  // namespace bgtc

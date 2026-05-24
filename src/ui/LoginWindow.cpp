#include "ui/LoginWindow.h"

#include "core/ClientProfile.h"
#include "sdk/GatewayClient.h"
#include "ui/MainWindow.h"

#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bgtc {

LoginWindow::LoginWindow(AppConfig config, QString userId, QString token, QWidget* parent)
    : QWidget(parent), config_(std::move(config)) {
    setWindowTitle("BoostGateway Tank Client - 登录");

    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("BoostGateway Tank Client 多人坦克客户端", this);
    title->setObjectName("titleLabel");
    layout->addWidget(title);

    auto* form = new QFormLayout();
    hostEdit_ = new QLineEdit(config_.host, this);
    portEdit_ = new QLineEdit(QString::number(config_.port), this);
    if (userId.isEmpty()) {
        userId = config_.playerPrefix + "_1";
    }
    if (token.isEmpty()) {
        token = "token:" + userId;
    }
    userEdit_ = new QLineEdit(userId, this);
    tokenEdit_ = new QLineEdit(token, this);
    form->addRow("服务地址", hostEdit_);
    form->addRow("服务端口", portEdit_);
    form->addRow("用户 ID", userEdit_);
    form->addRow("Token", tokenEdit_);
    layout->addLayout(form);

    statusLabel_ = new QLabel("请输入服务端地址和账号信息。", this);
    layout->addWidget(statusLabel_);

    loginButton_ = new QPushButton("连接并登录", this);
    registerButton_ = new QPushButton("注册账号", this);
    layout->addWidget(loginButton_);
    layout->addWidget(registerButton_);
    connect(loginButton_, &QPushButton::clicked, this, &LoginWindow::handleLogin);
    connect(registerButton_, &QPushButton::clicked, this, &LoginWindow::handleRegister);
    connect(userEdit_, &QLineEdit::textChanged, this, [this](const QString& user) {
        tokenEdit_->setText("token:" + user);
    });
}

void LoginWindow::handleLogin() {
    if (!updateConfigFromForm()) {
        return;
    }

    saveClientProfile(ClientProfile{config_, userEdit_->text().trimmed(), tokenEdit_->text()});
    auto* mainWindow = new MainWindow(config_, userEdit_->text(), tokenEdit_->text());
    mainWindow->resize(1100, 720);
    mainWindow->show();
    close();
}

void LoginWindow::handleRegister() {
    if (!updateConfigFromForm()) {
        return;
    }
    const auto userId = userEdit_->text().trimmed();
    const auto credential = tokenEdit_->text();
    if (userId.isEmpty() || credential.isEmpty()) {
        QMessageBox::warning(this, "注册信息无效", "用户 ID 和 Token/凭证不能为空。");
        return;
    }

    GatewayClient gateway;
    QString error;
    setStatus("正在连接 gateway 并注册账号...");
    if (!gateway.connectToGateway(config_, &error)) {
        setStatus("注册失败：" + error);
        QMessageBox::warning(this, "注册失败", error);
        return;
    }
    if (!gateway.registerUser(userId, credential, userId, &error)) {
        setStatus("注册失败：" + error);
        QMessageBox::warning(this, "注册失败", error);
        return;
    }
    saveClientProfile(ClientProfile{config_, userId, credential});
    setStatus("注册成功，可以直接登录：" + userId);
    QMessageBox::information(this, "注册成功", "账号已注册，可以点击“连接并登录”。");
}

bool LoginWindow::updateConfigFromForm() {
    bool ok = false;
    const auto port = portEdit_->text().toUShort(&ok);
    if (!ok || port == 0) {
        QMessageBox::warning(this, "端口无效", "Gateway port 必须是有效端口。");
        return false;
    }

    config_.host = hostEdit_->text().trimmed();
    config_.port = port;
    return true;
}

void LoginWindow::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

}  // namespace bgtc

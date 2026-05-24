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
    setWindowTitle("BoostGateway Tank Client - Login");

    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("BoostGateway Tank Client", this);
    title->setObjectName("titleLabel");
    layout->addWidget(title);

    auto* form = new QFormLayout();
    hostEdit_ = new QLineEdit(config_.host, this);
    portEdit_ = new QLineEdit(QString::number(config_.port), this);
    userEdit_ = new QLineEdit(config_.playerPrefix + "_1", this);
    tokenEdit_ = new QLineEdit("token:" + userEdit_->text(), this);
    form->addRow("Gateway Host", hostEdit_);
    form->addRow("Gateway Port", portEdit_);
    form->addRow("User ID", userEdit_);
    form->addRow("Token", tokenEdit_);
    layout->addLayout(form);

    statusLabel_ = new QLabel("Ready.", this);
    layout->addWidget(statusLabel_);

    loginButton_ = new QPushButton("Connect and Login", this);
    layout->addWidget(loginButton_);
    connect(loginButton_, &QPushButton::clicked, this, &LoginWindow::handleLogin);
}

void LoginWindow::handleLogin() {
    bool ok = false;
    const auto port = portEdit_->text().toUShort(&ok);
    if (!ok || port == 0) {
        QMessageBox::warning(this, "Invalid Port", "Gateway port must be a valid number.");
        return;
    }

    config_.host = hostEdit_->text();
    config_.port = port;

    auto* mainWindow = new MainWindow(config_, userEdit_->text(), tokenEdit_->text());
    mainWindow->resize(1100, 720);
    mainWindow->show();
    close();
}

void LoginWindow::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

}  // namespace bgtc

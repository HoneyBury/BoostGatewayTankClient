#include "ui/LoginWindow.h"

#include "core/ClientProfile.h"
#include "sdk/GatewayClient.h"
#include "ui/MainWindow.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace bgtc {

LoginWindow::LoginWindow(AppConfig config, QString userId, QString token, QWidget* parent)
    : QWidget(parent), config_(std::move(config)) {
    setWindowTitle("BoostGateway Tank Client - 登录");
    resize(520, 680);
    setStyleSheet(R"(
        QWidget {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #0b1020, stop:0.45 #12304a, stop:1 #0f766e);
            color: #eef8ff;
            font-size: 15px;
        }
        QFrame#LoginCard {
            background: rgba(7, 18, 31, 0.88);
            border: 1px solid rgba(125, 211, 252, 0.28);
            border-radius: 28px;
        }
        QLabel#titleLabel {
            background: transparent;
            color: #ffffff;
            font-size: 26px;
            font-weight: 800;
        }
        QLabel#Avatar {
            background: qradialgradient(cx:0.35, cy:0.3, radius:0.8,
                stop:0 #e0f2fe, stop:0.55 #38bdf8, stop:1 #0f766e);
            border-radius: 42px;
            color: #06202f;
            font-size: 36px;
            font-weight: 900;
        }
        QLabel#LoginHint, QLabel#StatusLabel {
            background: transparent;
            color: #a9c7d8;
        }
        QLineEdit {
            background: rgba(8, 20, 34, 0.92);
            border: 1px solid rgba(148, 213, 234, 0.24);
            border-radius: 16px;
            color: #ecfeff;
            padding: 12px 14px;
            selection-background-color: #0f766e;
        }
        QPushButton {
            background: #0ea5e9;
            border: 0;
            border-radius: 16px;
            color: white;
            font-weight: 700;
            padding: 12px 18px;
        }
        QPushButton:hover {
            background: #22d3ee;
        }
        QPushButton#RegisterButton {
            background: rgba(20, 184, 166, 0.22);
            border: 1px solid rgba(45, 212, 191, 0.38);
        }
    )");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(46, 46, 46, 46);
    root->addStretch(1);

    auto* card = new QFrame(this);
    card->setObjectName("LoginCard");
    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(42);
    shadow->setOffset(0, 18);
    shadow->setColor(QColor(0, 0, 0, 150));
    card->setGraphicsEffect(shadow);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(34, 34, 34, 34);
    layout->setSpacing(16);

    auto* avatar = new QLabel("T", card);
    avatar->setObjectName("Avatar");
    avatar->setFixedSize(84, 84);
    avatar->setAlignment(Qt::AlignCenter);
    layout->addWidget(avatar, 0, Qt::AlignHCenter);

    auto* title = new QLabel("Tank Arena 登录", card);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* hint = new QLabel("像 QQ 登录一样轻一点：选好网关，带上玩家 ID，直接进入大厅。", card);
    hint->setObjectName("LoginHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    hostEdit_ = new QLineEdit(config_.host, card);
    hostEdit_->setPlaceholderText("服务地址，例如 127.0.0.1");
    portEdit_ = new QLineEdit(QString::number(config_.port), card);
    portEdit_->setPlaceholderText("服务端口，例如 18080");
    if (userId.isEmpty()) {
        userId = config_.playerPrefix + "_1";
    }
    if (token.isEmpty()) {
        token = "token:" + userId;
    }
    userEdit_ = new QLineEdit(userId, card);
    userEdit_->setPlaceholderText("用户 ID");
    tokenEdit_ = new QLineEdit(token, card);
    tokenEdit_->setPlaceholderText("Token / 凭证");
    layout->addWidget(hostEdit_);
    layout->addWidget(portEdit_);
    layout->addWidget(userEdit_);
    layout->addWidget(tokenEdit_);

    statusLabel_ = new QLabel("请输入服务端地址和账号信息。", card);
    statusLabel_->setObjectName("StatusLabel");
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    layout->addWidget(statusLabel_);

    auto* buttonRow = new QHBoxLayout();
    loginButton_ = new QPushButton("连接并登录", card);
    registerButton_ = new QPushButton("注册账号", card);
    registerButton_->setObjectName("RegisterButton");
    buttonRow->addWidget(registerButton_);
    buttonRow->addWidget(loginButton_);
    layout->addLayout(buttonRow);

    root->addWidget(card);
    root->addStretch(1);

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

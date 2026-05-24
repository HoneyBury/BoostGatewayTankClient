#pragma once

#include "core/AppConfig.h"

#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;

namespace bgtc {

class LoginWindow final : public QWidget {
    Q_OBJECT

public:
    explicit LoginWindow(AppConfig config,
                         QString userId = {},
                         QString token = {},
                         QWidget* parent = nullptr);

private:
    void handleLogin();
    void handleRegister();
    void setStatus(const QString& text);
    [[nodiscard]] bool updateConfigFromForm();

    AppConfig config_;
    QLineEdit* hostEdit_ = nullptr;
    QLineEdit* portEdit_ = nullptr;
    QLineEdit* userEdit_ = nullptr;
    QLineEdit* tokenEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* loginButton_ = nullptr;
    QPushButton* registerButton_ = nullptr;
};

}  // namespace bgtc

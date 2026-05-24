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
    explicit LoginWindow(AppConfig config, QWidget* parent = nullptr);

private:
    void handleLogin();
    void setStatus(const QString& text);

    AppConfig config_;
    QLineEdit* hostEdit_ = nullptr;
    QLineEdit* portEdit_ = nullptr;
    QLineEdit* userEdit_ = nullptr;
    QLineEdit* tokenEdit_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QPushButton* loginButton_ = nullptr;
};

}  // namespace bgtc

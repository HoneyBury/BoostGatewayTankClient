#include "ui/LeaderboardWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bgtc {

LeaderboardWidget::LeaderboardWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    auto* actions = new QHBoxLayout();
    auto* topButton = new QPushButton("刷新 Top 10", this);
    auto* mineButton = new QPushButton("查询我的排名", this);
    actions->addWidget(topButton);
    actions->addWidget(mineButton);
    actions->addStretch(1);
    layout->addLayout(actions);

    output_ = new QTextEdit(this);
    output_->setReadOnly(true);
    output_->setPlaceholderText("排行榜结果会显示在这里。");
    layout->addWidget(output_, 1);

    connect(topButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshTop);
    connect(mineButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshMine);
}

void LeaderboardWidget::refreshTop() {
    QString error;
    const auto result = gateway_.queryLeaderboardTop(10, &error);
    setOutput(error.isEmpty() ? result : "查询 Top 10 失败：\n" + error);
}

void LeaderboardWidget::refreshMine() {
    QString error;
    const auto result = gateway_.queryLeaderboardRank(session_.userId, &error);
    setOutput(error.isEmpty() ? result : "查询我的排名失败：\n" + error);
}

void LeaderboardWidget::setOutput(const QString& text) {
    output_->setPlainText(text);
}

}  // namespace bgtc

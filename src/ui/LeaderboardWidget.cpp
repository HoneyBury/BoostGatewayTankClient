#include "ui/LeaderboardWidget.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    auto* afterBattleButton = new QPushButton("战斗后刷新", this);
    actions->addWidget(topButton);
    actions->addWidget(mineButton);
    actions->addWidget(afterBattleButton);
    actions->addStretch(1);
    layout->addLayout(actions);

    output_ = new QTextEdit(this);
    output_->setReadOnly(true);
    output_->setPlaceholderText("排行榜结果会显示在这里。");
    layout->addWidget(output_, 1);

    connect(topButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshTop);
    connect(mineButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshMine);
    connect(afterBattleButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshAfterBattle);
}

void LeaderboardWidget::refreshAfterBattle() {
    QString error;
    const auto top = gateway_.queryLeaderboardTop(10, &error);
    if (!error.isEmpty()) {
        setOutput(settlementSummary() + "\n\n刷新排行榜失败：\n" + error);
        return;
    }
    setOutput(settlementSummary() + "\n\n" + formatLeaderboardJson(top));
}

void LeaderboardWidget::refreshTop() {
    QString error;
    const auto result = gateway_.queryLeaderboardTop(10, &error);
    setOutput(error.isEmpty() ? formatLeaderboardJson(result) : "查询 Top 10 失败：\n" + error);
}

void LeaderboardWidget::refreshMine() {
    QString error;
    const auto result = gateway_.queryLeaderboardRank(session_.userId, &error);
    setOutput(error.isEmpty() ? formatLeaderboardJson(result) : "查询我的排名失败：\n" + error);
}

QString LeaderboardWidget::formatLeaderboardJson(const QString& body) const {
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        return body;
    }

    const auto root = doc.object();
    QStringList lines;
    lines << "排行榜";
    if (root.contains("status")) {
        lines << "状态: " + root.value("status").toString();
    }
    if (root.contains("entries") && root.value("entries").isArray()) {
        const auto entries = root.value("entries").toArray();
        for (const auto& item : entries) {
            const auto entry = item.toObject();
            lines << QString("#%1  %2  score=%3")
                         .arg(entry.value("rank").toInt())
                         .arg(entry.value("display_name").toString(entry.value("user_id").toString()))
                         .arg(entry.value("score").toInt());
        }
        if (entries.isEmpty()) {
            lines << "暂无排行榜数据";
        }
    } else if (root.contains("rank")) {
        lines << QString("#%1  %2  score=%3")
                     .arg(root.value("rank").toInt())
                     .arg(root.value("display_name").toString(root.value("user_id").toString(session_.userId)))
                     .arg(root.value("score").toInt());
    } else {
        lines << QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    }
    return lines.join('\n');
}

QString LeaderboardWidget::settlementSummary() const {
    QStringList lines;
    lines << "最近一局结算";
    lines << "Battle: " + (session_.battleId.isEmpty() ? "未知" : session_.battleId);
    lines << "Winner: " + (session_.lastWinnerUserId.isEmpty() ? "未知" : session_.lastWinnerUserId);
    lines << "Reason: " + (session_.lastFinishReason.isEmpty() ? "unknown" : session_.lastFinishReason);
    lines << QString("Frames: %1").arg(session_.lastBattleFrames);
    lines << QString("My Score: %1").arg(session_.lastBattleScore);
    return lines.join('\n');
}

void LeaderboardWidget::setOutput(const QString& text) {
    output_->setPlainText(text);
}

}  // namespace bgtc

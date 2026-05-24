#include "ui/LeaderboardWidget.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bgtc {

LeaderboardWidget::LeaderboardWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(14);

    auto* title = new QLabel("排行榜", this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto* hint = new QLabel("查看全服 Top 10、我的排名，或在战斗结束后合并显示最近结算。", this);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* actions = new QHBoxLayout();
    actions->setSpacing(10);
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
    output_->setPlaceholderText("排行榜结果、最近结算和查询错误会显示在这里。");
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
    lines << "排行榜 Top 10";
    lines << "----------------";
    if (root.contains("status")) {
        lines << "状态：" + root.value("status").toString();
        lines << "";
    }
    if (root.contains("entries") && root.value("entries").isArray()) {
        const auto entries = root.value("entries").toArray();
        for (const auto& item : entries) {
            const auto entry = item.toObject();
            lines << QString("#%1  %2    分数 %3")
                         .arg(entry.value("rank").toInt())
                         .arg(entry.value("display_name").toString(entry.value("user_id").toString()))
                         .arg(entry.value("score").toInt());
        }
        if (entries.isEmpty()) {
            lines << "暂无排行榜数据";
        }
    } else if (root.contains("rank")) {
        lines << "我的排名";
        lines << "----------------";
        lines << QString("#%1  %2    分数 %3")
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
    lines << "----------------";
    lines << "Battle：" + (session_.battleId.isEmpty() ? "未知" : session_.battleId);
    lines << "胜者：" + (session_.lastWinnerUserId.isEmpty() ? "未知" : session_.lastWinnerUserId);
    lines << "原因：" + (session_.lastFinishReason.isEmpty() ? "unknown" : session_.lastFinishReason);
    lines << QString("总帧数：%1").arg(session_.lastBattleFrames);
    lines << QString("我的分数：%1").arg(session_.lastBattleScore);
    return lines.join('\n');
}

void LeaderboardWidget::setOutput(const QString& text) {
    output_->setPlainText(text);
}

}  // namespace bgtc

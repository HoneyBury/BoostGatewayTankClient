#include "ui/LeaderboardWidget.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
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

    auto* hint = new QLabel("排行榜使用表格展示，不再直接暴露服务端 JSON；战斗结束后会保留最近结算摘要。", this);
    hint->setObjectName("PageHint");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto* actions = new QHBoxLayout();
    auto* topButton = new QPushButton("刷新 Top 10", this);
    auto* mineButton = new QPushButton("查询我的排名", this);
    auto* afterBattleButton = new QPushButton("战斗后刷新", this);
    actions->addWidget(topButton);
    actions->addWidget(mineButton);
    actions->addWidget(afterBattleButton);
    actions->addStretch(1);
    layout->addLayout(actions);

    settlementLabel_ = new QLabel(settlementSummary(), this);
    settlementLabel_->setObjectName("StatePill");
    settlementLabel_->setWordWrap(true);
    layout->addWidget(settlementLabel_);

    table_ = new QTableWidget(this);
    table_->setColumnCount(4);
    table_->setHorizontalHeaderLabels({"排名", "玩家", "用户 ID", "分数"});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    layout->addWidget(table_, 1);

    statusLabel_ = new QLabel("点击按钮刷新排行榜。", this);
    statusLabel_->setObjectName("PageHint");
    layout->addWidget(statusLabel_);

    connect(topButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshTop);
    connect(mineButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshMine);
    connect(afterBattleButton, &QPushButton::clicked, this, &LeaderboardWidget::refreshAfterBattle);
}

void LeaderboardWidget::refreshAfterBattle() {
    settlementLabel_->setText(settlementSummary());
    refreshTop();
}

void LeaderboardWidget::refreshTop() {
    QString error;
    const auto result = gateway_.queryLeaderboardTop(10, &error);
    if (!error.isEmpty()) {
        setStatus("查询 Top 10 失败：" + error);
        QMessageBox::warning(this, "排行榜查询失败", error);
        return;
    }
    renderLeaderboard(result, false);
}

void LeaderboardWidget::refreshMine() {
    QString error;
    const auto result = gateway_.queryLeaderboardRank(session_.userId, &error);
    if (!error.isEmpty()) {
        setStatus("查询我的排名失败：" + error);
        QMessageBox::warning(this, "排行榜查询失败", error);
        return;
    }
    renderLeaderboard(result, true);
}

void LeaderboardWidget::renderLeaderboard(const QString& body, bool mineOnly) {
    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (!doc.isObject()) {
        setStatus("排行榜返回无法解析。");
        QMessageBox::warning(this, "排行榜返回异常", "排行榜返回无法解析。");
        return;
    }

    table_->setRowCount(0);
    const auto root = doc.object();
    if (mineOnly || root.contains("rank")) {
        table_->setRowCount(1);
        setRankCell(0, 0, QString::number(root.value("rank").toInt()));
        setRankCell(0, 1, root.value("display_name").toString(root.value("user_id").toString(session_.userId)));
        setRankCell(0, 2, root.value("user_id").toString(session_.userId));
        setRankCell(0, 3, QString::number(root.value("score").toInt()));
        setStatus("我的排名已刷新。");
        return;
    }

    const auto entries = root.value("entries").toArray();
    table_->setRowCount(entries.size());
    for (int row = 0; row < entries.size(); ++row) {
        const auto entry = entries.at(row).toObject();
        setRankCell(row, 0, QString::number(entry.value("rank").toInt(row + 1)));
        setRankCell(row, 1, entry.value("display_name").toString(entry.value("user_id").toString()));
        setRankCell(row, 2, entry.value("user_id").toString());
        setRankCell(row, 3, QString::number(entry.value("score").toInt()));
    }
    setStatus(entries.isEmpty() ? "暂无排行榜数据。" : "Top 10 已刷新。");
}

void LeaderboardWidget::setRankCell(int row, int column, const QString& text) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    table_->setItem(row, column, item);
}

QString LeaderboardWidget::settlementSummary() const {
    return QString("最近一局：Battle=%1    胜者=%2    原因=%3    总帧数=%4    我的分数=%5")
        .arg(session_.battleId.isEmpty() ? "未知" : session_.battleId,
             session_.lastWinnerUserId.isEmpty() ? "未知" : session_.lastWinnerUserId,
             session_.lastFinishReason.isEmpty() ? "unknown" : session_.lastFinishReason)
        .arg(session_.lastBattleFrames)
        .arg(session_.lastBattleScore);
}

void LeaderboardWidget::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

}  // namespace bgtc

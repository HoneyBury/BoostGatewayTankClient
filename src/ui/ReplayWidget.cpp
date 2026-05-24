#include "ui/ReplayWidget.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bgtc {

ReplayWidget::ReplayWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    status_ = new QLabel("回放模块：可加载当前 battle_id 的服务端回放帧。", this);
    auto* loadButton = new QPushButton("加载当前 Battle 回放", this);
    output_ = new QTextEdit(this);
    output_->setReadOnly(true);
    output_->setPlaceholderText("这里会显示回放帧、时间轴和播放状态。");
    layout->addWidget(status_);
    layout->addWidget(loadButton);
    layout->addWidget(output_, 1);
    connect(loadButton, &QPushButton::clicked, this, &ReplayWidget::loadReplay);
}

void ReplayWidget::loadReplay() {
    if (session_.battleId.isEmpty()) {
        output_->setPlainText("当前没有 battle_id。请先完成一局战斗，或等待重连恢复战斗状态。");
        return;
    }

    QString error;
    const auto body = gateway_.loadReplay(session_.battleId, &error);
    if (!error.isEmpty()) {
        output_->setPlainText("加载回放失败：" + error + "\n当前 battle_id: " + session_.battleId);
        return;
    }

    const auto doc = QJsonDocument::fromJson(body.toUtf8());
    if (doc.isObject()) {
        const auto root = doc.object();
        const auto replay = root.value("replay").toObject();
        status_->setText(QString("回放已加载：battle=%1，frame_count=%2")
                             .arg(session_.battleId)
                             .arg(replay.value("frame_count").toInt()));
        output_->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
        return;
    }
    output_->setPlainText(body);
}

}  // namespace bgtc

#include "ui/ReplayWidget.h"

#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace bgtc {

ReplayWidget::ReplayWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent)
    : QWidget(parent), session_(session), gateway_(gateway) {
    auto* layout = new QVBoxLayout(this);
    status_ = new QLabel("回放模块已预留：等待服务端 SDK 暴露 replay list/load 通用 API。", this);
    auto* loadButton = new QPushButton("加载当前 Battle 回放", this);
    output_ = new QTextEdit(this);
    output_->setReadOnly(true);
    output_->setPlaceholderText("后续这里会显示回放帧、时间轴和播放状态。");
    layout->addWidget(status_);
    layout->addWidget(loadButton);
    layout->addWidget(output_, 1);
    connect(loadButton, &QPushButton::clicked, this, &ReplayWidget::loadReplay);
}

void ReplayWidget::loadReplay() {
    output_->setPlainText(
        "当前 battle_id: " + session_.battleId + "\n\n" +
        gateway_.unsupportedFeatureMessage("回放查询/加载"));
}

}  // namespace bgtc

#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;
class QTableWidget;
class QTextEdit;
class QTimer;

namespace bgtc {

class RoomWidget final : public QWidget {
    Q_OBJECT

public:
    RoomWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);
    void refreshRoomDetail();

signals:
    void battleStarted(QString battleId);
    void leftRoom();
    void returnToBattleRequested();
    void themeChanged(QString themeName);
    void loadingRequested(QString battleId);

private:
    void setReady(bool ready);
    void startBattle();
    void leaveRoom();
    void kickMember(const QString& userId);
    void transferOwner(const QString& userId);
    void renderRoomDetail(const QString& body);
    void setMemberCell(int row, int column, const QString& text);
    void appendLog(const QString& text);
    [[nodiscard]] bool requireRoom(const QString& action) const;

    ClientSession& session_;
    GatewayClient& gateway_;
    QLabel* roomState_ = nullptr;
    QTableWidget* memberTable_ = nullptr;
    QTextEdit* log_ = nullptr;
    QTimer* refreshTimer_ = nullptr;
};

}  // namespace bgtc

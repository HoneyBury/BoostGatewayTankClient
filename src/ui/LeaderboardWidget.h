#pragma once

#include "core/ClientState.h"
#include "sdk/GatewayClient.h"

#include <QWidget>

class QLabel;
class QTableWidget;

namespace bgtc {

class LeaderboardWidget final : public QWidget {
    Q_OBJECT

public:
    LeaderboardWidget(ClientSession& session, GatewayClient& gateway, QWidget* parent = nullptr);
    void refreshAfterBattle();
    void refreshTop();

private:
    void refreshMine();
    void renderLeaderboard(const QString& body, bool mineOnly);
    void setRankCell(int row, int column, const QString& text);
    [[nodiscard]] QString settlementSummary() const;
    void setStatus(const QString& text);

    ClientSession& session_;
    GatewayClient& gateway_;
    QLabel* statusLabel_ = nullptr;
    QLabel* settlementLabel_ = nullptr;
    QTableWidget* table_ = nullptr;
};

}  // namespace bgtc

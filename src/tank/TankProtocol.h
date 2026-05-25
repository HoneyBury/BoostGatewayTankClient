#pragma once

#include "tank/TankWorldModel.h"

#include <optional>
#include <string>
#include <vector>

namespace bgtc {

enum class TankActionType {
    Move,
    Fire,
    Stop,
};

struct TankAction {
    TankActionType type = TankActionType::Stop;
    int dx = 0;
    int dy = 0;
    int direction = 0;
};

struct TankInput {
    std::uint64_t seq = 0;
    std::vector<TankAction> actions;
};

std::string encodeTankInput(const TankInput& input);
std::string encodeLegacyMoveInput(int x, int y);
std::string encodeLegacyFireInput(int dx, int dy);
std::string encodeLegacyAttackInput(const std::string& targetUserId);
std::string encodeLegacyFinishInput(const std::string& reason);
std::string encodeLegacyPickupInput(const std::string& itemId);
std::optional<TankSnapshot> decodeTankSnapshot(const std::string& payload);
std::optional<BattleStateEvent> decodeBattleStateEvent(const std::string& payload);
std::optional<ReplayTimeline> decodeReplayTimeline(const std::string& payload);
std::optional<MatchFoundState> decodeMatchFoundState(const std::string& payload);

TankInput makeMoveInput(std::uint64_t seq, int dx, int dy);
TankInput makeFireInput(std::uint64_t seq, int direction);
TankInput makeStopInput(std::uint64_t seq);

}  // namespace bgtc

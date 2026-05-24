#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bgtc {

struct TankState {
    std::string userId;
    int x = 0;
    int y = 0;
    int hp = 100;
    int direction = 0;
    bool alive = true;
    int kills = 0;
    int deaths = 0;
    int score = 0;
};

struct BulletState {
    std::uint64_t id = 0;
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    std::string owner;
};

struct TankEvent {
    std::string type;
    std::string actor;
    std::string target;
    std::string itemId;
    std::string itemType;
    std::string buffType;
    int damage = 0;
    int frame = 0;
    int remainingTicks = 0;
};

struct ItemState {
    std::string id;
    std::string type;
    int x = 0;
    int y = 0;
    int remainingTicks = 0;
};

struct BattleStateEvent {
    std::string kind;
    std::string roomId;
    std::string battleId;
    int frame = 0;
    std::string trigger;
    std::string reason;
    std::string userId;
};

struct BattleScoreState {
    std::string userId;
    int score = 0;
};

struct BuffState {
    std::string userId;
    std::string type;
    int remainingTicks = 0;
};

struct TankSnapshot {
    int frame = 0;
    int totalFrames = 0;
    bool finished = false;
    std::string finishReason;
    std::string winnerUserId;
    std::vector<TankState> tanks;
    std::vector<BulletState> bullets;
    std::vector<TankEvent> events;
    std::vector<ItemState> items;
    std::vector<BuffState> buffs;
    std::vector<BattleScoreState> scores;
    std::optional<BattleStateEvent> battleState;
};

struct ReplayFrame {
    int frameNumber = 0;
    std::int64_t timestamp = 0;
    TankSnapshot snapshot;
};

struct ReplayTimeline {
    std::string battleId;
    std::vector<ReplayFrame> frames;
};

}  // namespace bgtc

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace bgtc {

struct TankState {
    std::string userId;
    double x = 0.0;
    double y = 0.0;
    int hp = 100;
    int direction = 0;
    int directionX = 1;
    int directionY = 0;
    int speedMultiplier = 1;
    bool alive = true;
    int kills = 0;
    int deaths = 0;
    int score = 0;
};

struct BulletState {
    std::string id;
    double x = 0.0;
    double y = 0.0;
    double dx = 0.0;
    double dy = 0.0;
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
    double x = 0.0;
    double y = 0.0;
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

struct MatchPlayerView {
    std::string userId;
    std::int64_t mmr = 0;
};

struct MatchFoundState {
    std::string matchId;
    std::string roomId;
    std::string mode;
    std::vector<MatchPlayerView> players;
};

}  // namespace bgtc

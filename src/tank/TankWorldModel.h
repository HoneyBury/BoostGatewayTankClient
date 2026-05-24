#pragma once

#include <cstdint>
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
    int damage = 0;
    int frame = 0;
};

struct TankSnapshot {
    int frame = 0;
    bool finished = false;
    std::string finishReason;
    std::string winnerUserId;
    std::vector<TankState> tanks;
    std::vector<BulletState> bullets;
    std::vector<TankEvent> events;
};

}  // namespace bgtc

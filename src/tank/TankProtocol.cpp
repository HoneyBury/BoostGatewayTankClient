#include "tank/TankProtocol.h"

#include <nlohmann/json.hpp>

namespace bgtc {
namespace {

using json = nlohmann::json;

TankState parseTank(const json& value) {
    TankState tank;
    tank.userId = value.value("user_id", "");
    tank.x = value.value("x", 0);
    tank.y = value.value("y", 0);
    tank.hp = value.value("hp", 100);
    tank.direction = value.value("direction", 0);
    tank.alive = value.value("alive", true);
    tank.kills = value.value("kills", 0);
    tank.deaths = value.value("deaths", 0);
    tank.score = value.value("score", 0);
    return tank;
}

BulletState parseBullet(const json& value) {
    BulletState bullet;
    bullet.id = value.value("id", std::uint64_t{0});
    bullet.x = value.value("x", 0);
    bullet.y = value.value("y", 0);
    bullet.dx = value.value("dx", 0);
    bullet.dy = value.value("dy", 0);
    bullet.owner = value.value("owner", "");
    return bullet;
}

TankEvent parseEvent(const json& value) {
    TankEvent event;
    event.type = value.value("type", "");
    event.actor = value.value("actor", "");
    event.target = value.value("target", "");
    event.damage = value.value("damage", 0);
    event.frame = value.value("frame", 0);
    return event;
}

}  // namespace

std::string encodeTankInput(const TankInput& input) {
    json actions = json::array();
    for (const auto& action : input.actions) {
        switch (action.type) {
            case TankActionType::Move:
                actions.push_back({
                    {"type", "move"},
                    {"dx", action.dx},
                    {"dy", action.dy},
                });
                break;
            case TankActionType::Fire:
                actions.push_back({
                    {"type", "fire"},
                    {"direction", action.direction},
                });
                break;
            case TankActionType::Stop:
                actions.push_back({{"type", "stop"}});
                break;
        }
    }
    return json{{"seq", input.seq}, {"actions", std::move(actions)}}.dump();
}

std::optional<TankSnapshot> decodeTankSnapshot(const std::string& payload) {
    auto doc = json::parse(payload, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }

    if (doc.contains("payload") && doc["payload"].is_string()) {
        return decodeTankSnapshot(doc["payload"].get<std::string>());
    }

    if (doc.value("type", "") != "tank.snapshot" && !doc.contains("tanks")) {
        return std::nullopt;
    }

    TankSnapshot snapshot;
    snapshot.frame = doc.value("frame", 0);
    snapshot.finished = doc.value("finished", false);
    snapshot.finishReason = doc.value("finish_reason", "");
    snapshot.winnerUserId = doc.value("winner_user_id", "");

    if (doc.contains("tanks") && doc["tanks"].is_array()) {
        for (const auto& tank : doc["tanks"]) {
            snapshot.tanks.push_back(parseTank(tank));
        }
    }
    if (doc.contains("bullets") && doc["bullets"].is_array()) {
        for (const auto& bullet : doc["bullets"]) {
            snapshot.bullets.push_back(parseBullet(bullet));
        }
    }
    if (doc.contains("events") && doc["events"].is_array()) {
        for (const auto& event : doc["events"]) {
            snapshot.events.push_back(parseEvent(event));
        }
    }
    return snapshot;
}

TankInput makeMoveInput(std::uint64_t seq, int dx, int dy) {
    return TankInput{seq, {TankAction{TankActionType::Move, dx, dy, 0}}};
}

TankInput makeFireInput(std::uint64_t seq, int direction) {
    return TankInput{seq, {TankAction{TankActionType::Fire, 0, 0, direction}}};
}

TankInput makeStopInput(std::uint64_t seq) {
    return TankInput{seq, {TankAction{TankActionType::Stop, 0, 0, 0}}};
}

}  // namespace bgtc

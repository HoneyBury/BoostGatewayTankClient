#include "tank/TankProtocol.h"

#include <nlohmann/json.hpp>

#include <sstream>

namespace bgtc {
namespace {

using json = nlohmann::json;

TankState parseTank(const json& value) {
    TankState tank;
    tank.userId = value.value("user_id", "");
    tank.x = value.value("x", value.value("pos_x", 0));
    tank.y = value.value("y", value.value("pos_y", 0));
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

ItemState parseItem(const json& value) {
    ItemState item;
    item.id = value.value("id", "");
    item.type = value.value("type", "");
    item.x = value.value("x", 0);
    item.y = value.value("y", 0);
    item.remainingTicks = value.value("remaining_ticks", 0);
    return item;
}

BattleScoreState parseScore(const json& value) {
    BattleScoreState score;
    score.userId = value.value("user_id", "");
    score.score = value.value("score", 0);
    return score;
}

std::vector<std::string> split(const std::string& value, char delimiter) {
    std::vector<std::string> parts;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, delimiter)) {
        parts.push_back(item);
    }
    return parts;
}

std::optional<BattleStateEvent> parseBattleStateSegments(const std::string& payload) {
    if (payload.rfind("battle_state:", 0) != 0) {
        return std::nullopt;
    }
    BattleStateEvent event;
    for (const auto& segment : split(payload.substr(std::string("battle_state:").size()), ':')) {
        const auto pos = segment.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const auto key = segment.substr(0, pos);
        const auto value = segment.substr(pos + 1);
        if (key == "kind") event.kind = value;
        else if (key == "room_id") event.roomId = value;
        else if (key == "battle_id") event.battleId = value;
        else if (key == "frame") event.frame = std::stoi(value);
        else if (key == "trigger") event.trigger = value;
        else if (key == "reason") event.reason = value;
        else if (key == "user_id") event.userId = value;
    }
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

std::string encodeLegacyMoveInput(int x, int y) {
    return "move:" + std::to_string(x) + "," + std::to_string(y);
}

std::string encodeLegacyAttackInput(const std::string& targetUserId) {
    return "attack:" + targetUserId;
}

std::string encodeLegacyFinishInput(const std::string& reason) {
    return "finish:" + reason;
}

std::optional<TankSnapshot> decodeTankSnapshot(const std::string& payload) {
    auto doc = json::parse(payload, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        if (auto battleState = decodeBattleStateEvent(payload)) {
            TankSnapshot snapshot;
            snapshot.frame = battleState->frame;
            snapshot.finished = battleState->kind == "finished" || battleState->kind == "settlement";
            snapshot.finishReason = battleState->reason;
            snapshot.battleState = *battleState;
            return snapshot;
        }
        return std::nullopt;
    }

    if (doc.contains("payload") && doc["payload"].is_string()) {
        return decodeTankSnapshot(doc["payload"].get<std::string>());
    }

    if (doc.value("type", "") != "tank.snapshot" && !doc.contains("tanks") &&
        !doc.contains("participants") && doc.value("kind", "") != "battle_finished") {
        return std::nullopt;
    }

    TankSnapshot snapshot;
    snapshot.frame = doc.value("frame", doc.value("frame_number", 0));
    snapshot.totalFrames = doc.value("total_frames", snapshot.frame);
    const auto kind = doc.value("kind", "");
    snapshot.finished = doc.value("finished", kind == "battle_finished" || kind == "finished" || kind == "settlement");
    snapshot.finishReason = doc.value("finish_reason", doc.value("reason", ""));
    snapshot.winnerUserId = doc.value("winner_user_id", "");

    if (!kind.empty()) {
        BattleStateEvent event;
        event.kind = kind;
        event.roomId = doc.value("room_id", "");
        event.battleId = doc.value("battle_id", "");
        event.frame = snapshot.frame;
        event.trigger = doc.value("trigger", "");
        event.reason = snapshot.finishReason;
        event.userId = doc.value("user_id", "");
        snapshot.battleState = event;
    }

    if (doc.contains("tanks") && doc["tanks"].is_array()) {
        for (const auto& tank : doc["tanks"]) {
            snapshot.tanks.push_back(parseTank(tank));
        }
    }
    if (doc.contains("participants") && doc["participants"].is_array()) {
        for (const auto& participant : doc["participants"]) {
            snapshot.tanks.push_back(parseTank(participant));
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
    if (doc.contains("items") && doc["items"].is_array()) {
        for (const auto& item : doc["items"]) {
            snapshot.items.push_back(parseItem(item));
        }
    }
    if (doc.contains("scores") && doc["scores"].is_array()) {
        for (const auto& score : doc["scores"]) {
            snapshot.scores.push_back(parseScore(score));
        }
    }
    return snapshot;
}

std::optional<BattleStateEvent> decodeBattleStateEvent(const std::string& payload) {
    if (auto parsed = parseBattleStateSegments(payload)) {
        return parsed;
    }

    auto doc = json::parse(payload, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return std::nullopt;
    }
    if (doc.contains("payload") && doc["payload"].is_string()) {
        return decodeBattleStateEvent(doc["payload"].get<std::string>());
    }
    const auto kind = doc.value("kind", "");
    if (kind.empty()) {
        return std::nullopt;
    }
    BattleStateEvent event;
    event.kind = kind;
    event.roomId = doc.value("room_id", "");
    event.battleId = doc.value("battle_id", "");
    event.frame = doc.value("frame", doc.value("frame_number", 0));
    event.trigger = doc.value("trigger", "");
    event.reason = doc.value("reason", "");
    event.userId = doc.value("user_id", "");
    return event;
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

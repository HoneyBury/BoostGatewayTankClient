#include "boost_gateway/sdk/client.h"
#include "tank/TankProtocol.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace sdk = boost_gateway::sdk;
using namespace std::chrono_literals;

namespace {

struct Step {
    std::string name;
    std::string category;
    bool passed = false;
    std::string detail;
};

struct GateState {
    std::vector<Step> steps;
    std::vector<sdk::PushMessage> pushes;
    std::mutex mutex;

    void add_step(std::string name, bool passed, std::string detail = {}, std::string category = {}) {
        steps.push_back({std::move(name), std::move(category), passed, std::move(detail)});
    }

    [[nodiscard]] bool overall_pass() const {
        for (const auto& step : steps) {
            if (!step.passed) return false;
        }
        return true;
    }

    [[nodiscard]] bool saw_push_fragment(const std::string& fragment) const {
        for (const auto& push : pushes) {
            if (push.body.find(fragment) != std::string::npos) return true;
        }
        return false;
    }

    [[nodiscard]] int max_frame() const {
        int frame = 0;
        for (const auto& push : pushes) {
            if (auto state = bgtc::decodeBattleStateEvent(push.body)) {
                frame = std::max(frame, state->frame);
            }
        }
        return frame;
    }
};

std::string json_escape(const std::string& value) {
    std::string out;
    for (const auto ch : value) {
        if (ch == '\\' || ch == '"') {
            out.push_back('\\');
        }
        if (ch == '\n') {
            out += "\\n";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

void write_summary(const GateState& state,
                   const std::string& host,
                   std::uint16_t port,
                   const std::string& room_id) {
    std::ofstream out("tank-client-headless-summary.json");
    out << "{\n";
    out << "  \"overall_pass\": " << (state.overall_pass() ? "true" : "false") << ",\n";
    out << "  \"host\": \"" << json_escape(host) << "\",\n";
    out << "  \"port\": " << port << ",\n";
    out << "  \"room_id\": \"" << json_escape(room_id) << "\",\n";
    out << "  \"push_count\": " << state.pushes.size() << ",\n";
    out << "  \"max_frame\": " << state.max_frame() << ",\n";
    out << "  \"steps\": [\n";
    for (std::size_t i = 0; i < state.steps.size(); ++i) {
        const auto& step = state.steps[i];
        out << "    {\"name\":\"" << json_escape(step.name)
            << "\",\"category\":\"" << json_escape(step.category)
            << "\",\"passed\":" << (step.passed ? "true" : "false")
            << ",\"detail\":\"" << json_escape(step.detail) << "\"}";
        if (i + 1 < state.steps.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(argc > 2 ? std::atoi(argv[2]) : 9201);
    const auto run_id = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto alice_id = "qt_gate_alice_" + run_id;
    const auto bob_id = "qt_gate_bob_" + run_id;
    const auto charlie_id = "qt_gate_charlie_" + run_id;
    const auto room_id = "qt_gate_room_" + run_id;

    GateState state;
    sdk::SdkClient alice;
    sdk::SdkClient bob;
    sdk::SdkClient charlie;
    auto on_push = [&](const sdk::PushMessage& push) {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.pushes.push_back(push);
        std::cout << "[push] id=" << push.message_id << " body=" << push.body << "\n";
    };
    alice.on_push(on_push);
    bob.on_push(on_push);
    charlie.on_push(on_push);

    constexpr auto timeout = 10s;
    const auto alice_connected = alice.connect(host, port, timeout);
    const auto bob_connected = bob.connect(host, port, timeout);
    const auto charlie_connected = charlie.connect(host, port, timeout);
    state.add_step("connect_three_clients", alice_connected && bob_connected && charlie_connected);
    if (!alice_connected || !bob_connected || !charlie_connected) {
        write_summary(state, host, port, room_id);
        return 2;
    }

    const auto alice_register = alice.register_account(alice_id, "token:" + alice_id, alice_id, timeout);
    const auto bob_register = bob.register_account(bob_id, "token:" + bob_id, bob_id, timeout);
    const auto charlie_register = charlie.register_account(charlie_id, "token:" + charlie_id, charlie_id, timeout);
    state.add_step("register_three_users",
                   alice_register.ok && bob_register.ok && charlie_register.ok,
                   alice_register.error_message + " | " + bob_register.error_message + " | " +
                       charlie_register.error_message);

    const auto alice_login = alice.login(alice_id, "token:" + alice_id, timeout);
    const auto bob_login = bob.login(bob_id, "token:" + bob_id, timeout);
    const auto charlie_login = charlie.login(charlie_id, "token:" + charlie_id, timeout);
    state.add_step("login_three_users", alice_login.ok && bob_login.ok && charlie_login.ok,
                   alice_login.error_message + " | " + bob_login.error_message + " | " +
                       charlie_login.error_message);

    const auto create = alice.create_room(room_id, timeout);
    state.add_step("create_room", create.ok, create.error_message);

    const auto join = bob.join_room(room_id, timeout);
    state.add_step("join_room", join.ok, join.error_message);

    const auto join_charlie = charlie.join_room(room_id, timeout);
    state.add_step("join_third_member", join_charlie.ok, join_charlie.error_message);

    const auto kick_charlie = alice.room_kick(charlie_id, timeout);
    const auto detail_after_kick = alice.room_detail(room_id, timeout);
    state.add_step("room_kick_member",
                   kick_charlie.ok &&
                       detail_after_kick.ok &&
                       detail_after_kick.response_body.find(charlie_id) == std::string::npos,
                   detail_after_kick.response_body.empty()
                       ? kick_charlie.error_message
                       : detail_after_kick.response_body);

    const auto transfer_owner = alice.room_transfer_owner(bob_id, timeout);
    const auto detail_after_transfer = alice.room_detail(room_id, timeout);
    state.add_step("room_transfer_owner",
                   transfer_owner.ok &&
                       detail_after_transfer.ok &&
                       detail_after_transfer.response_body.find("\"owner_user_id\":\"" + bob_id + "\"") !=
                           std::string::npos,
                   detail_after_transfer.response_body.empty()
                       ? transfer_owner.error_message
                       : detail_after_transfer.response_body);

    const auto room_list = alice.room_list(1, 20, "", timeout);
    state.add_step("room_list", room_list.ok && room_list.response_body.find(room_id) != std::string::npos,
                   room_list.response_body.empty() ? room_list.error_message : room_list.response_body);

    const auto room_detail = alice.room_detail(room_id, timeout);
    state.add_step("room_detail", room_detail.ok && room_detail.response_body.find(room_id) != std::string::npos,
                   room_detail.response_body.empty() ? room_detail.error_message : room_detail.response_body);

    const auto ready_a = alice.set_ready(true, timeout);
    const auto ready_b = bob.set_ready(true, timeout);
    state.add_step("ready_two_users", ready_a.ok && ready_b.ok,
                   ready_a.error_message + " | " + ready_b.error_message);

    const auto start = bob.start_battle(room_id, timeout);
    state.add_step("start_battle", start.ok, start.error_message);
    std::this_thread::sleep_for(250ms);

    const auto move_a = alice.send_battle_input(bgtc::encodeLegacyMoveInput(50, 50), timeout);
    const auto move_b = bob.send_battle_input(bgtc::encodeLegacyMoveInput(60, 60), timeout);
    state.add_step("send_battle_inputs", move_a.ok && move_b.ok,
                   move_a.error_message + " | " + move_b.error_message);
    std::this_thread::sleep_for(250ms);

    const auto item_state = alice.battle_state(start.battle_id, timeout);
    const auto item_snapshot = bgtc::decodeTankSnapshot(item_state.response_body);
    const auto item_id = item_snapshot.has_value() && !item_snapshot->items.empty()
                             ? item_snapshot->items.front().id
                             : std::string{};
    state.add_step("item_spawn_visible",
                   item_state.ok && item_snapshot.has_value() && !item_id.empty(),
                   item_state.response_body.empty() ? item_state.error_message : item_state.response_body);

    const auto pickup = item_id.empty()
                            ? sdk::BattleInputResult{false, -1, "missing_item", 0}
                            : alice.send_battle_input(bgtc::encodeLegacyPickupInput(item_id), timeout);
    std::this_thread::sleep_for(250ms);
    const auto pickup_state = alice.battle_state(start.battle_id, timeout);
    const auto pickup_snapshot = bgtc::decodeTankSnapshot(pickup_state.response_body);
    state.add_step("item_pickup_buff",
                   pickup.ok &&
                       pickup_state.ok &&
                       pickup_snapshot.has_value() &&
                       !pickup_snapshot->buffs.empty() &&
                       pickup_snapshot->buffs.front().type == "speed",
                   pickup_state.response_body.empty()
                       ? pickup.error_message
                       : pickup_state.response_body);

    const auto fire = alice.send_battle_input(bgtc::encodeLegacyFireInput(1, 0), timeout);
    std::this_thread::sleep_for(150ms);
    const auto bullet_state = alice.battle_state(start.battle_id, timeout);
    const auto bullet_snapshot = bgtc::decodeTankSnapshot(bullet_state.response_body);
    state.add_step("fire_spawns_bullet",
                   fire.ok &&
                       bullet_state.ok &&
                       bullet_snapshot.has_value() &&
                       !bullet_snapshot->bullets.empty(),
                   bullet_state.response_body.empty()
                       ? fire.error_message
                       : bullet_state.response_body);

    const auto battle_state = alice.battle_state(start.battle_id, timeout);
    const auto restored_snapshot = bgtc::decodeTankSnapshot(battle_state.response_body);
    state.add_step("battle_state_query",
                   battle_state.ok && restored_snapshot.has_value() &&
                       restored_snapshot->frame >= 1 &&
                       restored_snapshot->tanks.size() >= 2,
                   battle_state.response_body.empty()
                       ? battle_state.error_message
                       : battle_state.response_body);

    bob.disconnect();
    sdk::SdkClient bob_reconnected;
    bob_reconnected.on_push(on_push);
    const auto bob_reconnect_ok = bob_reconnected.connect(host, port, timeout);
    const auto bob_relogin = bob_reconnected.login(bob_id, "token:" + bob_id, timeout);
    const auto bob_resume = bob_reconnected.battle_state(start.battle_id, timeout);
    const auto bob_resume_snapshot = bgtc::decodeTankSnapshot(bob_resume.response_body);
    state.add_step("reconnect_battle_state_restore",
                   bob_reconnect_ok && bob_relogin.ok && bob_resume.ok &&
                       bob_resume_snapshot.has_value() &&
                       bob_resume_snapshot->frame >= 1 &&
                       bob_resume_snapshot->tanks.size() >= 2,
                   bob_resume.response_body.empty()
                       ? bob_resume.error_message
                       : bob_resume.response_body);

    const auto finish = alice.send_battle_input(bgtc::encodeLegacyFinishInput("surrender"), timeout);
    state.add_step("finish_battle", finish.ok, finish.error_message);
    std::this_thread::sleep_for(250ms);

    const auto replay = alice.replay_load(start.battle_id, timeout);
    state.add_step("replay_load",
                   replay.ok &&
                       replay.response_body.find(start.battle_id) != std::string::npos &&
                       replay.response_body.find("\"frames\"") != std::string::npos,
                   replay.response_body.empty() ? replay.error_message : replay.response_body);

    const bool saw_started =
        state.saw_push_fragment("battle_state:kind=started") ||
        start.error_message.find("battle_started") != std::string::npos;
    const bool saw_finished =
        state.saw_push_fragment("battle_state:kind=finished") ||
        state.saw_push_fragment("battle_state:kind=settlement") ||
        finish.error_message.find("battle_end_accepted") != std::string::npos;
    state.add_step("observe_battle_push_or_response", saw_started && saw_finished);

    const auto leaderboard = alice.leaderboard_top(10, timeout);
    state.add_step("leaderboard_top", leaderboard.ok, leaderboard.response_body);

    // ── Events observation ──────────────────────────────────────────
    const auto events_state = alice.battle_state(start.battle_id, timeout);
    const auto events_snapshot = bgtc::decodeTankSnapshot(events_state.response_body);
    state.add_step("observe_battle_events",
                   events_state.ok &&
                       events_snapshot.has_value() &&
                       !events_snapshot->events.empty(),
                   events_state.response_body.empty()
                       ? events_state.error_message
                       : events_state.response_body);

    // ── Multi-battle continuous play ────────────────────────────────
    const auto ready_a2 = alice.set_ready(true, timeout);
    const auto ready_b2 = bob_reconnected.set_ready(true, timeout);
    state.add_step("multi_battle_ready", ready_a2.ok && ready_b2.ok,
                   ready_a2.error_message + " | " + ready_b2.error_message);

    const auto start2 = bob_reconnected.start_battle(room_id, timeout);
    state.add_step("multi_battle_start", start2.ok, start2.error_message);
    std::this_thread::sleep_for(250ms);

    const auto move_a2 = alice.send_battle_input(bgtc::encodeLegacyMoveInput(70, 70), timeout);
    const auto move_b2 = bob_reconnected.send_battle_input(bgtc::encodeLegacyMoveInput(30, 30), timeout);
    state.add_step("multi_battle_inputs", move_a2.ok && move_b2.ok,
                   move_a2.error_message + " | " + move_b2.error_message);
    std::this_thread::sleep_for(250ms);

    const auto battle2_state = alice.battle_state(start2.battle_id, timeout);
    const auto battle2_snapshot = bgtc::decodeTankSnapshot(battle2_state.response_body);
    state.add_step("multi_battle_state_query",
                   battle2_state.ok && battle2_snapshot.has_value() &&
                       battle2_snapshot->frame >= 1 &&
                       battle2_snapshot->tanks.size() >= 2,
                   battle2_state.response_body.empty()
                       ? battle2_state.error_message
                       : battle2_state.response_body);

    const auto finish2 = alice.send_battle_input(bgtc::encodeLegacyFinishInput("surrender"), timeout);
    state.add_step("multi_battle_finish", finish2.ok, finish2.error_message);
    std::this_thread::sleep_for(250ms);

    // ── Matchmaking end-to-end ──────────────────────────────────────
    const auto match_join_a = alice.match_join(alice_id, 1200, "1v1", timeout);
    state.add_step("matchmaking_join_alice", match_join_a.ok, match_join_a.error_message);

    const auto match_join_b = bob_reconnected.match_join(bob_id, 1150, "1v1", timeout);
    state.add_step("matchmaking_join_bob", match_join_b.ok, match_join_b.error_message);
    std::this_thread::sleep_for(500ms);

    const auto match_status_a = alice.match_status(alice_id, "1v1", timeout);
    state.add_step("matchmaking_status_query",
                   match_status_a.ok,
                   match_status_a.response_body.empty()
                       ? match_status_a.error_message
                       : match_status_a.response_body);

    const auto match_leave_a = alice.match_leave(alice_id, "1v1", timeout);
    const auto match_leave_b = bob_reconnected.match_leave(bob_id, "1v1", timeout);
    state.add_step("matchmaking_leave", match_leave_a.ok && match_leave_b.ok,
                   match_leave_a.error_message + " | " + match_leave_b.error_message);

    alice.leave_room(room_id, timeout);
    bob_reconnected.leave_room(room_id, timeout);
    alice.disconnect();
    bob.disconnect();
    charlie.disconnect();

    write_summary(state, host, port, room_id);
    return state.overall_pass() ? 0 : 1;
}

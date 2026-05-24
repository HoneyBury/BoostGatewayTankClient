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
    bool passed = false;
    std::string detail;
};

struct GateState {
    std::vector<Step> steps;
    std::vector<sdk::PushMessage> pushes;
    std::mutex mutex;

    void add_step(std::string name, bool passed, std::string detail = {}) {
        steps.push_back({std::move(name), passed, std::move(detail)});
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
    const auto room_id = "qt_gate_room_" + run_id;

    GateState state;
    sdk::SdkClient alice;
    sdk::SdkClient bob;
    auto on_push = [&](const sdk::PushMessage& push) {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.pushes.push_back(push);
        std::cout << "[push] id=" << push.message_id << " body=" << push.body << "\n";
    };
    alice.on_push(on_push);
    bob.on_push(on_push);

    constexpr auto timeout = 10s;
    const auto alice_connected = alice.connect(host, port, timeout);
    const auto bob_connected = bob.connect(host, port, timeout);
    state.add_step("connect_two_clients", alice_connected && bob_connected);
    if (!alice_connected || !bob_connected) {
        write_summary(state, host, port, room_id);
        return 2;
    }

    const auto alice_login = alice.login(alice_id, "token:" + alice_id, timeout);
    const auto bob_login = bob.login(bob_id, "token:" + bob_id, timeout);
    state.add_step("login_two_users", alice_login.ok && bob_login.ok,
                   alice_login.error_message + " | " + bob_login.error_message);

    const auto create = alice.create_room(room_id, timeout);
    state.add_step("create_room", create.ok, create.error_message);

    const auto join = bob.join_room(room_id, timeout);
    state.add_step("join_room", join.ok, join.error_message);

    const auto ready_a = alice.set_ready(true, timeout);
    const auto ready_b = bob.set_ready(true, timeout);
    state.add_step("ready_two_users", ready_a.ok && ready_b.ok,
                   ready_a.error_message + " | " + ready_b.error_message);

    const auto start = alice.start_battle(room_id, timeout);
    state.add_step("start_battle", start.ok, start.error_message);
    std::this_thread::sleep_for(250ms);

    const auto move_a = alice.send_battle_input(bgtc::encodeLegacyMoveInput(50, 50), timeout);
    const auto move_b = bob.send_battle_input(bgtc::encodeLegacyMoveInput(60, 60), timeout);
    state.add_step("send_battle_inputs", move_a.ok && move_b.ok,
                   move_a.error_message + " | " + move_b.error_message);
    std::this_thread::sleep_for(250ms);

    const auto finish = alice.send_battle_input(bgtc::encodeLegacyFinishInput("surrender"), timeout);
    state.add_step("finish_battle", finish.ok, finish.error_message);
    std::this_thread::sleep_for(250ms);

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

    alice.leave_room(room_id, timeout);
    bob.leave_room(room_id, timeout);
    alice.disconnect();
    bob.disconnect();

    write_summary(state, host, port, room_id);
    return state.overall_pass() ? 0 : 1;
}

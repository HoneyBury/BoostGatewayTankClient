#include "tank/TankProtocol.h"

#include <cassert>
#include <iostream>

int main() {
    const auto payload = bgtc::encodeTankInput(bgtc::makeMoveInput(7, 1, 0));
    assert(payload.find("\"seq\":7") != std::string::npos);
    assert(payload.find("\"move\"") != std::string::npos);

    const auto snapshot = bgtc::decodeTankSnapshot(
        R"({"frame":3,"finished":false,"tanks":[{"user_id":"alice","x":2,"y":2,"hp":100,"direction":0,"alive":true}],"bullets":[],"events":[]})");
    assert(snapshot.has_value());
    assert(snapshot->frame == 3);
    assert(snapshot->tanks.size() == 1);
    assert(snapshot->tanks[0].userId == "alice");

    const auto battleState = bgtc::decodeBattleStateEvent(
        "battle_state:kind=frame:room_id=r1:battle_id=b1:frame=12:trigger=tick");
    assert(battleState.has_value());
    assert(battleState->kind == "frame");
    assert(battleState->frame == 12);

    const auto liveFrame = bgtc::decodeTankSnapshot(
        R"({"battle_id":"battle_1","frame_number":2,"kind":"frame_advanced","participants":[{"hp":100,"max_hp":100,"online":true,"pos_x":50,"pos_y":50,"score":0,"user_id":"alice"},{"hp":90,"max_hp":100,"online":true,"pos_x":60,"pos_y":60,"score":1,"user_id":"bob"}],"trigger":"input:alice:1"})");
    assert(liveFrame.has_value());
    assert(liveFrame->frame == 2);
    assert(liveFrame->tanks.size() == 2);
    assert(liveFrame->tanks[0].x == 50);
    assert(liveFrame->tanks[1].score == 1);
    assert(liveFrame->battleState.has_value());
    assert(liveFrame->battleState->kind == "frame_advanced");

    const auto liveFinish = bgtc::decodeTankSnapshot(
        R"({"battle_id":"battle_1","kind":"battle_finished","reason":"user_requested","total_frames":2,"winner_user_id":"alice"})");
    assert(liveFinish.has_value());
    assert(liveFinish->finished);
    assert(liveFinish->finishReason == "user_requested");
    assert(liveFinish->winnerUserId == "alice");

    std::cout << "tank protocol smoke test passed\n";
    return 0;
}

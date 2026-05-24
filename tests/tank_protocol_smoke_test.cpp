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

    std::cout << "tank protocol smoke test passed\n";
    return 0;
}

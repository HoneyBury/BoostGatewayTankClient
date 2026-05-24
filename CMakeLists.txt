cmake_minimum_required(VERSION 3.24)

project(BoostGatewayTankClient
    VERSION 0.1.0
    DESCRIPTION "Qt/C++ multiplayer tank battle validation client for BoostGateway"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BGTC_BUILD_TESTS "Build BoostGatewayTankClient tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(BoostGatewaySdk)

find_package(Qt6 QUIET COMPONENTS Widgets)
if(NOT Qt6_FOUND)
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    set(BGTC_QT_MAJOR 5)
else()
    set(BGTC_QT_MAJOR 6)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_executable(boost_gateway_tank_client
    src/app/main.cpp
    src/core/AppConfig.cpp
    src/core/AppConfig.h
    src/core/ClientState.h
    src/sdk/GatewayClient.cpp
    src/sdk/GatewayClient.h
    src/tank/TankProtocol.cpp
    src/tank/TankProtocol.h
    src/tank/TankWorldModel.h
    src/ui/LoginWindow.cpp
    src/ui/LoginWindow.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/ui/LobbyWidget.cpp
    src/ui/LobbyWidget.h
    src/ui/BattleWidget.cpp
    src/ui/BattleWidget.h
)

target_include_directories(boost_gateway_tank_client
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(boost_gateway_tank_client
    PRIVATE
        Qt${BGTC_QT_MAJOR}::Widgets
        boost_gateway::sdk
)

target_compile_definitions(boost_gateway_tank_client
    PRIVATE
        BGTC_APP_VERSION="${PROJECT_VERSION}"
)

if(BGTC_BUILD_TESTS)
    enable_testing()
    add_executable(tank_protocol_smoke_test
        tests/tank_protocol_smoke_test.cpp
        src/tank/TankProtocol.cpp
        src/tank/TankProtocol.h
        src/tank/TankWorldModel.h
    )
    target_include_directories(tank_protocol_smoke_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(tank_protocol_smoke_test PRIVATE boost_gateway::sdk)
    add_test(NAME tank_protocol_smoke_test COMMAND tank_protocol_smoke_test)
endif()

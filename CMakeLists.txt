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
option(BGTC_BUILD_HEADLESS "Build headless integration client" ON)

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
    src/core/ClientProfile.cpp
    src/core/ClientProfile.h
    src/core/Diagnostics.h
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
    src/ui/DiagnosticsWidget.cpp
    src/ui/DiagnosticsWidget.h
    src/ui/LeaderboardWidget.cpp
    src/ui/LeaderboardWidget.h
    src/ui/ReplayWidget.cpp
    src/ui/ReplayWidget.h
    src/ui/SettingsWidget.cpp
    src/ui/SettingsWidget.h
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

install(TARGETS boost_gateway_tank_client
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
install(FILES
    README.md
    docs/server-integration.md
    docs/production-roadmap.md
    DESTINATION docs
)

set(CPACK_PACKAGE_NAME "BoostGatewayTankClient")
set(CPACK_PACKAGE_VENDOR "HoneyBury")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Qt/C++ multiplayer tank validation client for BoostGateway")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
if(APPLE)
    set(CPACK_GENERATOR "TGZ")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)

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

    add_executable(tank_ui_smoke_test
        tests/qt_ui_smoke_test.cpp
        src/core/AppConfig.cpp
        src/core/AppConfig.h
        src/core/ClientState.h
        src/core/ClientProfile.cpp
        src/core/ClientProfile.h
        src/core/Diagnostics.h
        src/sdk/GatewayClient.cpp
        src/sdk/GatewayClient.h
        src/tank/TankProtocol.cpp
        src/tank/TankProtocol.h
        src/tank/TankWorldModel.h
        src/ui/BattleWidget.cpp
        src/ui/BattleWidget.h
        src/ui/LeaderboardWidget.cpp
        src/ui/LeaderboardWidget.h
        src/ui/LobbyWidget.cpp
        src/ui/LobbyWidget.h
        src/ui/ReplayWidget.cpp
        src/ui/ReplayWidget.h
    )
    target_include_directories(tank_ui_smoke_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(tank_ui_smoke_test
        PRIVATE
            Qt${BGTC_QT_MAJOR}::Widgets
            boost_gateway::sdk
    )
    add_test(NAME tank_ui_smoke_test COMMAND tank_ui_smoke_test)
    set_tests_properties(tank_ui_smoke_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(tank_ui_flow_test
        tests/tank_ui_flow_test.cpp
        src/core/AppConfig.cpp
        src/core/AppConfig.h
        src/core/ClientProfile.cpp
        src/core/ClientProfile.h
        src/core/ClientState.h
        src/core/Diagnostics.h
        src/sdk/GatewayClient.cpp
        src/sdk/GatewayClient.h
        src/tank/TankProtocol.cpp
        src/tank/TankProtocol.h
        src/tank/TankWorldModel.h
        src/ui/BattleWidget.cpp
        src/ui/BattleWidget.h
        src/ui/ReplayWidget.cpp
        src/ui/ReplayWidget.h
        src/ui/SettingsWidget.cpp
        src/ui/SettingsWidget.h
    )
    target_include_directories(tank_ui_flow_test
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(tank_ui_flow_test
        PRIVATE
            Qt${BGTC_QT_MAJOR}::Widgets
            boost_gateway::sdk
    )
    add_test(NAME tank_ui_flow_test COMMAND tank_ui_flow_test)
    set_tests_properties(tank_ui_flow_test PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(BGTC_BUILD_HEADLESS)
    add_executable(tank_headless_gate
        tests/tank_headless_gate.cpp
        src/tank/TankProtocol.cpp
        src/tank/TankProtocol.h
        src/tank/TankWorldModel.h
    )
    target_include_directories(tank_headless_gate
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(tank_headless_gate PRIVATE boost_gateway::sdk)
endif()

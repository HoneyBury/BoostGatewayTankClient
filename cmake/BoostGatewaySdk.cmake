set(BOOST_GATEWAY_SERVER_ROOT "" CACHE PATH "Path to the BoostGateway server repository")
set(BOOST_GATEWAY_SERVER_BUILD_DIR "" CACHE PATH "Path to the BoostGateway server build directory")
set(BOOST_GATEWAY_SDK_DIR "" CACHE PATH "Path to a directory containing boost_gateway_sdk-config.cmake")

set(_bgtc_sdk_found OFF)

if(BOOST_GATEWAY_SDK_DIR
   AND EXISTS "${BOOST_GATEWAY_SDK_DIR}/boost_gateway_sdk-config.cmake"
   AND EXISTS "${BOOST_GATEWAY_SDK_DIR}/boost_gateway_sdk-targets.cmake")
    list(PREPEND CMAKE_PREFIX_PATH "${BOOST_GATEWAY_SDK_DIR}")
    find_package(boost_gateway_sdk CONFIG REQUIRED)
    set(_bgtc_sdk_found ON)
endif()

if(NOT _bgtc_sdk_found AND BOOST_GATEWAY_SERVER_ROOT)
    set(_bgtc_sdk_package_candidates)
    if(BOOST_GATEWAY_SERVER_BUILD_DIR)
        list(APPEND _bgtc_sdk_package_candidates "${BOOST_GATEWAY_SERVER_BUILD_DIR}/sdk")
    endif()
    list(APPEND _bgtc_sdk_package_candidates
        "${BOOST_GATEWAY_SERVER_ROOT}/build/sdk"
        "${BOOST_GATEWAY_SERVER_ROOT}/build/release/sdk"
        "${BOOST_GATEWAY_SERVER_ROOT}/build/default/sdk"
        "${BOOST_GATEWAY_SERVER_ROOT}/runtime/sdk-package-prefix"
    )

    foreach(_candidate
        IN LISTS _bgtc_sdk_package_candidates
    )
        if(EXISTS "${_candidate}/boost_gateway_sdk-config.cmake"
           AND EXISTS "${_candidate}/boost_gateway_sdk-targets.cmake")
            list(PREPEND CMAKE_PREFIX_PATH "${_candidate}")
        endif()
    endforeach()
    find_package(boost_gateway_sdk CONFIG QUIET)
    if(boost_gateway_sdk_FOUND)
        set(_bgtc_sdk_found ON)
    endif()
endif()

if(NOT _bgtc_sdk_found AND BOOST_GATEWAY_SERVER_ROOT)
    set(_bgtc_sdk_build_candidates)
    if(BOOST_GATEWAY_SERVER_BUILD_DIR)
        list(APPEND _bgtc_sdk_build_candidates "${BOOST_GATEWAY_SERVER_BUILD_DIR}/sdk")
    endif()
    list(APPEND _bgtc_sdk_build_candidates
        "${BOOST_GATEWAY_SERVER_ROOT}/build/sdk"
        "${BOOST_GATEWAY_SERVER_ROOT}/build/release/sdk"
        "${BOOST_GATEWAY_SERVER_ROOT}/build/default/sdk"
    )

    foreach(_build_dir
        IN LISTS _bgtc_sdk_build_candidates
    )
        if(EXISTS "${_build_dir}/libboost_gateway_sdk.a")
            set(_bgtc_sdk_lib "${_build_dir}/libboost_gateway_sdk.a")
            set(_bgtc_sdk_build_include "${_build_dir}/include")
            set(_bgtc_sdk_found ON)
            break()
        endif()
    endforeach()

    if(_bgtc_sdk_found)
        find_package(nlohmann_json QUIET)
        if(NOT TARGET nlohmann_json::nlohmann_json)
            find_path(_bgtc_json_include nlohmann/json.hpp
                PATHS
                    "${BOOST_GATEWAY_SERVER_BUILD_DIR}/_deps/nlohmann_json-src/include"
                    "${BOOST_GATEWAY_SERVER_ROOT}/build/_deps/nlohmann_json-src/include"
                    "${BOOST_GATEWAY_SERVER_ROOT}/build/build/_deps/nlohmann_json-src/include"
                    "${BOOST_GATEWAY_SERVER_ROOT}/build/default/_deps/nlohmann_json-src/include"
                    "${BOOST_GATEWAY_SERVER_ROOT}/build/release/_deps/nlohmann_json-src/include"
                NO_DEFAULT_PATH)
            if(_bgtc_json_include)
                add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
                set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${_bgtc_json_include}")
            endif()
        endif()

        add_library(boost_gateway_sdk STATIC IMPORTED GLOBAL)
        set_target_properties(boost_gateway_sdk PROPERTIES
            IMPORTED_LOCATION "${_bgtc_sdk_lib}"
            INTERFACE_INCLUDE_DIRECTORIES
                "${BOOST_GATEWAY_SERVER_ROOT}/sdk/include;${_bgtc_sdk_build_include}"
            INTERFACE_COMPILE_DEFINITIONS "BOOST_ALL_NO_LIB;BOOST_ERROR_CODE_HEADER_ONLY")

        if(TARGET nlohmann_json::nlohmann_json)
            target_link_libraries(boost_gateway_sdk INTERFACE nlohmann_json::nlohmann_json)
        endif()

        add_library(boost_gateway::sdk ALIAS boost_gateway_sdk)
    endif()
endif()

if(NOT _bgtc_sdk_found)
    message(FATAL_ERROR
        "Could not import BoostGateway SDK. Set BOOST_GATEWAY_SDK_DIR to an installed SDK "
        "package config directory, or BOOST_GATEWAY_SERVER_ROOT to the BoostAsioDemo checkout.")
endif()

if(NOT TARGET boost_gateway::sdk)
    message(FATAL_ERROR "boost_gateway_sdk was found, but target boost_gateway::sdk is missing")
endif()

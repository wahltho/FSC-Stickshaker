cmake_minimum_required(VERSION 3.15)
project(fsc_stickshaker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(XPLANE_SDK_ROOT "${CMAKE_SOURCE_DIR}/../SDKs/XPlane_SDK" CACHE PATH "Path to X-Plane SDK root")
option(FSC_STICKSHAKER_BUILD_TESTS "Build host-side smoke tests" ON)

if (NOT EXISTS "${XPLANE_SDK_ROOT}/CHeaders/XPLM/XPLMPlugin.h")
    message(FATAL_ERROR "XPLM headers not found under ${XPLANE_SDK_ROOT}/CHeaders. Set XPLANE_SDK_ROOT accordingly.")
endif()

function(configure_xplm_target target)
    target_include_directories(${target} PRIVATE
        "${XPLANE_SDK_ROOT}/CHeaders"
        "${XPLANE_SDK_ROOT}/CHeaders/XPLM"
        "${XPLANE_SDK_ROOT}/CHeaders/Widgets"
    )

    if(APPLE)
        target_compile_definitions(${target} PRIVATE APL=1 IBM=0 LIN=0)
        target_link_options(${target} PRIVATE "-F${XPLANE_SDK_ROOT}/Libraries/Mac")
        target_link_libraries(${target} PRIVATE "-framework XPLM" "-framework XPWidgets")
    elseif(WIN32)
        target_compile_definitions(${target} PRIVATE APL=0 IBM=1 LIN=0)
        target_link_libraries(${target} PRIVATE
            "${XPLANE_SDK_ROOT}/Libraries/Win/XPLM_64.lib"
            "${XPLANE_SDK_ROOT}/Libraries/Win/XPWidgets_64.lib"
            ws2_32
        )
        if(MINGW)
            target_link_options(${target} PRIVATE "-static" "-static-libgcc" "-static-libstdc++")
        endif()
    else()
        target_compile_definitions(${target} PRIVATE APL=0 IBM=0 LIN=1)
        target_link_libraries(${target} PRIVATE
            "${XPLANE_SDK_ROOT}/Libraries/Lin/XPLM_64.so"
            "${XPLANE_SDK_ROOT}/Libraries/Lin/XPWidgets_64.so"
        )
    endif()

    target_compile_definitions(${target} PRIVATE XPLM200=1 XPLM210=1 XPLM300=1 XPLM301=1 XPLM303=1 XPLM400=1)

    set_target_properties(${target} PROPERTIES
        PREFIX ""
        SUFFIX ".xpl"
    )
endfunction()

add_library(fsc_stickshaker_core STATIC
    src/core/Config.cpp
    src/core/Log.cpp
    src/core/Protocol.cpp
    src/core/ShakerController.cpp
    src/core/Transport.cpp
)

target_include_directories(fsc_stickshaker_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(WIN32)
    target_link_libraries(fsc_stickshaker_core PUBLIC ws2_32)
endif()

add_library(fsc_stickshaker SHARED src/plugin/XPlanePlugin.cpp)
target_link_libraries(fsc_stickshaker PRIVATE fsc_stickshaker_core)
target_include_directories(fsc_stickshaker PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
configure_xplm_target(fsc_stickshaker)

if(APPLE)
    set_target_properties(fsc_stickshaker PROPERTIES OUTPUT_NAME "mac")
elseif(WIN32)
    set_target_properties(fsc_stickshaker PROPERTIES OUTPUT_NAME "win")
else()
    set_target_properties(fsc_stickshaker PROPERTIES OUTPUT_NAME "lin")
endif()

if(FSC_STICKSHAKER_BUILD_TESTS)
    enable_testing()
    add_executable(fsc_stickshaker_smoke tests/smoke.cpp)
    target_link_libraries(fsc_stickshaker_smoke PRIVATE fsc_stickshaker_core)
    add_test(NAME fsc_stickshaker_smoke COMMAND fsc_stickshaker_smoke)
endif()

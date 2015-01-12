project(lualwm2m C)
cmake_minimum_required (VERSION 2.8.3)
find_package(Lua51 REQUIRED)

SET(LIBLWM2M_DIR ${CMAKE_CURRENT_LIST_DIR}/liblwm2m/core)

set(CMAKE_BUILD_TYPE Debug)
add_definitions(-DLWM2M_CLIENT_MODE)

include_directories (${LIBLWM2M_DIR} ${CMAKE_CURRENT_LIST_DIR}/utils)
add_subdirectory(${LIBLWM2M_DIR} ${CMAKE_CURRENT_BINARY_DIR}/core)

SET(SOURCES src/lua_liblwm2m.c src/lua_object.c)

add_library(lwm2m MODULE ${SOURCES} ${CORE_SOURCES})
SET_TARGET_PROPERTIES(lwm2m PROPERTIES PREFIX "")


file(COPY src/lwm2mobject.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/simplesample.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/observesample.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/complexsample.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/dtlssample_ecc.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/dtlssample_ecc_disconnect.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/dtlssample_psk.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/dtlssample_psk_disconnect.lua DESTINATION "${CMAKE_BINARY_DIR}")
file(COPY sample/multiinstancesample.lua DESTINATION "${CMAKE_BINARY_DIR}")

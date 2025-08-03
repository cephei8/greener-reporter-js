cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project(greener-servermock-js)

set(CMAKE_CXX_STANDARD 20)
add_definitions(-DNAPI_VERSION=9)


set(GREENER_SERVERMOCK_SOURCE_FILES
        addon/addon.cpp
        addon/servermock.cpp
)
add_library(greener_servermock_addon SHARED ${GREENER_SERVERMOCK_SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(greener_servermock_addon PROPERTIES PREFIX "" SUFFIX ".node")
target_include_directories(greener_servermock_addon PUBLIC ${CMAKE_JS_INC} dist/include)
target_link_directories(greener_servermock_addon PUBLIC dist/lib/mac-x64)
target_link_libraries(greener_servermock_addon ${CMAKE_JS_LIB} greener_servermock)

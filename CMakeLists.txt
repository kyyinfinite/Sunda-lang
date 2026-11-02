cmake_minimum_required(VERSION 3.16)
project(sunda LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(SUNDA_SANITIZE "Aktifkeun AddressSanitizer jeung UBSan" OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -fno-rtti")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

if(SUNDA_SANITIZE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
endif()

add_library(sunda STATIC
    src/lexer.cpp
    src/parser.cpp
    src/resolver.cpp
    src/compiler.cpp
    src/chunk.cpp
    src/object.cpp
    src/table.cpp
    src/memory.cpp
    src/vm.cpp
    src/native.cpp
    src/modules.cpp
    src/formatter.cpp
    src/cli.cpp
)
target_include_directories(sunda PUBLIC src)

add_executable(sunda_cli src/main.cpp)
set_target_properties(sunda_cli PROPERTIES OUTPUT_NAME sunda)
target_link_libraries(sunda_cli PRIVATE sunda)

enable_testing()
add_test(NAME sunda_tests COMMAND ${CMAKE_SOURCE_DIR}/tests/run_tests.sh $<TARGET_FILE:sunda_cli>)
add_test(NAME sunda_cli_tests COMMAND $<TARGET_FILE:sunda_cli> test ${CMAKE_SOURCE_DIR}/tests)

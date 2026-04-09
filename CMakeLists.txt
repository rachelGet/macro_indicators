cmake_minimum_required(VERSION 3.16)
project(MacroIndicators LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")
    set(CMAKE_DEBUG_POSTFIX d)
    message(STATUS "Setting libCXX debug flag")
endif()

find_package(Threads REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

file(GLOB SOURCES "*.cpp")
add_executable(MacroIndicators ${SOURCES})

set_target_properties(MacroIndicators
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(MacroIndicators PRIVATE
    Threads::Threads
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(MSVC)
    target_compile_options(MacroIndicators PRIVATE /W3)
else()
    target_compile_options(MacroIndicators PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

# Copy style.qss next to the executable so it's found at runtime
add_custom_command(TARGET MacroIndicators POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/style.qss"
        "${CMAKE_BINARY_DIR}/bin/style.qss"
    COMMENT "Copying style.qss to build output"
)

install(TARGETS MacroIndicators DESTINATION bin)
install(FILES style.qss DESTINATION bin)

cmake_minimum_required(VERSION 3.15)
project(GameLang VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set compiler flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Collect source files
file(GLOB_RECURSE GAMELANG_SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
)
list(REMOVE_ITEM GAMELANG_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")

# Shared implementation used by the CLI and tests.
add_library(gamelang_lib ${GAMELANG_SOURCES})
target_include_directories(gamelang_lib PUBLIC src)

# Create the executable
add_executable(gamelang src/main.cpp)
target_link_libraries(gamelang PRIVATE gamelang_lib)

# Set output directory
set_target_properties(gamelang PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Enable testing
enable_testing()

# Add C++ unit tests (if any)
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "tests/*.cpp")
if(TEST_SOURCES)
    add_executable(gamelang_tests ${TEST_SOURCES})
    target_link_libraries(gamelang_tests PRIVATE gamelang_lib)
    target_compile_definitions(gamelang_tests PRIVATE TESTING)
    add_test(NAME GameLangTests COMMAND gamelang_tests)
endif()

# Integration tests that execute the bundled examples.
set(GAMELANG_EXAMPLES
    prisoners_dilemma
    strategy_tournament
    evolutionary_dynamics
    auction_theory
    market_entry_analysis
    feature_showcase
)

foreach(example_name IN LISTS GAMELANG_EXAMPLES)
    add_test(
        NAME "example_${example_name}"
        COMMAND gamelang "${CMAKE_CURRENT_SOURCE_DIR}/examples/${example_name}.gl"
    )
    set_tests_properties("example_${example_name}" PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    )
endforeach()

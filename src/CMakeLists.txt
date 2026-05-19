cmake_minimum_required(VERSION 3.10)
project(tmp)

message("Compiler Version: ${CMAKE_CXX_COMPILER} ${CMAKE_CXX_COMPILER_VERSION}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-std=c++23 -O3 -Wall -Wextra -Wpedantic -DNDEBUG)

include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Z3
find_package(Z3
  REQUIRED
  CONFIG
)
add_compile_options(${Z3_COMPONENT_CXX_FLAGS})

add_library(strips_core
    strips.cpp
)

target_include_directories(strips_core
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(strips_core
    PUBLIC
      nlohmann_json::nlohmann_json
      ${Z3_LIBRARIES}
)

add_executable(planner
    main.cpp
)

target_link_libraries(planner
    PRIVATE
      strips_core
)

install(TARGETS planner)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

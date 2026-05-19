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

file(GLOB SOURCES
    *.cpp
    *.h
)

add_executable(main ${SOURCES})

# Z3
find_package(Z3
  REQUIRED
  CONFIG
  # `NO_DEFAULT_PATH` is set so that -DZ3_DIR has to be passed to find Z3.
  # This should prevent us from accidentally picking up an installed
  # copy of Z3. This is here to benefit Z3's build system when building
  # this project. When making your own project you probably shouldn't
  # use this option.
  # NO_DEFAULT_PATH
)
# add_executable(cpp_example example.cpp)

# target_include_directories(cpp_example PRIVATE ${Z3_CXX_INCLUDE_DIRS})
# target_link_libraries(cpp_example PRIVATE ${Z3_LIBRARIES})

add_compile_options(${Z3_COMPONENT_CXX_FLAGS})

target_link_libraries(main PRIVATE nlohmann_json::nlohmann_json ${Z3_LIBRARIES})

install(TARGETS main)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES 
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()


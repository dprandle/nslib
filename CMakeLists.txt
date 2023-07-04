cmake_minimum_required(VERSION 3.11.0)

project(noble-steed)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)


set(NOBLE_STEED_SRC_DIR ${CMAKE_SOURCE_DIR}/src/noble-steed)
set(NOBLE_STEED_INCLUDE ${NOBLE_STEED_SRC_DIR} CACHE INTERNAL "")
set(SAMPLES_DIR ${CMAKE_SOURCE_DIR}/samples)
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

add_subdirectory(${NOBLE_STEED_SRC_DIR})
add_subdirectory(${SAMPLES_DIR})
add_subdirectory(${DEPS_DIR})

# if(WIN32)
# elseif(APPLE)
# else()
# endif()

# target_link_libraries(${TARGET_NAME} lib1 lib2 ... libn)

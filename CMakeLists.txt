cmake_minimum_required(VERSION 3.11.0)

project(nslib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(NSLIB_SRC_DIR ${CMAKE_SOURCE_DIR}/src/nslib)
set(NSLIB_INCLUDE ${NSLIB_SRC_DIR} CACHE INTERNAL "")
set(SAMPLES_DIR ${CMAKE_SOURCE_DIR}/samples)
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

add_subdirectory(${DEPS_DIR})
add_subdirectory(${NSLIB_SRC_DIR})
add_subdirectory(${SAMPLES_DIR})

# if(WIN32)
# elseif(APPLE)
# else()
# endif()

# target_link_libraries(${TARGET_NAME} lib1 lib2 ... libn)

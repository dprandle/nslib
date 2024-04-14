cmake_minimum_required(VERSION 3.11.0)

project(nslib)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(nslib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(NSLIB_VERSION_MAJOR 1)
set(NSLIB_VERSION_MINOR 0)
set(NSLIB_VERSION_PATCH 0)

set(NSLIB_TARGET_NAME noblesteed-${NSLIB_VERSION_MAJOR}.${NSLIB_VERSION_MINOR}.${NSLIB_VERSION_PATCH})
set(NSLIB_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SAMPLES_DIR ${CMAKE_SOURCE_DIR}/samples)
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)
set(BUILD_DIR ${CMAKE_SOURCE_DIR}/build)

find_package(Vulkan REQUIRED)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

# Add dependencies
add_subdirectory(${DEPS_DIR})

# Set the src files for the project
file(GLOB_RECURSE src_files
  ${NSLIB_SRC_DIR}/*.c*
)

# Create lib
add_library(${NSLIB_TARGET_NAME} SHARED ${src_files})

set_target_properties(${NSLIB_TARGET_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)
set_target_properties(${NSLIB_TARGET_NAME} PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)
set_target_properties(${NSLIB_TARGET_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)
target_compile_definitions(${NSLIB_TARGET_NAME} PUBLIC
  NSLIB_VERSION_MAJOR=${NSLIB_VERSION_MAJOR}
  NSLIB_VERSION_MINOR=${NSLIB_VERSION_MINOR}
  NSLIB_VERSION_PATCH=${NSLIB_VERSION_PATCH}
  DLL_EXPORTS
  _CRT_SECURE_NO_WARNINGS 
)
target_include_directories(${NSLIB_TARGET_NAME} PRIVATE ${GLFW_INCLUDE} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${NSLIB_TARGET_NAME} PRIVATE glfw PUBLIC ${Vulkan_LIBRARIES})

if(WIN32)
  add_custom_target(link_target ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${BUILD_DIR}/debug/compile_commands.json ${CMAKE_SOURCE_DIR}/compile_commands.json)
else() # Unix
  target_compile_options(${NSLIB_TARGET_NAME} PUBLIC -msse4.1)
  if(APPLE)
  else()
  endif()
endif()

# Add samples
add_subdirectory(${SAMPLES_DIR})


# target_link_libraries(${TARGET_NAME} lib1 lib2 ... libn)

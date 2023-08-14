cmake_minimum_required(VERSION 3.11.0)

project(nslib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

set(NSLIB_VERSION_MAJOR 1)
set(NSLIB_VERSION_MINOR 0)
set(NSLIB_VERSION_PATCH 0)

set(NSLIB_TARGET_NAME noblesteed-${NSLIB_VERSION_MAJOR}.${NSLIB_VERSION_MINOR}.${NSLIB_VERSION_PATCH})
set(NSLIB_SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(NSLIB_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)
set(NSLIB_INCLUDE_DIR_DEST ${NSLIB_INCLUDE_DIR}/${CMAKE_PROJECT_NAME})
set(SAMPLES_DIR ${CMAKE_SOURCE_DIR}/samples)
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

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
  NSLIB_VERSION_PATCH=${NSLIB_VERSION_PATCH})
target_include_directories(${NSLIB_TARGET_NAME} PRIVATE ${GLFW_INCLUDE} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${NSLIB_TARGET_NAME} PRIVATE glfw PUBLIC ${Vulkan_LIBRARIES})

# Add all header files to an include folder in the build dir so samples can include the same way as if the lib is installed
file(GLOB_RECURSE header_files LIST_DIRECTORIES false RELATIVE ${NSLIB_SRC_DIR} ${NSLIB_SRC_DIR}/*.h)

# Copy all of the header files in the same original structure
foreach(header_file ${header_files})
  get_filename_component(pdir ${header_file} DIRECTORY)
  file(COPY ${NSLIB_SRC_DIR}/${header_file} DESTINATION ${NSLIB_INCLUDE_DIR_DEST}/${pdir})
endforeach()

# Add samples
add_subdirectory(${SAMPLES_DIR})

# if(WIN32)
# elseif(APPLE)
# else()
# endif()

# target_link_libraries(${TARGET_NAME} lib1 lib2 ... libn)

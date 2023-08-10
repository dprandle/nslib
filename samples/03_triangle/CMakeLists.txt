set(TARGET_NAME 03_triangle)

# Set the src files for the project
file(GLOB src_files
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(${TARGET_NAME} ${src_files})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)

set_target_properties(${TARGET_NAME} ${TARGET_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>
  INCLUDE_DIRECTORIES ${NSLIB_INCLUDE})

target_link_libraries(${TARGET_NAME} nslib)

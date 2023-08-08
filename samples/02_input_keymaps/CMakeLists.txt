set(TARGET_NAME 02_input_keymaps)

# Set the src files for the project
file(GLOB src_files
  ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

# Create lib
add_executable(${TARGET_NAME} ${src_files})

set_target_properties(${TARGET_NAME} ${TARGET_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>
  INCLUDE_DIRECTORIES ${NSLIB_INCLUDE})

target_link_libraries(${TARGET_NAME} nslib)

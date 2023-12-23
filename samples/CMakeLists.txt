
function(create_sample TARGET_NAME)
  file(GLOB src_files
    ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}/src/*.cpp)

  add_executable(${TARGET_NAME} ${src_files})

  set_target_properties(${TARGET_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)
  set_target_properties(${TARGET_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)

  target_include_directories(${TARGET_NAME} PUBLIC ${NSLIB_SRC_DIR})
  target_link_libraries(${TARGET_NAME} ${NSLIB_TARGET_NAME})
endfunction()

function(add_sample_compile_shaders_target TARGET_NAME)
  add_compile_shaders_target(${TARGET_NAME}/src/shaders bin/shaders)
endfunction()

create_sample(basic_window)
create_sample(input_keymaps)
create_sample(triangle)
add_sample_compile_shaders_target(triangle)
create_sample(containers)
create_sample(pack_unpack)
create_sample(rdev)


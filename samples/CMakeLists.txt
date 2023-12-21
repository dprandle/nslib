
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

function(add_compile_shaders_command TARGET_NAME)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin/shaders)
file(GLOB shader_files
  ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}/src/shaders/*)

foreach(source ${shader_files})
  get_filename_component(cur_fname ${source} NAME)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/bin/shaders/${cur_fname}.spv
    DEPENDS ${source}
    COMMAND
    ${glslc_executable} ARGS -c ${source} -o ${CMAKE_BINARY_DIR}/bin/shaders/${cur_fname}.spv
  )
  add_custom_target(${cur_fname}-target ALL DEPENDS ${CMAKE_BINARY_DIR}/bin/shaders/${cur_fname}.spv)
endforeach()
endfunction()

create_sample(01_basic_window)
create_sample(02_input_keymaps)
create_sample(03_triangle)
add_compile_shaders_command(03_triangle)
create_sample(04_containers)
create_sample(05_pack_unpack)

function(add_compile_shaders_target_abs src_dir dest_dir)
file(MAKE_DIRECTORY ${dest_dir})
file(GLOB shader_files ${src_dir}/*)

foreach(source ${shader_files})
  get_filename_component(cur_fname ${source} NAME)
  add_custom_command(
    OUTPUT ${dest_dir}/${cur_fname}.spv
    DEPENDS ${source}
    COMMAND
    ${glslc_executable} ARGS -c ${source} -o ${dest_dir}/${cur_fname}.spv
  )
  add_custom_target(${cur_fname}-target ALL DEPENDS ${dest_dir}/${cur_fname}.spv)
endforeach()
endfunction()

function(add_compile_shaders_target relative_src_dir build_relative_dest_dir)
  message("Compiling shaders in ${CMAKE_CURRENT_SOURCE_DIR}/${relative_src_dir} to ${CMAKE_BINARY_DIR}/${build_relative_dest_dir}")
  add_compile_shaders_target_abs(${CMAKE_CURRENT_SOURCE_DIR}/${relative_src_dir}  ${CMAKE_BINARY_DIR}/${build_relative_dest_dir})
endfunction()

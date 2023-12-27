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
  # We have to have an actual target depending out output in order for the command to be run on any change to the input file
  add_custom_target(${cur_fname}-target ALL DEPENDS ${dest_dir}/${cur_fname}.spv)
endforeach()
endfunction()

function(add_compile_shaders_target relative_src_dir build_relative_dest_dir)
  message("Compiling shaders in ${CMAKE_CURRENT_SOURCE_DIR}/${relative_src_dir} to ${CMAKE_BINARY_DIR}/${build_relative_dest_dir}")
  add_compile_shaders_target_abs(${CMAKE_CURRENT_SOURCE_DIR}/${relative_src_dir}  ${CMAKE_BINARY_DIR}/${build_relative_dest_dir})
endfunction()

function(add_copy_data_dir_command_abs target_name src_dir dest_dir)
  add_custom_command(
    TARGET ${target_name} POST_BUILD
    COMMAND cmake -E copy_directory ${src_dir} ${dest_dir})
endfunction()

function(add_copy_data_dir_command target_name relative_src_dir build_relative_dest_dir)
  message("Adding command to copy ${CMAKE_CURRENT_SOURCE_DIR}/${relative_src_dir} to ${CMAKE_BINARY_DIR}/${build_relative_dest_dir}")
  add_copy_data_dir_command_abs(${target_name} ${CMAKE_CURRENT_SOURCE_DIR}/${relative_src_dir} ${CMAKE_BINARY_DIR}/${build_relative_dest_dir})
endfunction()

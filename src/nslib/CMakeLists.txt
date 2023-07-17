# Set the src files for the project
file(GLOB src_files
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c*
  ${CMAKE_CURRENT_SOURCE_DIR}/containers/*.c*  
  )

# Create lib
add_library(${CMAKE_PROJECT_NAME} SHARED ${src_files})

message(GLFWinclude: ${GLFW_INCLUDE})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES INCLUDE_DIRECTORIES ${GLFW_INCLUDE})
set_target_properties(${CMAKE_PROJECT_NAME} ${CMAKE_PROJECT_NAME} ${CMAKE_PROJECT_NAME} ${CMAKE_PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

target_link_libraries(${CMAKE_PROJECT_NAME} glfw vulkan)


# Set the src files for the project
file(GLOB src_files
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c*
  ${CMAKE_CURRENT_SOURCE_DIR}/containers/*.c*  
  )

  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
# Create lib
add_library(${CMAKE_PROJECT_NAME} SHARED ${src_files})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/bin>)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES
  LIBRARY_OUTPUT_DIRECTORY $<1:${CMAKE_BINARY_DIR}/lib>)

find_package(Vulkan REQUIRED)
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${GLFW_INCLUDE} PUBLIC ${Vulkan_INCLUDE_DIRS})
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE glfw PUBLIC ${Vulkan_LIBRARIES})

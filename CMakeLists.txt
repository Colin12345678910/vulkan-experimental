# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project("vulkan_experimental" VERSION 0.1.0)


find_package(Vulkan REQUIRED)

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  message(STATUS "BUILDING SHADER")
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS ${GLSL})
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders 
    DEPENDS ${SPIRV_BINARY_FILES}
    )
    
if (MSVC)
  add_compile_options(/MP)
endif()    

# Handle "Install"

string(TIMESTAMP CURRENT_DATE "%Y.%m.%d")
set(CPACK_PACKAGE_VERSION ${CURRENT_DATE})

message(STATUS "BIN DIR: ${CMAKE_SOURCE_DIR}/bin/")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
install(TARGETS engine RUNTIME DESTINATION bin COMPONENT runtime)
install(FILES $<TARGET_RUNTIME_DLLS:engine> DESTINATION bin COMPONENT runtime)
#install(DIRECTORY "${CMAKE_SOURCE_DIR}/bin/" DESTINATION bin COMPONENT runtime FILES_MATCHING PATTERN "*")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/" DESTINATION assets COMPONENT runtime FILES_MATCHING PATTERN "*")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/cache/" DESTINATION cache COMPONENT runtime FILES_MATCHING PATTERN "*")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/shaders/" DESTINATION shaders COMPONENT runtime FILES_MATCHING PATTERN "*.spv")

set(CPACK_SOURCE_IGNORE_FILES "/lib/" "/include/")

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "VulkanExperimental")
set(CPACK_COMPONENTS_ALL runtime)
include(CPack)


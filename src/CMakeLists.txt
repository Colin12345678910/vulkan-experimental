# Add minimal main source to the projects executable.

add_executable (engine 
  main.cpp
)

#The source is contained in a static lib that can be attached to several projects.

add_library(vk_core STATIC
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_images.h
  vk_images.cpp 
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_engine.h
  vk_engine.cpp
  vk_loader.h
  vk_loader.cpp
  camera.cpp
  camera.h
  IRenderable.cpp
  IRenderable.h
  RenderNode.cpp
  RenderNode.h
  GLTFMetalicRoughness.cpp
  GLTFMetalicRoughness.h
  CVars.cpp
  CVars.h
  ShadowMap.cpp
  ShadowMap.h
  IRenderpass.h
  HDRI.cpp
  HDRI.h
  Tbin.h
)

function(setup executable) #Basically just assign the correct CXX standards.
set_property(TARGET ${executable} PROPERTY CXX_STANDARD 20)
target_compile_definitions(${executable} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_include_directories(${executable} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

setup(engine)
setup(vk_core)

#Setup dependences for VK_CORE
target_link_libraries(vk_core PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf ktx)
target_precompile_headers(vk_core PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

#Simple library include for the execution one.
target_link_libraries(engine PUBLIC vk_core)

add_custom_command(TARGET engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:engine> $<TARGET_FILE_DIR:engine>
  COMMAND_EXPAND_LISTS
  )

########## Testing
## Maybe move to diff proj?

# GoogleTest requires at least C++17

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(
  hello_test
  ../test/hello_test.cc

)

set_property(TARGET hello_test PROPERTY CXX_STANDARD 20)
target_link_libraries(
  hello_test
  PUBLIC
  GTest::gtest_main
  vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2 vkbootstrap imgui fastgltf::fastgltf vk_core
)
target_precompile_headers(hello_test PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

target_include_directories(hello_test PUBLIC ${CMAKE_SOURCE_DIR}/src)
include(GoogleTest)
gtest_discover_tests(hello_test)

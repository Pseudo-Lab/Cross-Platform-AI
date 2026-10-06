cmake_minimum_required(VERSION 3.24)
project(EnvSettings LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)

add_executable(env_check main.cpp)
target_link_libraries(env_check PRIVATE Vulkan::Vulkan ${CMAKE_DL_LIBS})

set(SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/check.comp")
set(SHADER_BINARY "${CMAKE_CURRENT_BINARY_DIR}/check.comp.spv")

add_custom_command(
    OUTPUT "${SHADER_BINARY}"
    COMMAND Vulkan::glslc --target-env=vulkan1.3 "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
    DEPENDS "${SHADER_SOURCE}"
    COMMENT "Compile check.comp"
    VERBATIM
)
add_custom_target(env_shader DEPENDS "${SHADER_BINARY}")
add_dependencies(env_check env_shader)

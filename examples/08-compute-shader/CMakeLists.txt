cmake_minimum_required(VERSION 3.24)
project(compute_shaderExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)

set(SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/add.comp")
set(SHADER_BINARY "${CMAKE_CURRENT_BINARY_DIR}/add.comp.spv")
add_custom_command(
    OUTPUT "${SHADER_BINARY}"
    # 고정 워크그룹 크기에서 maintenance4가 필요한 LocalSizeId 생성을 피하도록 SPIR-V 1.3을 지정합니다.
    COMMAND Vulkan::glslc --target-env=vulkan1.3 --target-spv=spv1.3
            "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
    DEPENDS "${SHADER_SOURCE}"
    COMMENT "Compile add.comp"
    VERBATIM
)
add_custom_target(compute_shader_shader DEPENDS "${SHADER_BINARY}")

add_executable(compute_shader_example main.cpp)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../99-completed/08-compute-shader.cpp")
    add_executable(compute_shader_completed EXCLUDE_FROM_ALL ../99-completed/08-compute-shader.cpp)
    set_target_properties(compute_shader_completed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/completed")
endif()

foreach(target compute_shader_example compute_shader_completed)
    if(NOT TARGET ${target})
        continue()
    endif()
    set_target_properties(${target} PROPERTIES OUTPUT_NAME main)
    target_link_libraries(${target} PRIVATE Vulkan::Vulkan ${CMAKE_DL_LIBS})
    target_compile_definitions(${target} PRIVATE ADD_SHADER_PATH="${SHADER_BINARY}")
    add_dependencies(${target} compute_shader_shader)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
endforeach()

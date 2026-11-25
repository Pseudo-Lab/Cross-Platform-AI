cmake_minimum_required(VERSION 3.24)
project(naiveMatmulExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(VAI_SOURCE_DIR "" CACHE PATH "Path to a vAi checkout with submodules initialized")
if(NOT EXISTS "${VAI_SOURCE_DIR}/vai/engine/neural-net.h")
    message(FATAL_ERROR "Set -DVAI_SOURCE_DIR=/absolute/path/to/vAi (see README.md).")
endif()
find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)

set(VAI_BUILD_MODEL_DEMO OFF CACHE BOOL "" FORCE)
set(VAI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(VAI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(VAI_BUILD_BENCH OFF CACHE BOOL "" FORCE)
set(VAI_BUILD_PYTHON OFF CACHE BOOL "" FORCE)
add_subdirectory("${VAI_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/vai")

set(SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/naive-matmul.comp")
set(SHADER_BINARY "${CMAKE_CURRENT_BINARY_DIR}/naive-matmul.comp.spv")
add_custom_command(
    OUTPUT "${SHADER_BINARY}"
    COMMAND Vulkan::glslc --target-env=vulkan1.3 --target-spv=spv1.3
            "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
    DEPENDS "${SHADER_SOURCE}"
    VERBATIM
)
add_custom_target(naive_matmul_shader DEPENDS "${SHADER_BINARY}")
add_executable(naive_matmul_example main.cpp)
target_link_libraries(naive_matmul_example PRIVATE vAi)
target_compile_definitions(naive_matmul_example PRIVATE MATMUL_SHADER_PATH="${SHADER_BINARY}")
add_dependencies(naive_matmul_example naive_matmul_shader)
set_target_properties(naive_matmul_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin")
if(MSVC)
    target_compile_options(naive_matmul_example PRIVATE /utf-8)
endif()

set(COMPLETED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../99-completed")
if(EXISTS "${COMPLETED_DIR}/15-naive-matmul.cpp"
   AND EXISTS "${COMPLETED_DIR}/15-naive-matmul.h"
   AND EXISTS "${COMPLETED_DIR}/15-naive-matmul.comp")
    set(COMPLETED_SHADER "${CMAKE_CURRENT_BINARY_DIR}/completed/naive-matmul.comp.spv")
    add_custom_command(
        OUTPUT "${COMPLETED_SHADER}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/completed"
        COMMAND Vulkan::glslc --target-env=vulkan1.3 --target-spv=spv1.3
                "${COMPLETED_DIR}/15-naive-matmul.comp" -o "${COMPLETED_SHADER}"
        DEPENDS "${COMPLETED_DIR}/15-naive-matmul.comp"
        VERBATIM
    )
    add_custom_target(naive_matmul_completed_shader DEPENDS "${COMPLETED_SHADER}")
    add_executable(naive_matmul_completed EXCLUDE_FROM_ALL "${COMPLETED_DIR}/15-naive-matmul.cpp")
    target_link_libraries(naive_matmul_completed PRIVATE vAi)
    target_compile_definitions(naive_matmul_completed PRIVATE MATMUL_SHADER_PATH="${COMPLETED_SHADER}")
    add_dependencies(naive_matmul_completed naive_matmul_completed_shader)
    set_target_properties(naive_matmul_completed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/completed")
    if(MSVC)
        target_compile_options(naive_matmul_completed PRIVATE /utf-8)
    endif()
endif()

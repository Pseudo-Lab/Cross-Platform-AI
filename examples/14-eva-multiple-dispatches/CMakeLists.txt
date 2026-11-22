cmake_minimum_required(VERSION 3.24)
project(eva_multiple_dispatchesExample LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)

set(EVA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../external/eva")
if(NOT EXISTS "${EVA_SOURCE_DIR}/external/SPIRV-Reflect/CMakeLists.txt")
    message(FATAL_ERROR "Run git submodule update --init --recursive from the repository root.")
endif()
set(EVA_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(EVA_ENABLE_WINDOW OFF CACHE BOOL "" FORCE)
set(EVA_ENABLE_RAYTRACING OFF CACHE BOOL "" FORCE)
set(EVA_ENABLE_PERFORMANCE_QUERY OFF CACHE BOOL "" FORCE)
add_subdirectory("${EVA_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/eva" EXCLUDE_FROM_ALL)

set(SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../13-multiple-dispatches/add.comp")
set(SHADER_BINARY "${CMAKE_CURRENT_BINARY_DIR}/add.comp.spv")
add_custom_command(
    OUTPUT "${SHADER_BINARY}"
    COMMAND Vulkan::glslc --target-env=vulkan1.3 --target-spv=spv1.3
            "${SHADER_SOURCE}" -o "${SHADER_BINARY}"
    DEPENDS "${SHADER_SOURCE}"
    COMMENT "Compile add.comp"
    VERBATIM
)

add_executable(eva_multiple_dispatches_example main.cpp "${SHADER_BINARY}")
set_target_properties(eva_multiple_dispatches_example PROPERTIES OUTPUT_NAME main)
target_link_libraries(eva_multiple_dispatches_example PRIVATE eva)
target_compile_definitions(eva_multiple_dispatches_example PRIVATE ADD_SHADER_PATH="${SHADER_BINARY}")
if(MSVC)
    target_compile_options(eva_multiple_dispatches_example PRIVATE /utf-8)
    target_compile_options(eva PRIVATE /utf-8)
endif()

include(CTest)
if(BUILD_TESTING)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/validation/vk_layer_settings.txt" CONTENT
"khronos_validation.validate_sync = true
khronos_validation.syncval_shader_accesses_heuristic = true
khronos_validation.debug_action = VK_DBG_LAYER_ACTION_LOG_MSG
khronos_validation.log_filename = stdout
khronos_validation.report_flags = error,warn
")
    add_test(NAME eva_multiple_dispatches COMMAND eva_multiple_dispatches_example)
    set_tests_properties(eva_multiple_dispatches PROPERTIES
        ENVIRONMENT "VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation;VK_LAYER_SETTINGS_PATH=${CMAKE_CURRENT_BINARY_DIR}/validation"
        FAIL_REGULAR_EXPRESSION "Validation Error;SYNC-HAZARD;VUID-;EVA_ASSERT FAILED"
        TIMEOUT 30
    )
endif()

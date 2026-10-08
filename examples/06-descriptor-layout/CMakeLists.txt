cmake_minimum_required(VERSION 3.24)
project(DescriptorLayoutExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan 1.3 REQUIRED)

add_executable(descriptor_layout_example main.cpp)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../99-completed/06-descriptor-layout.cpp")
    add_executable(descriptor_layout_completed EXCLUDE_FROM_ALL ../99-completed/06-descriptor-layout.cpp)
    set_target_properties(descriptor_layout_completed PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/completed")
endif()

foreach(target descriptor_layout_example descriptor_layout_completed)
    if(NOT TARGET ${target})
        continue()
    endif()
    set_target_properties(${target} PROPERTIES OUTPUT_NAME main)
    target_link_libraries(${target} PRIVATE Vulkan::Vulkan ${CMAKE_DL_LIBS})
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
endforeach()

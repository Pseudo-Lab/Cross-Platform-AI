cmake_minimum_required(VERSION 3.24)
project(ValidationLayersExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan 1.3 REQUIRED)

add_executable(validation_layers main.cpp)
target_link_libraries(validation_layers PRIVATE Vulkan::Vulkan ${CMAKE_DL_LIBS})

if(MSVC)
    target_compile_options(validation_layers PRIVATE /utf-8)
endif()

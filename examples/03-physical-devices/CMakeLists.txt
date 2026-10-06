cmake_minimum_required(VERSION 3.24)
project(PhysicalDevicesExample LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan 1.3 REQUIRED)

add_executable(physical_devices main.cpp)
target_link_libraries(physical_devices PRIVATE Vulkan::Vulkan ${CMAKE_DL_LIBS})

if(MSVC)
    target_compile_options(physical_devices PRIVATE /utf-8)
endif()

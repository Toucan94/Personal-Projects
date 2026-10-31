cmake_minimum_required(VERSION 3.10)

project(Vulkan_Tutorial)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)

add_executable(Main src/main.cpp)

target_compile_definitions(Main PRIVATE VULKAN_HPP_NO_STRUCT_CONSTRUCTORS)

target_link_libraries(Main PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

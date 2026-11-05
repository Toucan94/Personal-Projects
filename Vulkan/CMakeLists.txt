cmake_minimum_required(VERSION 3.10)

project(Vulkan_Tutorial)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)

find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)
message(STATUS "slangc: ${SLANGC_EXECUTABLE}")

add_executable(Main src/main.cpp)

target_compile_definitions(Main PRIVATE
    VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders/"
)

function (add_slang_shader_target TARGET)
    cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})
    set (SHADERS_DIR ${CMAKE_CURRENT_LIST_DIR}/shaders)
    set (ENTRY_POINTS -entry vertMain -entry fragMain)
    add_custom_command(
            OUTPUT ${SHADERS_DIR}/slang.spv
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${SHADER_SOURCES}
            COMMENT "Compiling Slang Shaders"
            VERBATIM
    )
    add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
endfunction()

add_slang_shader_target(MainShaders SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.slang)
add_dependencies(Main MainShaders)

target_link_libraries(Main PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

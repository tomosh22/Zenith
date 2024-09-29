cmake_minimum_required(VERSION 3.16)
project(Zenith)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Define available platforms and configurations
if (WIN32)
    set(PLATFORM "win64")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

# Source directories and exclude specific files
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

list(FILTER SOURCES EXCLUDE REGEX ".*VulkanSDK.*")
list(FILTER SOURCES EXCLUDE REGEX ".*FluxCompiler.*")
list(FILTER SOURCES EXCLUDE REGEX ".*glm-master.*")
list(FILTER SOURCES EXCLUDE REGEX ".*entt-3.13.2.*")
list(FILTER SOURCES EXCLUDE REGEX ".*Tools.*")
list(FILTER SOURCES EXCLUDE REGEX ".*reactphysics3d-0.10.1/helloworld.*")
list(FILTER SOURCES EXCLUDE REGEX ".*reactphysics3d-0.10.1/test.*")
list(FILTER SOURCES EXCLUDE REGEX ".*reactphysics3d-0.10.1/testbed.*")
list(FILTER SOURCES EXCLUDE REGEX ".*cmake.*")

# Add executable or library (depending on the project type)
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Zenith
        ${CMAKE_CURRENT_SOURCE_DIR}/Zenith/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/glfw-3.4.bin.WIN64/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/VulkanSDK/1.3.280.0/Include
        ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/glm-master
        ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/entt-3.13.2/single_include
        ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/reactphysics3d-0.10.1/include
        ${CMAKE_CURRENT_SOURCE_DIR}/Games
        ${CMAKE_CURRENT_SOURCE_DIR}/Zenith/Vulkan
        ${CMAKE_CURRENT_SOURCE_DIR}/Zenith/Windows
)

# Preprocessor definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
        ZENITH_VULKAN
        ZENITH_WINDOWS
        ZENITH_DEBUG
        NOMINMAX
        GLM_ENABLE_EXPERIMENTAL
        SHADER_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/Zenith/Flux/Shaders/"
)

# Precompiled headers
target_precompile_headers(${PROJECT_NAME} PRIVATE "Zenith/Core/Zenith.h")

# Link libraries
target_link_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/VulkanSDK/1.3.280.0/Lib
        ${CMAKE_CURRENT_SOURCE_DIR}/Middleware/glfw-3.4.bin.WIN64/lib-vc2022
)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw3_mt vulkan-1)

# Add ZenithTools dependency if ToolsEnabled is set
if (ZENITH_TOOLS_ENABLED)
    add_subdirectory(ZenithTools)
    target_link_libraries(${PROJECT_NAME} PRIVATE ZenithTools)
endif()

# Project output
set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/cmakeoutput"
)


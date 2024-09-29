# Project settings
project(Zenith)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Source directories and exclude specific files
file(GLOB_RECURSE SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
# Add source file exclusion logic if necessary

# Add executable or library (depending on the project type)
add_library(${PROJECT_NAME} STATIC ${SOURCES})

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
    # Add other include paths as necessary
)

# Preprocessor definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE 
    ZENITH_VULKAN
    ZENITH_WINDOWS
    NOMINMAX
)

# Precompiled headers
target_precompile_headers(${PROJECT_NAME} PRIVATE "Zenith.h")

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE glfw3_mt vulkan-1)

# Add ZenithTools dependency if ToolsEnabled is set
if (ZENITH_TOOLS_ENABLED)
    add_subdirectory(ZenithTools)
    target_link_libraries(${PROJECT_NAME} PRIVATE ZenithTools)
endif()

# Project output
set_target_properties(${PROJECT_NAME} PROPERTIES 
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
